/*  null.c - NULL output plugin, useful, trust me :) 
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "output_plugin.h"


static int null_init()
{
	fprintf(stdout, "NOTE: THIS IS THE NULL PLUGIN.\n"
					"      YOU WILL NOT HEAR SOUND!!\n");
	return 1;
}

static int null_open(char *name)
{
	return 1;
}


static void null_close()
{
	return;
}


static int null_write(void *data, int count)
{
	static int warn = 0;

	if (warn++ > 100) {
			printf("NULL PLUGIN ACTIVE...NO SOUND\n");
			warn = 0;
	}		
	usleep(10000);
	return 1;
}


static int null_set_buffer(int fragment_size, int fragment_count, int channels)
{
	return 1;
}


static int null_set_sample_rate(int rate)
{
	return rate;
}

static int null_get_latency()
{
	return 4096;
}

output_plugin null_output = {
	OUTPUT_PLUGIN_VERSION,
	{ "NULL output v1.0" },
	{ "Andy Lo A Foe" },
	null_init,
	null_open,
	null_close,
	null_write,
	null_set_buffer,
	null_set_sample_rate,
	NULL,
	null_get_latency
};


output_plugin *output_plugin_info(void)
{
	return &null_output;
}
