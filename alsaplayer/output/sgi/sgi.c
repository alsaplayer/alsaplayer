/*  sgi.c - SGI Audio Library output module
 *  Copyright (C) 1999 Michael Pruett <michael@68k.org>
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
 * As an exception to the GPL-v3 licence, you are authorised to compile,
 * link and distribute AlsaPlayer on a SGI system. See the COPYING file
 * in the main AlsaPlayer directory.
 *
*/ 

#include "config.h"
#include <stdio.h>
#include <dmedia/audio.h>
#include "output_plugin.h"

static ALport port = NULL;

static int sgi_init ()
{
	port = alOpenPort("alsaplayer", "w", NULL);

	if (port == NULL)
	{
		printf("alOpenPort failed!");
		return 0;
	}

	return 1;
}

static int sgi_open (const char *device)
{
	if (port != NULL) 
		return 1;
	return 0;
}

static void sgi_close ()
{
	return;
}

static int sgi_write (void *data, int count)
{
	ALconfig	config;
	int			frameSize, channelCount;

	config = alGetConfig(port);
	if (config == NULL)
	{
		printf("SGI: alGetConfig failed.\n");
		return 0;
	}

	channelCount = alGetChannels(config);
	frameSize = alGetWidth(config);

	if (frameSize == 0 || channelCount == 0)
	{
		printf("SGI: bad configuration.\n");
		return 0;
	}

	alWriteFrames(port, data, count / (frameSize * channelCount));
	return 1;
}

static int sgi_set_buffer(int *fragment_size, int *fragment_count, int *channels)
{
	printf("SGI: fragments fixed at 256/256, stereo\n");
	return 1;
}

static int sgi_set_sample_rate(int rate)
{
	ALpv	params;
	int		rv;

	rv = alGetResource(port);

	params.param = AL_RATE;
	params.value.ll = (long long) rate << 32;

	if (alSetParams(rv, &params, 1) < 0)
	{
		printf("SGI: alSetParams failed: %s\n", alGetErrorString(oserror()));
		return 0;
	}

	return rate;
}

static int sgi_get_latency ()
{
	return ((256*256));
}

output_plugin sgi_output;

output_plugin *output_plugin_info(void)
{
	memset(&sgi_output, 0, sizeof(output_plugin));
	sgi_output.version = OUTPUT_PLUGIN_VERSION;
	sgi_output.name = "SGI Audio Library output v1.0 (broken for mono output)";
	sgi_output.author = "Michael Pruett";
	sgi_output.init = sgi_init;
	sgi_output.open = sgi_open;
	sgi_output.close = sgi_close;
	sgi_output.write = sgi_write;
	sgi_output.set_buffer = sgi_set_buffer;
	sgi_output.set_sample_rate = sgi_set_sample_rate;
	sgi_output.get_latency = sgi_get_latency;
	return &sgi_output;
}
