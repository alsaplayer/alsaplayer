/*  esound.c - ESD output plugin
 *  Copyright (C) 1999 Andy Lo A Foe <andy@alsa-project.org>
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

#include "config.h"
#include <stdio.h>
#include <esd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include "output_plugin.h"
#include "AlsaPlayer.h"

static int esound_socket = -1;

static int esound_init(void)
{
	char *host = NULL;
	char *name = NULL;
	esd_format_t format = ESD_BITS16 | ESD_STEREO | ESD_STREAM | ESD_PLAY;
	int rate = 44100;

	if (global_session_name) {
		name = global_session_name; 
	}
#if 0            
	printf("ESD: loading libesd.so\n");
	void *handle = dlopen("libesd.so", RTLD_LAZY);
	if (handle == NULL) {
		printf("ESD: %s\n", dlerror());
		return 0;
	} else {
		int (*esd_play_stream_fallback)(esd_format_t,int,
                        const char *, const char *);
                        
                 esd_play_stream_fallback = dlsym(handle, "esd_play_stream_fallback");
                 eounsd_socket = (*esd_play_stream_fallback)(format, rate, host,
                                name);
                if (eounsd_socket < 0) {
					/* printf("ESD: could not open socket connection\n"); */
					dlclose(handle);
					return 0; 
                }
	}
#else
	esound_socket = esd_play_stream_fallback(format, rate, host, name);
	if (esound_socket < 0) {
		/* printf("ESD: could not open socket connection\n"); */
		return 0;
	}	
#endif
	return 1;
}

static int esound_open(const char *path)
{
	if (esound_socket >= 0) 
		return 1;
	return 0;
}


static void esound_close(void)
{
	return;
}


static int esound_write(void *data, int count)
{
	write(esound_socket, data, count);
	return 1;
}


static int esound_set_buffer(int *fragment_size, int *fragment_count, int *channels)
{
	printf("ESD: fragments fixed at 256/256, stereo\n");
	return 1;
}


static unsigned int esound_set_sample_rate(unsigned int rate)
{
	printf("ESD: rate fixed at 44100Hz\n");
	return rate;
}

static int esound_get_latency(void)
{
	return ((256*256));	// Hardcoded, but esd sucks ass
}

output_plugin esound_output;

output_plugin *output_plugin_info(void)
{
	memset(&esound_output, 0, sizeof(output_plugin));
	esound_output.version = OUTPUT_PLUGIN_VERSION;
	esound_output.name = "ESD output v1.0 (broken for mono output)";
	esound_output.author = "Andy Lo A Foe";
	esound_output.init = esound_init;
	esound_output.open = esound_open;
	esound_output.close = esound_close;
	esound_output.write = esound_write;
	esound_output.set_buffer = esound_set_buffer;
	esound_output.set_sample_rate = esound_set_sample_rate;
	esound_output.get_latency = esound_get_latency;
	return &esound_output;
}
