/*
 * AlsaPlayer MAD plugin.
 *
 * Copyright (C) 2001 Andy Lo A Foe <andy@alsaplayer.org>
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
 *
 *
 *  $Id$
 */ 

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "input_plugin.h"
#include "mad.h"

#define BLOCK_SIZE 4096	/* We can use any block size we like */
#define MAX_NUM_SAMPLES 8192

struct mad_local_data {
				
		char path[FILENAME_MAX+1];
		struct mad_synth  synth; 
	  struct mad_stream stream;
 	  struct mad_frame  frame;
		int16_t samples[MAX_NUM_SAMPLES];
};		


/* utility to scale and round samples to 16 bits */
	
static inline
signed int scale(mad_fixed_t sample)
{
				/* round */
				sample += (1L << (MAD_F_FRACBITS - 16));

				/* clip */
				if (sample >= MAD_F_ONE)
								sample = MAD_F_ONE - 1;
				else if (sample < -MAD_F_ONE)
								sample = -MAD_F_ONE;

				/* quantize */
			return sample >> (MAD_F_FRACBITS + 1 - 16);
}



static int mad_frame_seek(input_object *obj, int frame)
{
	struct mad_local_data *data;
	int64_t pos;
	
	if (!obj)
		return 0;
	data = (struct mad_local_data *)obj->local_data;
	
	return 0;
}

static int mad_frame_size(input_object *obj)
{
	if (!obj) {
				printf("No frame size!!!!\n");
				return 0;
	}
	return obj->frame_size;
}


static int mad_play_frame(input_object *obj, char *buf)
{
	char pcmout[BLOCK_SIZE];
	char pcmout2[BLOCK_SIZE];
	int pcm_index;
	int ret;
	int current_section;
	int bytes_needed;
	int mono2stereo = 0;
	struct mad_local_data *data;
	
	if (!obj)
			return 0;
	data = (struct mad_local_data *)obj->local_data;
	if (!data)
			return 0;
	memset(pcmout,0,sizeof(pcmout));

	return 1;
}


static int mad_frame_to_sec(input_object *obj, int frame)
{
	struct mad_local_data *data;
	int64_t l = 0;
	int sec = 0;

	if (!obj)
			return 0;
	data = (struct mad_local_data *)obj->local_data;
	return sec;
}


static int mad_nr_frames(input_object *obj)
{
		struct mad_local_data *data;
		int frames = 10000;		
		if (!obj)
				return 0;
		data = (struct mad_local_data *)obj->local_data;
		
		return frames;
}


int mad_stream_info(input_object *obj, stream_info *info)
{
	struct mad_local_data *data;	
	
	if (!obj || !info)
			return 0;
	data = (struct mad_local_data *)obj->local_data;
	
	return 1;
}


static int mad_sample_rate(input_object *obj)
{
		struct mad_local_data *data;
		
		if (!obj)
				return 44100;
		data = (struct mad_local_data *)obj->local_data;
		
		return 44100;
}

static int mad_init() 
{
		return 1;
}		


static int mad_channels(input_object *obj)
{
		struct mad_local_data *data;

		if (!obj)
				return 2; /* Default to 2, this is flaky at best! */
		data = (struct mad_local_data *)obj->local_data;
		
		return 2;
}


static float mad_test_support(const char *path)
{
	char *ext;      
	ext = strrchr(path, '.');

	if (!ext)
					return 0.0;
	ext++;
	if (!strcasecmp(ext, "mp3") ||
									!strcasecmp(ext, "mp2") ||
									strstr(path, "http://")) {
					return 0.9;
	} else {
					return 0.0;
	}
}


static int mad_open(input_object *obj, char *path)
{
		struct mad_local_data *data;
		
		if (!obj)
				return 0;
	
		obj->local_data = malloc(sizeof(struct mad_local_data));
		if (!obj->local_data) {
				return 0;
		}
		data = (struct mad_local_data *)obj->local_data;
	
		mad_synth_init  (&data->synth);
		mad_stream_init (&data->stream);
		mad_frame_init  (&data->frame);
		
		return 1;
}

static void mad_close(input_object *obj)
{
	struct mad_local_data *data;
	
	if (!obj)
			return;
	data = (struct mad_local_data *)obj->local_data;

	mad_synth_finish (&data->synth);
	mad_frame_finish (&data->frame);
	mad_stream_finish(&data->stream);
}


input_plugin mad_plugin =
{
	INPUT_PLUGIN_VERSION,
	0,
	{ "MAD plugin v0.1" },
	{ "Andy Lo A Foe" },
	NULL,
	mad_init,
	NULL,
	NULL,
	mad_test_support,
	mad_open,
	mad_close,
	mad_play_frame,
	mad_frame_seek,
	mad_frame_size,
	mad_nr_frames,
	mad_frame_to_sec,
	mad_sample_rate,
	mad_channels,
	mad_stream_info,
	NULL,
	NULL
};

input_plugin *input_plugin_info (void)
{
	return &mad_plugin;
}
