/*  AlsaNode.h
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
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

#ifndef __AlsaNode_h__
#define __AlsaNode_h__

#ifdef __linux__
#define USE_REALTIME
#endif

#define OUTPUT_RATE 44100

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#ifdef USE_ESD
#include <esd.h>
#endif
#include "output_plugin.h"

#define MAX_SUB	16

typedef bool(*streamer_type)(void *arg, void *buf, int size);

typedef struct _subscriber
{
	int ID;
	streamer_type streamer;
	bool active;
	void *arg;
	_subscriber *prev;
	_subscriber *next;
} subscriber;

class AlsaNode
{
 private:
	output_plugin plugins[32];
	output_plugin *plugin;
	int plugin_count;
	subscriber subs[MAX_SUB]; 
	pthread_mutex_t queue_mutex; 
	pthread_mutex_t thread_mutex;
	int count;
	int follow_id; 
	int fragment_size;
	int nr_fragments;
	int sample_freq;
	int external_latency;
	char *use_pcm;
	bool realtime_sched;
	bool init;
	bool looping;
	static void looper(void *);
 public:
	subscriber *root_sub;
	int sock;
	void *sound_handle;
	pthread_t looper_thread;
	streamer_type streamer;	
	void *argument;

	AlsaNode(char *name, int realtime=0);
	~AlsaNode();
	int SetSamplingRate(int freq);
	int SamplingRate() { return sample_freq; }
	int SetStreamBuffers(int frag_size, int count, int channels);
	int RegisterPlugin(output_plugin *the_plugin);
	virtual int GetLatency();
	int GetFragmentSize() { return fragment_size; }
	void StartStreaming();
	void StopStreaming();	
	bool IsInStream(int);
	int AddStreamer(streamer_type str, void *arg, int);
	bool RemoveStreamer(int);
	bool ReadyToRun();
};


#endif
