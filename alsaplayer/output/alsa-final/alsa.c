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

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "output_plugin.h"
#include "alsaplayer_error.h"

#define QUEUE_COUNT

static snd_pcm_t *sound_handle = NULL;
static snd_output_t *errlog;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static int frag_size = 2048;
static int frag_count = 16;
static int nr_channels = 2;
static unsigned int output_rate = 44100;

static int alsa_init(void)
{
	// Always return ok for now
	sound_handle = NULL;
	return 1;
}


static int alsa_open(const char *name)
{
	int err;

	if (!name || !*name) {
		name = "default";
	}	

	if ((err = snd_pcm_open(&sound_handle, name, stream, 0)) < 0) {

		alsaplayer_error("snd_pcm_open: %s (%s)", snd_strerror(err), name);
		return 0;
	}
	err = snd_output_stdio_attach(&errlog, stderr, 0);
	if (err < 0) {
		alsaplayer_error("snd_output_stdio_attach: %s", snd_strerror(err));
		return 0;
	}
	return 1;
}


static void alsa_close(void)
{
	if (sound_handle) {
		snd_pcm_drain(sound_handle);
		snd_pcm_close(sound_handle);
		sound_handle = NULL;
	}	
	if (errlog)
		snd_output_close(errlog);
	return;
}

#ifndef timersub
#define timersub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
  (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
  if ((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
	} \
} while (0)
#endif


/*
 *  *   Underrun and suspend recovery
 *   */

static int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (err == -EPIPE) {    /* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			alsaplayer_error("Can't recovery from underrun, prepare failed: %s", snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);       /* wait until the suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				alsaplayer_error("Can't recovery from suspend, prepare failed: %s", snd_strerror(err));
		}
		return 0;
	}
	return err;
}




static int alsa_write(void *data, int cnt)
{
	snd_pcm_uframes_t fcount;
	int err;

	fcount = (snd_pcm_uframes_t) (cnt / 4);
	if (!sound_handle) {
		alsaplayer_error("WTF?");
		return 0;
	}	
	err = snd_pcm_writei(sound_handle, data, fcount);
	if (err < 0) {
		if (xrun_recovery(sound_handle, err) < 0) {
			alsaplayer_error("alsa: xrun");
			return 0;
		}	
		err = snd_pcm_writei(sound_handle, data, fcount);
		if (err < 0) {
			if (xrun_recovery(sound_handle, err) < 0) {
				alsaplayer_error("alsa: xrun");
				return 0;
			}	
		}	
	}
	return 1;
}


static int alsa_set_buffer(int *fragment_size, int *fragment_count, int *channels)
{
	int err;
	unsigned int val;
	snd_pcm_uframes_t periodsize;
	snd_pcm_hw_params_t *hwparams;
	/* avoid "always true message" by assert(ptr) */
	snd_pcm_hw_params_t **noWarnPtr = &hwparams;
	snd_pcm_hw_params_alloca(noWarnPtr);
	if (!sound_handle) {
		puts("hmm, no sound handle... WTF?");
		goto _err;	
	}	
	err = snd_pcm_hw_params_any(sound_handle, hwparams);
	if (err < 0) {
		puts("error on snd_pcm_hw_par/ams_any");
		goto _err;
	}	
	err = snd_pcm_hw_params_set_access(sound_handle, hwparams,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		puts("error on set_access SND_PCM_ACCESS_RW_INTERLEAVED");
		goto _err;
	}	
	err = snd_pcm_hw_params_set_format(sound_handle, hwparams,
					   SND_PCM_FORMAT_S16);
	if (err < 0) {
		puts("error on set_format SND_PCM_FORMAT_S16");
		goto _err;
	}
	val = output_rate;
	err = snd_pcm_hw_params_set_rate_near(sound_handle, hwparams,
					 &val, 0);
	if (err < 0) {
		/* Try 48KHZ too */
		printf("%d HZ not available, trying 48000 HZ\n", output_rate);
		val = 48000;
		err = snd_pcm_hw_params_set_rate_near(sound_handle, hwparams,
				&val, 0);
		printf("error on setting output_rate (%d)\n", output_rate);			
		goto _err;
	}
	output_rate = val;
	
	err = snd_pcm_hw_params_set_channels(sound_handle, hwparams, *channels);
	if (err < 0) {
		printf("error on set_channels (%d)\n", *channels);
		goto _err;
	}
	periodsize = (*fragment_size) / 4; /* bytes -> frames for 16-bit,stereo */
	err = snd_pcm_hw_params_set_period_size_near(sound_handle, hwparams,
						&periodsize, 0);
	if (err < 0) {
		printf("error on set_period_size (%d)\n", (int)periodsize);			
		goto _err;
	}
	frag_size = periodsize << 2;
	
	val = *fragment_count;
	err = snd_pcm_hw_params_set_periods_near(sound_handle, hwparams,
					    &val, 0);
	if (err < 0) {
		printf("error on set_periods (%d)\n", val);			
		goto _err;
	}
	frag_count = val;
	
	err = snd_pcm_hw_params(sound_handle, hwparams);
	if (err < 0) {
		alsaplayer_error("Unable to install hw params:");
		snd_pcm_hw_params_dump(hwparams, errlog);
		return 0;
	}
	nr_channels = *channels;

	*fragment_size = frag_size;
	*fragment_count = frag_count;
	//alsaplayer_error("Fragment size/count: %d/%d", *fragment_size, *fragment_count);
	return 1;
 _err:
	alsaplayer_error("Unavailable hw params:");
	snd_pcm_hw_params_dump(hwparams, errlog);
	return 0;
}


static unsigned int alsa_set_sample_rate(unsigned int rate)
{
	output_rate = rate;
	alsa_set_buffer(&frag_size, &frag_count, &nr_channels);
	return output_rate;
}

#if 0
static int alsa_get_queue_count(void)
{
	snd_pcm_status_t *status;
	snd_pcm_uframes_t avail;
	int err;

	if (!sound_handle) {
		alsaplayer_error("No handle in alsa_get_queue_count()");
		return 0;
	}	
	snd_pcm_status_alloca(&status);
	if ((err = snd_pcm_status(sound_handle, status))<0) {
		alsaplayer_error("can't determine status");
		return 0;
	}	
	avail = snd_pcm_status_get_avail(status);				
	return ((int)avail);
}
#endif

static int alsa_get_latency(void)
{
	return (frag_size * frag_count);
}

output_plugin alsa_output;

#ifdef __cplusplus
extern "C" {
#endif
	
output_plugin *output_plugin_info(void)
{
	memset(&alsa_output, 0, sizeof(output_plugin));
	alsa_output.version = OUTPUT_PLUGIN_VERSION;
	alsa_output.name = "ALSA output v1.9.0beta12";
	alsa_output.author = "Andy Lo A Foe";
	alsa_output.init = alsa_init;
	alsa_output.open = alsa_open;
	alsa_output.close = alsa_close;
	alsa_output.write = alsa_write;
	alsa_output.set_buffer = alsa_set_buffer;
	alsa_output.set_sample_rate = alsa_set_sample_rate;
#ifdef QUEUE_COUNT
	//alsa_output.get_queue_count = alsa_get_queue_count;
#endif
	alsa_output.get_latency = alsa_get_latency;
	
	return &alsa_output;
}

#ifdef __cplusplus
}
#endif
