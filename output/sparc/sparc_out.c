/*  sparc_out.cpp - sparc output driver
 *  Copyright (C) 2007 Andy Lo A Foe <andy@alsaplayer.org>
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

#include <stropts.h>
#include <sys/conf.h>
#include <stdio.h>

#include <sys/audioio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>

#include "output_plugin.h"

static int sparc_audio;

static int sparc_open(int card, int dev)
{
	audio_info_t ainfo;
	struct audio_device ad;

	sparc_audio = open("/dev/audio", O_WRONLY);

	if (sparc_audio < 0) {
		fprintf(stderr, "Cannot open /dev/audio\n");
		return 0;
	}
	if(ioctl(sparc_audio, AUDIO_GETDEV, &ad) == -1) {
		fprintf(stderr, "Huh? 1\n");	
		close(sparc_audio);
		return 0;
	}
	printf("Audio device type: %s\n", ad.name);

	AUDIO_INITINFO(&ainfo);

	ainfo.play.port |= AUDIO_SPEAKER;

	if(ioctl(sparc_audio, AUDIO_SETINFO, &ainfo) == -1) {
		close(sparc_audio);
		return 0;
	}		
	return 1;	
}	

static void sparc_close()
{
	close(sparc_audio);
}

static int sparc_init()
{
	sparc_audio = -1;
	return 1;
}

static int sparc_write(void *data, int count)
{
	write(sparc_audio, data, count);
	return 1;
}	


static int sparc_set_buffer(int *fragment_size, int *fragment_count, int *channels)
{
	return 1;
}


static int sparc_set_sample_rate(int rate)
{
	audio_info_t ainfo;

	AUDIO_INITINFO(&ainfo);
	ainfo.play.sample_rate = rate;
	ainfo.play.channels = 2;
	ainfo.play.encoding = AUDIO_ENCODING_LINEAR;
	ainfo.play.precision = 16;
	ainfo.play.buffer_size = 20480;
	if(ioctl(sparc_audio, AUDIO_SETINFO, &ainfo) == -1)
		return 0;
	return rate;
}


output_plugin sparc_output;

output_plugin *output_plugin_info(void)
{
	memset(&sparc_output, 0, sizeof(output_plugin));
	sparc_output.version = OUTPUT_PLUGIN_VERSION;
	sparc_output.name = "SPARC output v1.0";
	sparc_output.author = "The 5 minute hack dept.";
	sparc_output.init = sparc_init;
	sparc_output.open = sparc_open;
	sparc_output.close = sparc_close;
	sparc_output.write = sparc_write;
	sparc_output.set_buffer = sparc_set_buffer;
	sparc_output.set_sample_rate = sparc_set_sample_rate;
	return &sparc_output;
}		

