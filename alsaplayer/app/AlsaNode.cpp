/*  AlsaNode.cpp - AlsaNode virtual Node class
 *  Copyright (C) 1999 Andy Lo A Foe <andy@alsa-project.org>
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
	root_sub = NULL;
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
		subs[i].active = false;
		subs[i].streamer = NULL;
		subs[i].ID = -1;
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
	
	read_size = node->GetFragmentSize();
	while (node->looping) {
		subscriber *next = NULL;
		subscriber *i = node->root_sub;
		memset(buffer_data, 0, read_size); // Fresh buffer
		if (i == NULL) { // Quit immmediately
			node->plugin->write(buffer_data, read_size);
			read_size = node->GetFragmentSize(); // Change on the fly
			continue;
		}
		//printf("first = %d\n", i->ID);
		do {
			if (!i->active) { // Skip inactive streamers
				i = i->next;
				continue;
			}	
			//printf(".%d.", i->ID);
			status = i->streamer(i->arg, buffer_data, read_size);
			next = i->next; 
			if (status == false) { // Disable this streamer
				i->active = false;
			}
			i = next;
			//printf("next = %d\n", i ? i->ID : 0);
		} while (i != NULL); // Till the last buffer
		//printf("\n");
		node->plugin->write(buffer_data, read_size);

		read_size = node->GetFragmentSize(); // Change on the fly
		if (usleep_hack) {
				int count = node->plugin->get_queue_count();
				if (count > 8192)
					dosleep(1000);
		}			
	}
	delete []buffer_data;
	pthread_mutex_unlock(&node->thread_mutex);
	pthread_exit(NULL);
}

// Note that preferred_pos is ignored for now. Once implemented
// you will be able to specify where the new Streamer should be
// inserted (BEGIN, END, RANDOM). We should probably also keep a 
// record of what the preferred stream of a node is (so RANDOM
// really isn't random)


bool AlsaNode::IsInStream(int the_id)
{
	subscriber *i;
	
	if (root_sub == NULL)
		return false;
	pthread_mutex_lock(&queue_mutex);	
	for (i = root_sub; i != NULL; i = i->next) {
		if (i->ID == the_id) {
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
	subscriber *new_sub = NULL;
	
	pthread_mutex_lock(&queue_mutex);
	if (root_sub == NULL) {
#ifdef DEBUG	
		printf("Adding first subscriber...\n");
#endif
		root_sub = new subscriber;
		if (!root_sub) {
			printf("WTF!!!\n");
			pthread_mutex_unlock(&queue_mutex);
			return false;
		}	
		root_sub->ID = follow_id++;	
		root_sub->prev = NULL;
		root_sub->next = NULL;
		root_sub->streamer = str;
		root_sub->arg = arg;
		root_sub->active = true; // Default is stream directly
		count++;
		pthread_mutex_unlock(&queue_mutex);
		return root_sub->ID;
	}
	new_sub = new subscriber; // Allocate new node
	if (!new_sub) {
		printf("WTF2!!!!!\n");
		pthread_mutex_unlock(&queue_mutex);
		return -1;
	}	
#ifdef DEBUG		
	printf("Adding new subscriber...\n");	
#endif
	new_sub->ID = follow_id++;
	new_sub->streamer = str; // Assign streamer
	new_sub->arg = arg; // Argument
	new_sub->prev = NULL; // Become the new root
	new_sub->next = root_sub; // Set up next pointer
	new_sub->active = true;
	root_sub->prev = new_sub;
	root_sub = new_sub;
#ifdef DEBUG		
	printf("Sucess..\n");
#endif
	count++;
	pthread_mutex_unlock(&queue_mutex);
	return new_sub->ID;
}


bool AlsaNode::RemoveStreamer(int the_id)
{
	subscriber *i, *tmp;
	if (the_id < 0) {
		printf("request for negative ID removal\n");
		return true;
	}	
	pthread_mutex_lock(&queue_mutex);
	for (i = root_sub; i != NULL; i = i->next) {
		if (!i)
			break;
		if (i->ID == the_id) {
			i->active = false;
			if (i == root_sub) { // root 
				if (i->next == NULL) { // last one
					//printf("Removing last streamer\n");
					delete root_sub;
					root_sub = NULL;
					count--;
					pthread_mutex_unlock(&queue_mutex);
					if (IsInStream(the_id)) 
						printf("AAAAARGH1!!!\n");	
					return true;
				}
#ifdef DEBUG				
				printf("Removing root streamer...\n");
#endif
				tmp = root_sub;
				root_sub = root_sub->next;
				root_sub->prev = NULL;
				delete tmp;
				count--;
				pthread_mutex_unlock(&queue_mutex);
				if (IsInStream(the_id))
					printf("AAAAAARGH2!!!!\n");
				return true;
			} else {
				if (i->next == NULL) { // Last one in loop
#ifdef DEBUG
					printf("Removing end streamer...\n");
#endif
					tmp = i;	
					i = i->prev;
					i->next = NULL;
					tmp->active = false;
					tmp->next = NULL;
					tmp->prev = NULL;
					tmp->ID = -1;
					//delete tmp; // WAAH!
					count--;
					pthread_mutex_unlock(&queue_mutex);
					if (IsInStream(the_id))
						printf("AAAAAAARGH3!!!\n");
					return true;
				} else {	
#ifdef DEBUG
					printf("Removing mid streamer...\n");
#endif
					i->prev->next = i->next;
					i->next->prev = i->prev;
					//delete i; // WAAH!
				}
			}
			count--;
			pthread_mutex_unlock(&queue_mutex);
			if (IsInStream(the_id)) 
				printf("AAAAAARGH4!!!\n");
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
	if (looper_thread) {
		looping = false;
		pthread_join(looper_thread, NULL); // Wait for thread
	}
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

