/*  AlsaNode.cpp - AlsaNode virtual Node class
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@loafoe.com>
 *
 *  This file is part of AlsaPlayer.
 *
 *  AlsaPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AlsaPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
*/

#include "AlsaPlayer.h"
#include "config.h"
#include "prefs.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef USE_REALTIME
#include <sched.h>
#endif
#include <dlfcn.h>
#include <cassert>
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include "AlsaNode.h"
#include "utilities.h"
#include "alsaplayer_error.h"
#include "ap_string.h"

static const char *default_output_addons[] = {
	"alsa",
	"jack",
	"oss",
	"nas",
	"sparc",
	"sgi",
	"esound",
	"null",
	NULL
};



extern void exit_sighandler(int);

AlsaNode::AlsaNode(const char *name, const char *args, int realtime)
{
	follow_id = 1;
	count = 0;
	plugin_count = 0;
	plugin = NULL;
	nr_fragments = fragment_size = external_latency = 0;
	init = false;
	thread_running = false;
	realtime_sched = realtime;
	sample_freq = OUTPUT_RATE;

	// Parse driver name,args
	if (name && *name) {
		ap_strlcpy(driver_name, name, sizeof (driver_name));
	}
	if (args && *args) {
		ap_strlcpy(driver_args, args, sizeof (driver_args));
	}


	for (int i = 0; i < MAX_SUB; i++) {
		memset(&subs[i], 0, sizeof(subscriber));
	}
	if (global_session_name)
		snprintf(client_name, sizeof (client_name), "%s", global_session_name);
	else
		snprintf(client_name, sizeof (client_name), "alsaplayer-%d", getpid());

	pthread_mutex_init(&thread_mutex, NULL);
	pthread_mutex_init(&queue_mutex, NULL);
}


AlsaNode::~AlsaNode()
{
	StopStreaming();
	assert(plugin);
	plugin->close();

	if (driver_name) {
		driver_name [0] = 0;
	}
	if (driver_args) {
		driver_args [0] = 0;
	}
}


int AlsaNode::RegisterPlugin(const char *module)
{
	char path[1024];
	const char *pluginroot;
	void *our_handle;
	struct stat statbuf;
	output_plugin_info_type output_plugin_info;

	if (!global_pluginroot)
		 pluginroot = ADDON_DIR;
	else
		pluginroot = global_pluginroot;

	if (module) {
		snprintf(path, sizeof(path), "%s/output/lib%s_out.so", pluginroot, module);
		if (stat(path, &statbuf) != 0) {
			return false;
		}
		if ((our_handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))) {
			output_plugin_info =
				(output_plugin_info_type) dlsym(our_handle,
								"output_plugin_info");
			if (output_plugin_info) {
				if (RegisterPlugin(output_plugin_info())) {
					return true;
				}
				dlclose(our_handle);
				alsaplayer_error("Failed to register plugin: %s", path);
				return false;
			} else {
				alsaplayer_error("Symbol error in shared object: %s", path);
				dlclose(our_handle);
				return false;
			}
		} else {
			alsaplayer_error("%s\n", dlerror());
		}
	} else {
		for (int i = 0; default_output_addons[i]; i++) {
			snprintf(path, sizeof(path)-1, "%s/output/lib%s_out.so", pluginroot,
					default_output_addons[i]);
			if (stat(path, &statbuf) != 0)
				continue;
			if ((our_handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))) {
				output_plugin_info =
					(output_plugin_info_type) dlsym(our_handle, "output_plugin_info");
				if (output_plugin_info) {
#ifdef DEBUG
					alsaplayer_error("Loading output plugin: %s", path);
#endif
					if (RegisterPlugin(output_plugin_info())) {
						if (!ReadyToRun()) {
							alsaplayer_error("%s failed to init", path);
							continue;       // This is not clean
						}
						return true;    // Return as soon as we load one successfully!
					} else {
						alsaplayer_error("%s failed to load", path);
					}
				} else {
					alsaplayer_error("could not find symbol in shared object");
				}
				dlclose(our_handle);
			} else {
				alsaplayer_error("%s\n", dlerror());
			}
		}
	}
	// If we arrive here it means we haven't found any suitable output-addons
	alsaplayer_error
		("\nI could not find a suitable output module on your\n"
		 "system. Make sure they're in \"%s/output/\".\n"
		 "Use the -o parameter to select one.\n", pluginroot);
	return false;


}


int AlsaNode::SetSamplingRate(int freq)
{
	int actual_rate;

	assert(plugin);

	if ((actual_rate = plugin->set_sample_rate(freq))) {
		sample_freq = actual_rate;
		return actual_rate;
	} else {
		return 0;
	}
}


int AlsaNode::SetStreamBuffers(int frag_size, int frag_count, int channels)
{
	assert(plugin);
	int size;
	int counter;
	int chans;

	size = frag_size;
	counter = frag_count;
	chans = channels;

	if (plugin->set_buffer(&size, &counter, &chans)) {
		nr_fragments = counter;
		fragment_size = size;
		return 1;
	} else {
		return 0;
	}
}

#define MAX_WRITE	(64*1024)

void AlsaNode::looper(void *pointer)
{
	short *buffer_data = NULL;
	AlsaNode *node = (AlsaNode *)pointer;
	int read_size = node->GetFragmentSize();
	bool status;

	signal(SIGINT, exit_sighandler);

	assert(node->plugin);

	buffer_data = new short [MAX_WRITE+1];
	if (!buffer_data) {
		alsaplayer_error("Error allocating mix buffer");
		return;
	}
	memset(buffer_data, 0, (MAX_WRITE+1) * sizeof (buffer_data [0]));
#ifdef DEBUG
	alsaplayer_error("THREAD-%d=soundcard thread\n", getpid());
#endif
#ifdef USE_REALTIME

	if (node->realtime_sched) {
		struct sched_param sp;
		memset(&sp, 0, sizeof(sp));
		sp.sched_priority = sched_get_priority_max(SCHED_FIFO);

		if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
			if (node->fragment_size < 1024) {
				alsaplayer_error("***NOTE***: please consider scheduling real-time!");
			}
		} else {
			mlockall(MCL_CURRENT);
			if (global_verbose)
				alsaplayer_error("real-time scheduling on");
		}
	}
#endif
	node->looping = true;

	read_size = node->GetFragmentSize();
	if (read_size > MAX_WRITE) {
		alsaplayer_error("Fragment size too large. Exiting looper");
		pthread_exit(NULL);
	}
	while (node->looping) {
		subscriber *i;
		int c;

		memset(buffer_data, 0, read_size * sizeof(short));
		for (c = 0; c < MAX_SUB; c++) {
			i = &node->subs[c];

			// Skip inactive streamers
			if (!i->active || !i->streamer) {
				continue;
			}

			//printf("streaming %d\n", i->ID);
			status = i->streamer(i->arg, buffer_data, read_size / sizeof(short));

			if (status == false) { // Disable this streamer
				i->active = false;
			}
		}
		node->plugin->write(buffer_data, read_size);

		read_size = node->GetFragmentSize(); // Change on the fly
	}
	delete []buffer_data;
	//alsaplayer_error("Exiting looper thread...");
	pthread_exit(NULL);
}

bool AlsaNode::IsInStream(int the_id)
{
	int c;

	pthread_mutex_lock(&queue_mutex);
	for (c = 0; c < MAX_SUB; c++) {
		if (subs[c].ID == the_id) {
			pthread_mutex_unlock(&queue_mutex);
			return true;
		}
	}
	pthread_mutex_unlock(&queue_mutex);
	return false;
}


int AlsaNode::RegisterPlugin(output_plugin *the_plugin)
{
	int version;

	if (plugin_count == MAX_OUTPUT_PLUGINS) {
		alsaplayer_error("Maximum number of output plugins reached (%d)", MAX_OUTPUT_PLUGINS);
		return 0;
	}
	output_plugin *tmp = &plugins[plugin_count];
	tmp->version = the_plugin->version;
	if ((version = tmp->version) != OUTPUT_PLUGIN_VERSION) {
		alsaplayer_error("Wrong version on plugin (v%d, wanted v%d)",
				version - 0x1000, OUTPUT_PLUGIN_VERSION - 0x1000);
		return 0;
	}
	tmp->name = the_plugin->name;
	tmp->author = the_plugin->author;
	tmp->init = the_plugin->init;
	tmp->open = the_plugin->open;
	tmp->close = the_plugin->close;
	tmp->write = the_plugin->write;
	tmp->start_callbacks = the_plugin->start_callbacks;
	tmp->set_buffer = the_plugin->set_buffer;
	tmp->set_sample_rate = the_plugin->set_sample_rate;
	tmp->get_queue_count = the_plugin->get_queue_count;
	tmp->get_latency = the_plugin->get_latency;

	/* No longer ready to play. */
	init = false;

	/* If we already have a plugin, close the old one. */
	if (plugin)  {
		//alsaplayer_error("Closing already opened plugin?!");
		plugin->close();
	}

	/* Remember this plugin - it's the one we're going to use. */
	plugin = tmp;
	if (!plugin->init() || !plugin->open(driver_args)) {
		alsaplayer_error("Failed to initialize plugin!");
		plugin = NULL;
		return 0; // Unclean but good enough for now
	}
#if 0
	/* Schedule realtime from this point on */
	if (realtime_sched) {
		struct sched_param sp;
		memset(&sp, 0, sizeof(sp));
		sp.sched_priority = sched_get_priority_max(SCHED_FIFO);

		if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
			alsaplayer_error("failed to set up real-time scheduling!");
		} else {
			mlockall(MCL_CURRENT);
			alsaplayer_error("real-time scheduling on");
		}
	}
#endif
	/* If this is a callback based plugin, immediately start */
	if (plugin->start_callbacks) {
		if (!plugin->start_callbacks(subs)) {
			plugin->close();
			plugin = NULL;
			return 0;
		}
	}

	init = true;
	if (global_verbose)
		fprintf(stdout, "Output plugin: %s\n", plugin->name);
	return 1;
}


int AlsaNode::AddStreamer(streamer_type str, void *arg, int preferred_pos)
{
	int c;
	subscriber *i;
	pthread_mutex_lock(&queue_mutex);

	switch (preferred_pos) {
		case POS_BEGIN:
		case POS_MIDDLE:
			for (c = 0; c < MAX_SUB; c++) {
				i = &subs[c];
				if (!i->active) { // found an empty slot
					i->ID = follow_id++;
					i->streamer = str;
					i->arg = arg;
					i->active = true; // Default is stream directly
					count++;
					pthread_mutex_unlock(&queue_mutex);
					return i->ID;
				}
			}
			break;
		case POS_END:
			for (c = MAX_SUB-1; c >= 0; c--) {
				i = &subs[c];
				if (!i->active) { // found an empty slot
					i->ID = follow_id++;
					i->streamer = str;
					i->arg = arg;
					i->active = true; // Default is stream directly
					count++;
					pthread_mutex_unlock(&queue_mutex);
					return i->ID;
				}
			}
			break;
		default:
			break;
	}
	alsaplayer_error("oversubscribed....!");
	pthread_mutex_unlock(&queue_mutex);
	return -1;
}


bool AlsaNode::RemoveStreamer(int the_id)
{
	int c;
	subscriber *i;

	pthread_mutex_lock(&queue_mutex);
	for (c = 0; c < MAX_SUB; c++) {
		i = &subs[c];
		if (i->ID == the_id) {
			i->active = false;
			pthread_mutex_unlock(&queue_mutex);
			return true;
		}
	}
	alsaplayer_error("Streamer not found");
	pthread_mutex_unlock(&queue_mutex);
	return false;
}


void AlsaNode::StartStreaming()
{

	assert(plugin);

	if (plugin->start_callbacks)
		return;
	pthread_mutex_lock(&thread_mutex);
	if (thread_running) {
		//alsaplayer_error("Already looping...");
		pthread_mutex_unlock(&thread_mutex);
		return;
	}
	//alsaplayer_error("Creating looper thread");
	pthread_create(&looper_thread, NULL, (void * (*)(void *))looper, this);
	thread_running = true;
	pthread_mutex_unlock(&thread_mutex);
}


void AlsaNode::StopStreaming()
{
	assert(plugin);

	if (plugin->start_callbacks) {
		return;
	}

	looping = false;
	pthread_mutex_lock(&thread_mutex);
	if (thread_running) {
		if (pthread_join(looper_thread, NULL)) {
			// Hmmm
		}
		thread_running = false;
	}
	pthread_mutex_unlock(&thread_mutex);
}


bool AlsaNode::ReadyToRun()
{
	return init;
}


int AlsaNode::GetLatency()
{
	int counter = 0;

	assert(plugin);
	//if (plugin->start_callbacks) {
	//	return (fragment_size * nr_fragments);
	//}

	if (plugin->get_queue_count && (counter = plugin->get_queue_count()) >= 0) {
		return counter;
	} else if (plugin->get_latency && (counter = plugin->get_latency()) >= 0) {
		return counter;
	} else
		return (external_latency + ((nr_fragments-1) * fragment_size));
}

