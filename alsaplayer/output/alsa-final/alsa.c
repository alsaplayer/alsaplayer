/*  alsa.c - ALSA output plugin
 *  Copyright (C) 1999 by Andy Lo A Foe <andy@alsa-project.org>
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
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <string.h>
#include "output_plugin.h"
#include "error.h"

#define LOW_FRAGS	1	
//#define QUEUE_COUNT

static snd_pcm_t *sound_handle;
static snd_output_t *errlog;
static int stream = SND_PCM_STREAM_PLAYBACK;
static int frag_size = -1;
static int frag_count = -1;
static int output_rate = 44000;

static int alsa_init()
{
	// Always return ok for now
	sound_handle = NULL;
	return 1;
}


static int alsa_open(char *name)
{
	int err;

	if ((err = snd_pcm_open(&sound_handle, name, stream, 0)) < 0) {

		alsaplayer_error("snd_pcm_open: %s (%s)", snd_strerror(err),
										name);
		return 0;
	}
	err = snd_output_stdio_attach(&errlog, stderr, 0);
	if (err < 0) {
		alsaplayer_error("snd_output_stdio_attach: %s", snd_strerror(err));
		return 0;
	}
	return 1;
}


static void alsa_close()
{
	if (sound_handle) {
		snd_pcm_close(sound_handle);
		sound_handle = NULL;
	}	
	if (errlog)
		snd_output_close(errlog);
	return;
}

static int alsa_write(void *data, int count)
{
	int err;
	if (!sound_handle)
		return 0;
#if	1
	err = snd_pcm_writei(sound_handle, data, count / 4);
	if (err == -EPIPE) {
		alsaplayer_error("underrun. resetting stream");
		snd_pcm_prepare(sound_handle);
		err = snd_pcm_writei(sound_handle, data, count / 4);
		if (err != count / 4) {
			alsaplayer_error("ALSA write error: %s", snd_strerror(err));
			return 0;
		} else if (err < 0) {
			alsaplayer_error("ALSA write error: %s", snd_strerror(err));
			return 0;
		}	
	}
#endif
	return 1;
}


static int alsa_set_buffer(int fragment_size, int fragment_count, int channels)
{
	int err;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_hw_params_alloca(&hwparams);
	if (!sound_handle) {
		printf("hmm, no sound hanlde... WTF?\n");
		goto _err;	
	}	
	err = snd_pcm_hw_params_any(sound_handle, hwparams);
	if (err < 0) {
		printf("error on snd_pcm_hw_params_any\n");			
		goto _err;
	}	
	err = snd_pcm_hw_params_set_access(sound_handle, hwparams,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		printf("error on set_access SND_PCM_ACCESS_RW_INTERLEAVED\n");
		goto _err;
	}	
	err = snd_pcm_hw_params_set_format(sound_handle, hwparams,
					   SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		printf("error on set_format SND_PCM_FORMAT_S16_LE\n");			
		goto _err;
	}	
	err = snd_pcm_hw_params_set_rate_near(sound_handle, hwparams,
					 output_rate, 0);
	if (err < 0) {
		printf("error on setting output_rate (%d)\n", output_rate);			
		goto _err;
	}	
	err = snd_pcm_hw_params_set_channels(sound_handle, hwparams, channels);
	if (err < 0) {
		printf("error on set_channels (%d)\n", channels);
		goto _err;
	}	
	err = snd_pcm_hw_params_set_period_size(sound_handle, hwparams,
						fragment_size / 4, 0);
	if (err < 0) {
		printf("error on set_period_size (%d)\n", fragment_size / 4);			
		goto _err;
	}	
	err = snd_pcm_hw_params_set_periods(sound_handle, hwparams,
					    fragment_count, 0);
	if (err < 0) {
		printf("error on set_periods (%d)\n", fragment_count);			
		goto _err;
	}
	err = snd_pcm_hw_params(sound_handle, hwparams);
	if (err < 0) {
		alsaplayer_error("Unable to install hw params:");
		snd_pcm_hw_params_dump(hwparams, errlog);
		return 0;
	}
	
	frag_size = fragment_size;
	frag_count = fragment_count; 

	return 1;
 _err:
	alsaplayer_error("Unavailable hw params:");
	snd_pcm_hw_params_dump(hwparams, errlog);
	return 0;
}


static int alsa_set_sample_rate(int rate)
{
	output_rate = rate;
	return 1;
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

output_plugin alsa_output = {
	OUTPUT_PLUGIN_VERSION,
	{ "ALSA output v1.9.0beta8" },
	{ "Andy Lo A Foe" },
	alsa_init,
	alsa_open,
	alsa_close,
	alsa_write,
	alsa_set_buffer,
	alsa_set_sample_rate,
#ifdef QUEUE_COUNT
	alsa_get_queue_count,
#else
	NULL,
#endif	
	alsa_get_latency,
};


output_plugin *output_plugin_info(void)
{
	return &alsa_output;
}


