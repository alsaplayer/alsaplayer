/*  AlsaNode.cpp - AlsaNode virtual Node class
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 *  $Id$
 *
*/ 

#include "AlsaPlayer.h"
#include "config.h"
#include "prefs.h"
#include <sys/mman.h>
#include <sys/types.h>
#ifdef USE_REALTIME
#include <sched.h>
#endif
#include <dlfcn.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include "AlsaNode.h"
#include "utilities.h"
#include "alsaplayer_error.h"

#define MAX_FRAGS	32
#define LOW_FRAGS	1	

extern void exit_sighandler(int);

AlsaNode::AlsaNode(char *name, int realtime)
{
	follow_id = 1;
	count = 0;
	plugin_count = 0;
	plugin = NULL;
	nr_fragments = fragment_size = external_latency = 0;	
	init = false;
	use_pcm = name;
	realtime_sched = realtime;
	sample_freq = OUTPUT_RATE;

	for (int i = 0; i < MAX_SUB; i++) {
		memset(&subs[i], 0, sizeof(subscriber));
	}
	if (global_session_name)
		sprintf(client_name, "%s", global_session_name);
	else	
		sprintf(client_name, "alsaplayer-%d", getpid());
	
	pthread_mutex_init(&thread_mutex, NULL);
	pthread_mutex_init(&queue_mutex, NULL);
}


AlsaNode::~AlsaNode()
{
	StopStreaming();
	assert(plugin);
	plugin->close();
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


int AlsaNode::SetStreamBuffers(int frag_size, int count, int channels)
{
	assert(plugin);

	if (plugin->set_buffer(frag_size, count, channels)) {
		nr_fragments = count;
		fragment_size = frag_size;
		return 1;
	} else {
		return 0;
	}	
}


void AlsaNode::looper(void *pointer)
{
	char *buffer_data;
	AlsaNode *node = (AlsaNode *)pointer;
	int read_size = node->GetFragmentSize();
	bool usleep_hack = true;
	bool status;
	int c;

	signal(SIGINT, exit_sighandler);

	assert(node->plugin);
#ifdef DEBUG
	alsaplayer_error("THREAD-%d=soundcard thread\n", getpid());
#endif
	if (pthread_mutex_trylock(&node->thread_mutex) != 0) {
		pthread_exit(NULL);
		return;
	}	
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
	usleep_hack = (node->plugin->get_queue_count != NULL);

	node->looping = true;

	buffer_data = new char[16384];

	if (!buffer_data) {
		alsaplayer_error("HUH?? error!");
		exit(1);
	}	

	memset(buffer_data, 0, 16384);

	read_size = node->GetFragmentSize();
	while (node->looping) {
		subscriber *i;
		int c;

		memset(buffer_data, 0, 16384);
		for (c = 0; c < MAX_SUB; c++) {
			i = &node->subs[c];
			if (!i->active) { // Skip inactive streamers
				continue;
			}	
			if (i->streamer) {
				//printf("streaing %d\n", i->ID);
				status = i->streamer(i->arg, buffer_data, read_size / sizeof(short));
			} else { 
				continue;
			}	
			if (status == false) { // Disable this streamer
				i->active = false;
			}
		}
		node->plugin->write(buffer_data, read_size);

		read_size = node->GetFragmentSize(); // Change on the fly
		if (usleep_hack) {
			int count = node->plugin->get_queue_count();
			if (count > 8192)
				dosleep(1000);
		}			
	}
	delete buffer_data;
	pthread_mutex_unlock(&node->thread_mutex);
	pthread_exit(NULL);
}

bool AlsaNode::IsInStream(int the_id)
{
	subscriber *i;
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

	if (plugin_count == MAX_PLUGIN) {
		alsaplayer_error("Maximum number of plugins reached (%d)", MAX_PLUGIN);
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
	if (!tmp->init() || !tmp->open(use_pcm)) {
		plugin = NULL;
		return 0; // Unclean but good enough for now
	}

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
		fprintf(stdout, "Output plugin: %s\n", tmp->name);
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
	pthread_create(&looper_thread, NULL, (void * (*)(void *))looper, this);
	pthread_detach(looper_thread);
}


void AlsaNode::StopStreaming()
{
	assert(plugin);

	if (plugin->start_callbacks) {
		return;
	}	
	
	looping = false;
	pthread_cancel(looper_thread);
	//pthread_join(looper_thread, NULL); // Wait for thread
}


bool AlsaNode::ReadyToRun()
{
	return init;
}


int AlsaNode::GetLatency()
{
	int count = 0;
	
	assert(plugin);
	//if (plugin->start_callbacks) {
	//	return (fragment_size * nr_fragments);
	//}	
	
	if (plugin->get_queue_count && (count = plugin->get_queue_count()) >= 0) {
		return count;
	} else if (plugin->get_latency && (count = plugin->get_latency()) >= 0) {
		return count;
	} else  
		return (external_latency + ((nr_fragments-1) * fragment_size));
}

