/*  oss.c - Output driver for OSS/Lite and OSS/Linux
 *  Copyright (C) 1999 Andy Lo A Foe <andy@alsa-project.org>
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

#define LOW_FRAGS	1	

static int oss_fd;
static int oss_card;
static int oss_device;

static int oss_init()
{
	// Always return ok for now
	oss_fd = -1;
	oss_card = 0;
	oss_device = 0;
	return 1;
}

static int oss_open(char *name)
{
	int err;

	if (name[0] != '/') {
		name = "/dev/dsp";
		}
	if ((oss_fd = open(name,  O_WRONLY, 0)) == -1) {
		alsaplayer_error("error opening %s", name);
		return 0;
	}
	return 1;
}


static void oss_close()
{
	close(oss_fd);
	return;
}


static int oss_write(void *data, int count)
{
	write(oss_fd, data, count);
	return 1;
}


static int oss_set_buffer(int fragment_size, int fragment_count, int channels)
{
	static int val;
	int hops;

	for (hops=0; fragment_size >>=1; hops++);
	
	val = (fragment_count << 16) + hops;
	ioctl(oss_fd,SNDCTL_DSP_SETFRAGMENT,&val);
	val = AFMT_S16_NE;
	ioctl(oss_fd,SNDCTL_DSP_SETFMT,&val);
	val = channels - 1;
	ioctl(oss_fd,SNDCTL_DSP_STEREO,&val);
	ioctl(oss_fd,SNDCTL_DSP_GETBLKSIZE,&val);
	return 1;
}


static int oss_set_sample_rate(int rate)
{
	if (ioctl(oss_fd,SNDCTL_DSP_SPEED,&rate) < 0) {
					alsaplayer_error("error setting sample_rate");
					return 0;
	}				
	return rate;
}


output_plugin oss_output = {
	OUTPUT_PLUGIN_VERSION,
	{ "OSS output v1.0" },
	{ "Andy Lo A Foe" },
	oss_init,
	oss_open,
	oss_close,
	oss_write,
	oss_set_buffer,
	oss_set_sample_rate,
	NULL,
	NULL
};


output_plugin *output_plugin_info(void)
{
	return &oss_output;
}
