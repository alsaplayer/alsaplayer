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

#include "AlsaPlayer.h"
#include "config.h"
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
#include "error.h"
#ifdef USE_JACK
#include <jack/jack.h>
#endif


#define MAX_FRAGS	32
#define LOW_FRAGS	1	

extern void exit_sighandler(int);

void jack_shutdown (void *arg)
{
	char *name = (char *)arg;
	if (name)
		fprintf(stderr, "%s exitting...check jackd for details\n", name);
	kill(0, SIGTERM);
}



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
#ifdef USE_JACK	
	char client_name[32];
	char dest_port1[32], dest_port2[32];
	use_jack = 0;
#endif
	realtime_sched = realtime;
	sample_freq = OUTPUT_RATE;

	for (int i = 0; i < MAX_SUB; i++) {
		memset(&subs[i], 0, sizeof(subscriber));
	}	
#ifdef USE_JACK
	if (global_session_name)
		sprintf(client_name, "%s", global_session_name);
	else	
		sprintf(client_name, "alsaplayer-%d", getpid());
	if (strncmp(name, "jack", 4) == 0) { // Use JACK
		if ((client = jack_client_new(client_name)) == 0) {
			alsaplayer_error("jack server not running?");
			return;
		}
		jack_set_process_callback (client, 
			(JackProcessCallback)process, this);
		jack_set_buffer_size_callback (client, 
			(JackProcessCallback)bufsize, this);
		jack_set_sample_rate_callback (client, 
			(JackProcessCallback)srate, this);
		//printf("engine sample rate: %lu\n", jack_get_sample_rate (client));
		jack_on_shutdown (client, jack_shutdown, strdup(client_name));

		my_output_port1 = jack_port_register (client, "out_1",
				JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);		
		my_output_port2 = jack_port_register (client, "out_2",
				JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);		

		if (jack_activate (client)) {
			alsaplayer_error("cannot activate client");
		}	
		//printf("client activated\n");
	
		strcpy(dest_port1, "alsa_pcm:out_1");
		strcpy(dest_port2, "alsa_pcm:out_2");
		if (sscanf(name, "jack %31s %31s", dest_port1, dest_port2) == 1) {
		  strcpy(dest_port2, dest_port1);
		}

		printf("connecting to jack ports: %s & %s\n", dest_port1, dest_port2);

		if (jack_connect (client, jack_port_name(my_output_port1),
				dest_port1)) {
				alsaplayer_error("cannot connect output port 1");
		}		
		if (jack_connect (client, jack_port_name(my_output_port2),
				dest_port2)) {
				alsaplayer_error("cannot connect output port 2");
		}		
	
		use_jack = 1;
		init = 1;
	}
#endif
	pthread_mutex_init(&thread_mutex, NULL);
	pthread_mutex_init(&queue_mutex, NULL);
}


AlsaNode::~AlsaNode()
{
	StopStreaming();
#ifdef USE_JACK
	if (use_jack) {
		if (client)
			jack_client_close (client);
		return;
	}	
#endif
	assert(plugin);
	plugin->close();
}


int AlsaNode::SetSamplingRate(int freq)
{
#ifdef USE_JACK
	if (use_jack) {
		//alsaplayer_error("Not setting samplerate for JACK");	
		return 1;
	}
#endif
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
#ifdef USE_JACK
	if (use_jack) {
		//alsaplayer_error("Not setting buffersize for JACK");
		return 1;
	}
#endif
	assert(plugin);

	if (plugin->set_buffer(frag_size, count, channels)) {
		nr_fragments = count;
		fragment_size = frag_size;
		return 1;
	} else {
		return 0;
	}	
}

#ifdef USE_JACK
int AlsaNode::bufsize(nframes_t nframes, void *arg)
{
	AlsaNode *node = (AlsaNode *)arg;

	//printf("the maximum buffer size is now %lu\n", nframes);
	if (node) {
		node->fragment_size = nframes;
		node->nr_fragments = 3;
	}	
	return 0;
}


int AlsaNode::srate(nframes_t rate, void *arg)
{
	AlsaNode *node = (AlsaNode *)arg;

	//printf ("the sample rate is now %lu/sec\n", rate);
	if (node)
		node->sample_freq = (int)rate;
	return 0;
}

#define SAMPLE_MAX_16BIT  32767.0f

void sample_move_dS_s16 (sample_t *dst, char *src,
 			unsigned long nsamples, unsigned long src_skip) 
{
        /* ALERT: signed sign-extension portability !!! */
        while (nsamples--) {
                *dst = (*((short *) src)) / SAMPLE_MAX_16BIT;
                dst++;
                src += src_skip;
        }
}      

//#define STATS  /* Only meaningful if fragments per interrupt is 64 */

int AlsaNode::process(nframes_t nframes, void *arg)
{
	AlsaNode *node = (AlsaNode *)arg;
	char bufsize[16384];
	static bool realtime_set = 0;

#ifdef USE_REALTIME
	if (node->realtime_sched && !realtime_set) {
		struct sched_param sp;
		memset(&sp, 0, sizeof(sp));
		sp.sched_priority = sched_get_priority_max(SCHED_FIFO);

		//alsaplayer_error("THREAD-%d=soundcard thread\n", getpid());

		if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
				alsaplayer_error("failed to setup realtime scheduling!");
		} else {
			mlockall(MCL_CURRENT);
			printf("realtime scheduling active\n");
		}
		realtime_set = 1;
	}	
#endif

	if (node) {
		subscriber *i;
		int c;
		bool status;
		sample_t *out1 = (sample_t *) jack_port_get_buffer(node->my_output_port1, nframes);
		sample_t *out2 = (sample_t *) jack_port_get_buffer(node->my_output_port2, nframes);
		
		memset(bufsize, 0, sizeof(bufsize));	

		for (c = 0; c < MAX_SUB; c++) {
			i = &node->subs[c];
			if (!i->active || !i->streamer) { // Skip inactive streamers
				continue;
			}	
			i->active = i->streamer(i->arg, bufsize, nframes * 2);
		}
		sample_move_dS_s16(out1, bufsize, nframes, sizeof(short) << 1);
		sample_move_dS_s16(out2, bufsize + sizeof(short), nframes, sizeof(short) << 1); 
	}	

	return 0;
}

#endif

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
			printf("real-time scheduling on\n");
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
		alsaplayer_error("Wrong version on plugin v%d, wanted v%d",
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
		alsaplayer_error("Closing already opened plugin?!");
		plugin->close();
	}
	/* Remember this plugin - it's the one we're going to use. */
	plugin = tmp;
	if (!tmp->init() || !tmp->open(use_pcm)) {
		plugin = NULL;
       	return 0; // Unclean but good enough for now
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
#ifdef USE_JACK
	if (use_jack) {
		return;
	}	
#endif	
	pthread_create(&looper_thread, NULL, (void * (*)(void *))looper, this);
}


void AlsaNode::StopStreaming()
{
#ifdef USE_JACK
	if (use_jack) {
		return;
	}	
#endif
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
#ifdef USE_JACK
	if (use_jack) // Should jack have a hardware buffer length query?
			return (fragment_size * nr_fragments);
#endif
	assert(plugin);
	if (plugin->get_queue_count && (count = plugin->get_queue_count()) >= 0) {
		return count;
	} else if (plugin->get_latency && (count = plugin->get_latency()) >= 0) {
		return count;
	} else  
		return (external_latency + ((nr_fragments-1) * fragment_size));
}

