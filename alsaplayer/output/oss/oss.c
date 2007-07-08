/*  oss.c - Output driver for OSS/Lite and OSS/Linux
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "output_plugin.h"
#include "alsaplayer_error.h"

static int oss_fd;
static int oss_card;
static int oss_device;

static int oss_init(void)
{
	// Always return ok for now
	oss_fd = -1;
	oss_card = 0;
	oss_device = 0;
	return 1;
}

static int oss_open(const char *name)
{
	if (name[0] != '/') {
		name = "/dev/dsp";
		}
	if ((oss_fd = open(name,  O_WRONLY, 0)) == -1) {
		alsaplayer_error("error opening %s", name);
		return 0;
	}
	return 1;
}


static void oss_close(void)
{
	close(oss_fd);
	return;
}


static int oss_write(void *data, int count)
{
	write(oss_fd, data, count);
	return 1;
}


static int oss_set_buffer(int *fragment_size, int *fragment_count, int *channels)
{
	int val;
	int hops;
	int size;
	int count;
	
	size = *fragment_size;
	count = *fragment_count;
	
	for (hops=0; size >>=1; hops++);
	
	val = (count << 16) + hops;
	ioctl(oss_fd,SNDCTL_DSP_SETFRAGMENT,&val);
	val = AFMT_S16_NE;
	ioctl(oss_fd,SNDCTL_DSP_SETFMT,&val);
	val = *channels - 1;
	ioctl(oss_fd,SNDCTL_DSP_STEREO,&val);
	ioctl(oss_fd,SNDCTL_DSP_GETBLKSIZE,&val);
	return 1;
}


static unsigned int oss_set_sample_rate(unsigned int rate)
{
	if (ioctl(oss_fd,SNDCTL_DSP_SPEED,&rate) < 0) {
					alsaplayer_error("error setting sample_rate");
					return 0;
	}				
	return rate;
}


output_plugin oss_output;

#ifdef __cplusplus
extern "C" {
#endif

output_plugin *output_plugin_info(void)
{
	memset(&oss_output, 0, sizeof(output_plugin));
	oss_output.version = OUTPUT_PLUGIN_VERSION;
	oss_output.name = "OSS output v1.0";
	oss_output.author = "Andy Lo A Foe";
	oss_output.init = oss_init;
	oss_output.open = oss_open;
	oss_output.close = oss_close;
	oss_output.write = oss_write;
	oss_output.set_buffer = oss_set_buffer;
	oss_output.set_sample_rate = oss_set_sample_rate;
	return &oss_output;
}

#ifdef __cplusplus
}
#endif

