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
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "output_plugin.h"
#include "alsaplayer_error.h"

#define QUEUE_COUNT

static snd_pcm_t *sound_handle;
static snd_output_t *errlog;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static int frag_size = 2048;
static int frag_count = 16;
static int nr_channels = 2;
static unsigned int output_rate = 44100;

static int alsa_init()
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


static void alsa_close()
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


static int alsa_write(void *data, int count)
{
	snd_pcm_status_t *status;
	int err;
	
	if (!sound_handle)
		return 0;
	err = snd_pcm_writei(sound_handle, data, count / 4);
	if (err == -EPIPE) {
		snd_pcm_status_alloca(&status);
		if ((err = snd_pcm_status(sound_handle, status))<0) {
			alsaplayer_error("xrun. can't determine length");
		} else {
			if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
				struct timeval now, diff, tstamp;
				gettimeofday(&now, 0);
				snd_pcm_status_get_trigger_tstamp(status, &tstamp);
				timersub(&now, &tstamp, &diff);
				alsaplayer_error("xrun of at least %.3f msecs. resetting stream",
					diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
			} else 
				alsaplayer_error("xrun. can't determine length");
		}	
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
	return 1;
}


static int alsa_set_buffer(int fragment_size, int fragment_count, int channels)
{
	int err;
	unsigned int val;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_hw_params_alloca(&hwparams);
	if (!sound_handle) {
		puts("hmm, no sound handle... WTF?");
		goto _err;	
	}	
	err = snd_pcm_hw_params_any(sound_handle, hwparams);
	if (err < 0) {
		puts("error on snd_pcm_hw_params_any");
		goto _err;
	}	
	err = snd_pcm_hw_params_set_access(sound_handle, hwparams,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		puts("error on set_access SND_PCM_ACCESS_RW_INTERLEAVED");
		goto _err;
	}	
	err = snd_pcm_hw_params_set_format(sound_handle, hwparams,
					   SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		puts("error on set_format SND_PCM_FORMAT_S16_LE");
		goto _err;
	}	
	err = snd_pcm_hw_params_set_rate_near(sound_handle, hwparams,
					 output_rate, 0);
	if (err < 0) {
		/* Try 48KHZ too */
		printf("%d HZ not available, trying 48000 HZ\n", output_rate);
		output_rate = 48000;
		err = snd_pcm_hw_params_set_rate_near(sound_handle, hwparams,
				output_rate, 0);
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
	if ((val = snd_pcm_hw_params_get_periods_max(hwparams, 0)) < fragment_count) {
			fragment_count = val;
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
	nr_channels = channels;
	
	return 1;
 _err:
	alsaplayer_error("Unavailable hw params:");
	snd_pcm_hw_params_dump(hwparams, errlog);
	return 0;
}


static unsigned int alsa_set_sample_rate(unsigned int rate)
{
	output_rate = rate;
	alsa_set_buffer(frag_size, frag_count, nr_channels);
	return output_rate;
}

static int alsa_get_queue_count()
{
	snd_pcm_status_t *status;
	snd_pcm_uframes_t avail;
	int err;

	snd_pcm_status_alloca(&status);
	if ((err = snd_pcm_status(sound_handle, status))<0) {
		alsaplayer_error("can't determine status");
		return 0;
	}	
	avail = snd_pcm_status_get_avail(status);				
	return ((int)avail);
}

static int alsa_get_latency()
{
	//alsaplayer_error("frag_size = %d, frag_count = %d", frag_size, frag_count);
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
