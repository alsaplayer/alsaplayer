/*  jack.cpp - JACK output driver
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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
#include <glib.h>
#include <jack/jack.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include "AlsaNode.h"
#include "output_plugin.h"
#include "alsaplayer_error.h"
#include "prefs.h"

typedef jack_default_audio_sample_t sample_t;

static jack_port_t *my_output_port1;
static jack_port_t *my_output_port2;
static jack_client_t *client = NULL;
static jack_nframes_t buffer_size;
static jack_nframes_t sample_rate;
static jack_nframes_t nr_fragments;
static jack_nframes_t latency = 0;

static char dest_port1[32];
static char dest_port2[32];

static int bufsize(jack_nframes_t nframes, void *arg);
static int srate(jack_nframes_t nframes, void *arg);
static int process (jack_nframes_t nframes, void *arg);
static int jack_prepare(void *arg);
static void jack_shutdown(void *arg);
static void jack_restarter(void *arg);

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

void jack_restarter(void *arg)
{
	alsaplayer_error("sleeping 2 second");
	sleep (2);

	if (client) {
		jack_client_close(client);
		client = NULL;
	}	
	if (jack_prepare(arg) < 0) {
		alsaplayer_error("failed reconnecting to jack...exitting");
		kill(0, SIGTERM);
	}       
}


void jack_shutdown (void *arg)
{
	pthread_t restarter;

	alsaplayer_error("trying to reconnect to jack (spawning thread)");
	pthread_create(&restarter, NULL, (void * (*)(void *))jack_restarter, arg);
	pthread_detach(restarter);
}


int jack_get_latency()
{
	return (latency << 2); // We need to return latency in bytes
}

int jack_prepare(void *arg)
{
	char str[32];

	if (strlen(dest_port1) && strlen(dest_port2)) {
		sprintf(str, "alsaplayer-%d", getpid());
		if ((client = jack_client_new(str)) == 0) {
			alsaplayer_error("jack server not running?");
			return -1;
		}
		jack_set_process_callback (client, (JackProcessCallback)process, arg);
		jack_set_buffer_size_callback (client, (JackProcessCallback)bufsize, arg);
		jack_set_sample_rate_callback (client, (JackProcessCallback)srate, arg);
		jack_on_shutdown (client, jack_shutdown, arg);

		my_output_port1 = jack_port_register (client, "out_1",
				JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);               
		my_output_port2 = jack_port_register (client, "out_2",
				JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);               

		if (jack_activate (client)) {
			alsaplayer_error("cannot activate client");
			return -1;
		}       
		//alsaplayer_error("connecting to jack ports: %s & %s", dest_port1, dest_port2);

		if (jack_connect (client, jack_port_name(my_output_port1), dest_port1)) {
			alsaplayer_error("cannot connect output port 1");
			return -1;
		}               
		if (jack_connect (client, jack_port_name(my_output_port2), dest_port2)) {
			alsaplayer_error("cannot connect output port 2");
			return -1;
		}               
		return 0;
	}
	return -1;
}


int bufsize(jack_nframes_t nframes, void *arg)
{
	//printf("the maximum buffer size is now %lu\n", nframes);
	buffer_size = nframes;
	nr_fragments = 3;
	return 0;
}


int srate(jack_nframes_t rate, void *arg)
{
	sample_rate = rate;
	return 0;
}



static int jack_init()
{
	// Always return ok for now
	strcpy(dest_port1, prefs_get_string(ap_prefs,
		"jack", "output1", "alsa_pcm:out_1"));
	strcpy(dest_port2, prefs_get_string(ap_prefs,
		"jack", "output2", "alsa_pcm:out_2"));
	return 1;
}

static int jack_open(char *name)
{
	int err;
	char *s;
	if ((s=strchr(name, ','))) {
		*s++ = 0;
		strncpy(dest_port1, name, 31);
		strncpy(dest_port2, s, 31);
		dest_port1[31] = dest_port2[31] = 0;
		alsaplayer_error("Using: %s & %s",
				dest_port1, dest_port2);
	}	
	return 1;
}


static int jack_start_callbacks(void *data)
{
	if (jack_prepare(data) < 0) {
		return 0;
	}
	return 1;
}


static void jack_close()
{
	if (client) {
		jack_client_close (client);
		client = NULL;
	}	
	return;
}


static int jack_set_buffer(int fragment_size, int fragment_count, int channels)
{
	static int val;
	
	return 1;
}


static int jack_set_sample_rate(int rate)
{
	return rate;
}

int process(jack_nframes_t nframes, void *arg)
{
	subscriber *subs = (subscriber *)arg;
	char bufsize[16384];
	static bool realtime_set = 0;

	if (!realtime_set) {
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

	if (subs) {
		subscriber *i;
		int c;
		bool status;
		sample_t *out1 = (sample_t *) jack_port_get_buffer(my_output_port1, nframes);
		sample_t *out2 = (sample_t *) jack_port_get_buffer(my_output_port2, nframes);

		memset(bufsize, 0, sizeof(bufsize));

		latency = jack_port_get_total_latency(client, my_output_port1);

		for (c = 0; c < MAX_SUB; c++) {
			i = subs + c;
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


output_plugin jack_output;

#ifdef __cplusplus
extern "C" {
#endif

output_plugin *output_plugin_info(void)
{
	memset(&jack_output, 0, sizeof(output_plugin));
	jack_output.version = OUTPUT_PLUGIN_VERSION;
	jack_output.name = "JACK output v2.0";
	jack_output.author = "Andy Lo A Foe";
	jack_output.init = jack_init;
	jack_output.open = jack_open;
	jack_output.close = jack_close;
	jack_output.start_callbacks = jack_start_callbacks;
	jack_output.set_buffer = jack_set_buffer;
	jack_output.set_sample_rate = jack_set_sample_rate;
	jack_output.get_latency = jack_get_latency;
	return &jack_output;
}

#ifdef __cplusplus
}
#endif

