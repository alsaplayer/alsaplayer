/*  alsa.c - ALSA output plugin
 *  Copyright (C) 1999 by Andy Lo A Foe <andy@alsa-project.org>
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
#include <sys/asoundlib.h>
#include <stdlib.h>
#include <string.h>
#include "output_plugin.h"

//#define QUEUE_COUNT

static snd_pcm_t *sound_handle;
static snd_pcm_format_t format;
static snd_pcm_channel_info_t pinfo;
static int direction = SND_PCM_OPEN_PLAYBACK;
static int mode = SND_PCM_MODE_BLOCK;
static int channel = SND_PCM_CHANNEL_PLAYBACK;
static int frag_size = -1;
static int frag_count = -1;
static int output_rate = 44000;

static int alsa_init()
{
	// Always return ok for now
	sound_handle = NULL;
	return 1;
}


static int alsa_open(const char *name)
{
	int err;
	int card, device;

	if (!name || !*name) {
		name = "hw:0,0";
	}

	if (sscanf(name, "hw:%d,%d", &card, &device)!=2) {
		fprintf(stderr, "ALSA->open(): invalid device \"%s\", using 0,0\n",
				name);
		card = device = 0;
	}	
	if ((err = snd_pcm_open(&sound_handle, card, device, direction)) < 0) {
		fprintf(stderr, "ALSA->open(): %s\n", snd_strerror(err));
		return 0;
	}
	memset(&pinfo, 0, sizeof(pinfo));
	if ((err = snd_pcm_channel_info(sound_handle, &pinfo)) < 0) {
		fprintf(stderr, "ALSA->open(): %s\n", snd_strerror(err));
		return 0;
	}		
	return 1;
}


static void alsa_close()
{
	snd_pcm_close(sound_handle);
	return;
}

static int alsa_write(void *data, int count)
{
	snd_pcm_channel_status_t status;
		                
	snd_pcm_write(sound_handle, data, count);
	memset(&status, 0, sizeof(status));
	if (snd_pcm_channel_status(sound_handle, &status) < 0) {
		fprintf(stderr, "ALSA: could not get channel status\n");
		return 0;
	}       
	if (status.underrun) {
		fprintf(stderr, "ALSA: underrun. resetting channel\n");
		snd_pcm_channel_flush(sound_handle, channel);
		snd_pcm_playback_prepare(sound_handle);
		snd_pcm_write(sound_handle, data, count);
		if (snd_pcm_channel_status(sound_handle, &status) < 0) {
			fprintf(stderr, "ALSA: could not get channel status. giving up\n");
			return 0;
		}
		if (status.underrun) {
			fprintf(stderr, "ALSA: write error. giving up\n");
			return 0;
		}               
	}
	return 1;
}


static int alsa_set_buffer(int *fragment_size, int *fragment_count, int *channels)
{
	snd_pcm_channel_params_t params;
	snd_pcm_channel_setup_t setup;
	int err;
			
	memset(&params, 0, sizeof(params));

	params.mode = mode;
	params.channel = channel;
	params.start_mode = SND_PCM_START_FULL;
	params.stop_mode = SND_PCM_STOP_STOP;
	params.buf.block.frag_size = *fragment_size;
	params.buf.block.frags_max = *fragment_count;
	params.buf.block.frags_min = 1;

	memset(&format, 0, sizeof(format));
	format.format =  SND_PCM_SFMT_S16;
	format.rate = output_rate;
	format.voices = *channels;
	format.interleave = 1;
	memcpy(&params.format, &format, sizeof(format));
		
	snd_pcm_channel_flush(sound_handle, channel);
	
	if ((err = snd_pcm_channel_params(sound_handle, &params)) < 0) {
		fprintf(stderr, "ALSA->set_buffer(): parameter error \"%s\"\n",
			snd_strerror(err));
		return 0;
	}
	if ((err = snd_pcm_channel_prepare(sound_handle, channel)) < 0) {
		fprintf(stderr, "ALSA->set_buffer(): prepare error \"%s\"\n",
			snd_strerror(err));
		return 0;
	}
	memset(&setup, 0, sizeof(setup));
	setup.mode = mode;
	setup.channel = channel;
	if ((err = snd_pcm_channel_setup(sound_handle, &setup)) < 0) {
		fprintf(stderr, "ALSA->set_buffer(): setup error \"%s\"\n", 
			snd_strerror(err));
		return 0;
	}	
	frag_size = *fragment_size;
	frag_count = *fragment_count;

	return 1;
}


static int alsa_set_sample_rate(int rate)
{
	snd_pcm_channel_params_t params;
	int err;
	
	memset(&params, 0, sizeof(params));
	
	params.mode = mode;
	params.channel = channel;
	params.start_mode = SND_PCM_START_FULL;
	params.stop_mode = SND_PCM_STOP_STOP;
	memset(&format, 0, sizeof(format));
	format.format =  SND_PCM_SFMT_S16;
	format.rate = output_rate = rate;
	format.voices = 2;
	format.interleave = 1;
	memcpy(&params.format, &format, sizeof(format));
	
	if ((err = snd_pcm_channel_params(sound_handle, &params)) < 0) {
		fprintf(stderr, "ALSA->set_sample_rate(): parameter error \"%s\"\n", snd_strerror(err));
		return 0;
	}
	return output_rate;
}

#ifdef QUEUE_COUNT
static int alsa_get_queue_count()
{
	return 0;
}
#endif

static int alsa_get_latency()
{
	return (frag_size * frag_count);
}

output_plugin alsa_output;

output_plugin *output_plugin_info(void)
{
	memset(&alsa_output, 0, sizeof(output_plugin));
	alsa_output.version = OUTPUT_PLUGIN_VERSION;
	alsa_output.name = "ALSA output v1.5.10b";
	alsa_output.author = "Andy Lo A Foe";
	alsa_output.init = alsa_init;
	alsa_output.open = alsa_open;
	alsa_output.close = alsa_close;
	alsa_output.write = alsa_write;
	alsa_output.set_buffer = alsa_set_buffer;
	alsa_output.set_sample_rate = alsa_set_sample_rate;
#ifdef QUEUE_COUNT	
	alsa_output.get_queue_count = alsa_get_queue_count;
#endif	
	alsa_output.get_latency = alsa_get_latency;
	return &alsa_output;
}

