/*  AlsaNode.cpp - AlsaNode virtual Node class
 *  Copyright (C) 1999-2001 Andy Lo A Foe <andy@alsa-project.org>
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

#include "config.h"
#include "AlsaNode.h"
#include "utilities.h"
#include <sys/mman.h>
#ifdef USE_REALTIME
#include <sched.h>
#endif
#include <dlfcn.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>

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
	sock = -1;
	use_pcm = name;
	realtime_sched = realtime;
	sample_freq = OUTPUT_RATE;

	for (int i = 0; i < MAX_SUB; i++) {
		memset(&subs[i], 0, sizeof(subscriber));
	}	
	
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
	assert(plugin);

	if (plugin->set_sample_rate(freq)) {
		sample_freq = freq;
		return 1;
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
	printf("THREAD-%d=soundcard thread\n", getpid());
#endif
	if (pthread_mutex_trylock(&node->thread_mutex) != 0) {
		//printf("Node is already looping()\n");
		return;
	}	
#ifdef USE_REALTIME
	if (node->realtime_sched) {
		struct sched_param sp;
		memset(&sp, 0, sizeof(sp));
		sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	
		if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
			//printf("normal scheduling on\n");
			if (node->fragment_size < 1024) {
				printf("***NOTE***: please consider scheduling real-time!\n");
			}
		} else {
			mlockall(MCL_CURRENT);
			printf("real-time scheduling on\n");
		}
	}	
#endif
	usleep_hack = (node->plugin->get_queue_count != NULL);
	
	node->looping = true;

	buffer_data = new char[16384];

	if (!buffer_data) {
		printf("HUH?? error!\n");
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
				status = i->streamer(i->arg, buffer_data, read_size);
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
		fprintf(stderr, "Maximum number of plugins reached (%d)\n", MAX_PLUGIN);
		return 0;
	}	
	output_plugin *tmp = &plugins[plugin_count];
	tmp->version = the_plugin->version;
	if ((version = tmp->version) != OUTPUT_PLUGIN_VERSION) {
		fprintf(stderr, "Wrong version on plugin v%d, wanted v%d\n",
			version - 0x1000, OUTPUT_PLUGIN_VERSION - 0x1000);
		return 0;
	}	
	strncpy(tmp->name, the_plugin->name, 256);
	strncpy(tmp->author, the_plugin->author, 256);
	tmp->init = the_plugin->init;
	tmp->open = the_plugin->open;
	tmp->close = the_plugin->close;
	tmp->write = the_plugin->write;
	tmp->set_buffer = the_plugin->set_buffer;
	tmp->set_sample_rate = the_plugin->set_sample_rate;
	tmp->get_queue_count = the_plugin->get_queue_count;
	tmp->get_latency = the_plugin->get_latency;

	/* No longer ready to play. */
	init = false;

	/* If we already have a plugin, close the old one. */
	if (plugin)  {
		printf("Closing already open plugin?!\n");
		plugin->close();
	}
	/* Remember this plugin - it's the one we're going to use. */
	plugin = tmp;
	if (!tmp->init() || !tmp->open(use_pcm)) {
		plugin = NULL;
       	return 0; // Unclean but good enough for now
    	}
	init = true;
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
	printf("Oversubscribed....!\n");
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
	printf("Streamer not found\n");
	pthread_mutex_unlock(&queue_mutex);
	return false;
}


void AlsaNode::StartStreaming()
{
	pthread_create(&looper_thread, NULL, (void * (*)(void *))looper, this);
}


void AlsaNode::StopStreaming()
{
	looping = false;
	pthread_join(looper_thread, NULL); // Wait for thread
}


bool AlsaNode::ReadyToRun()
{
	return init;
}


int AlsaNode::GetLatency()
{
	int count = 0;

	assert(plugin);
	if (plugin->get_queue_count && (count = plugin->get_queue_count()) >= 0) {
		return count;
	} else if (plugin->get_latency && (count = plugin->get_latency()) >= 0) {
		//printf("requesting latency with get_latency()\n");
		return count;
	} else  // This is obsolete, not to mention incorrect
		return (external_latency + ((nr_fragments-1) * fragment_size));
}

