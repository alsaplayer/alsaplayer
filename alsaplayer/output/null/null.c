/*  null.c - NULL output plugin, useful, trust me :) 
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "output_plugin.h"
#include "alsaplayer_error.h"
#include "utilities.h"

static int null_init(void)
{
	alsaplayer_error("NOTE: THIS IS THE NULL PLUGIN."
					"      YOU WILL NOT HEAR SOUND!!");
	return 1;
}

static int null_open(const char *name)
{
	return 1;
}


static void null_close(void)
{
	return;
}


static int null_write(void *data, int count)
{
	dosleep(10000);
	return 1;
}


static int null_set_buffer(int *fragment_size, int *fragment_count, int *channels)
{
	return 1;
}


static unsigned int null_set_sample_rate(unsigned int rate)
{
	return rate;
}

static int null_get_latency(void)
{
	return 4096;
}

output_plugin null_output;

output_plugin *output_plugin_info(void)
{
	memset(&null_output, 0, sizeof(output_plugin));
	null_output.version = OUTPUT_PLUGIN_VERSION;
	null_output.name = "NULL output v1.0";
	null_output.author = "Andy Lo A Foe";
	null_output.init = null_init;
	null_output.open = null_open;
	null_output.close = null_close;
	null_output.write = null_write;
	null_output.set_buffer = null_set_buffer;
	null_output.set_sample_rate = null_set_sample_rate;
	null_output.get_latency = null_get_latency;
	return &null_output;
}
