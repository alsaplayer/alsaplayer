/*  mpg123_engine.c 
 *  Copyright (C) 1999 Andy Lo A Foe <arloafoe@cs.vu.nl>
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "mpg123.h"
#include "mpg123_engine.h"
#include "input_plugin.h"

#define RESYNC_FRAMES 4

/* #define DEBUG */

static long outscale = 32768; /* Have to figure out what this does */
static int decoder_init = 0;
int _internal_mpg123_error = 0;
char *equalfile = NULL; 

struct mpeg_local_data {
	char mpeg_name[FILENAME_MAX+1];
	struct parameter param;
	struct frame fr;

	int streaming;
	int padded;
	int cnt;
	int junk_size;

	int sample_freq;
	int mpeg_fd;
};


extern int tabsel_123[2][3][16];
extern void stream_close(struct reader *rds);
extern int calc_numframes(struct frame *);
void stream_jump_to_frame(struct frame *fr,int frame);
void set_synth_functions(struct frame *fr);


static int mpeg_channels(input_object *obj)
{
	if (!obj)
		return 0;	
	return obj->nr_channels;
}

static int mpeg_frame_size(input_object *obj)
{
	if (!obj)
			return 0;
	return obj->frame_size;
}


static int mpeg_sample_rate(input_object *obj)
{
	struct mpeg_local_data *data;	
	int result = 0;	
	if (!obj)
			return result;
	data = (struct mpeg_local_data *)obj->local_data;	
	if (data)
			result = data->sample_freq;
	return result;
}


static int mpeg_nr_frames(input_object *obj)
{
	if (!obj)
			return 0; 
	return obj->nr_frames; /* This is silly, but clean */
}


static void initialise_decoder()
{
	pcm_sample = (unsigned char *)malloc(16384);
	
	make_decode_tables(outscale);
    init_layer2(); /* inits also shared tables with layer1 */
    init_layer3(0); /* No down sample support (yet?) */
}


int mpeg_play_frame(input_object *obj, char *buf);

static int mpeg_frame_seek(input_object *obj, int frame)
{
	struct mpeg_local_data *data;	
	int i;	
	if (!obj)
			return 0;
	data = (struct mpeg_local_data *)obj->local_data;
	if (data && rd) {
		if (data->streaming) {
			fprintf(stderr, "No seeking in streaming data\n");
			return 0;
		}	
		if (frame < RESYNC_FRAMES) { /* Always skip first 3 to 5 frames */
			stream_jump_to_frame(&data->fr, RESYNC_FRAMES);
			mpeg_play_frame(obj, NULL);
			mpeg_play_frame(obj, NULL);
			return RESYNC_FRAMES + 2;
		} else {
			stream_jump_to_frame(&data->fr, frame-RESYNC_FRAMES);
			for (i = 0; i < RESYNC_FRAMES; i++)
				mpeg_play_frame(obj, NULL);
			return frame;
		}
	} else {
		printf("WOAH. READER OR DATA = NULL\n");
	}	
	return frame;
}




int mpeg_play_frame(input_object *obj, char *buf)
{
	struct mpeg_local_data *data;
	struct audio_info_struct ai;
	int written;
	
	if (!obj || _internal_mpg123_error)
			return 0;
	data = (struct mpeg_local_data *)obj->local_data;
#ifdef DEBUG
	printf("playing frame\n");
#endif
	if (data) {
		if (!read_frame(&data->fr)) {
			return 0;
		}	
		if (data->fr.error_protection) {
			getbits(16);
		}
		ai.pcm_buffer = buf;
		written = data->fr.do_layer(&data->fr, 0, &ai);
#ifdef DEBUG
		printf("%d -> %d\n", written, mpeg_frame_size(obj)); 
#endif
		if (written > mpeg_frame_size(obj)) {
			fprintf(stderr, "WARNING: frame too large (%d > %d)\n", written, mpeg_frame_size(obj));
		}	
	}
#ifdef DEBUG
	printf("play frame success\n");
#endif	
	return 1;
}


static int mpeg_get_frame_info(input_object *obj, const char *fname)
{
	struct mpeg_local_data *data;
		
	if (!obj)
			return 0;
    data = (struct mpeg_local_data *)obj->local_data;
	if (data) {
		read_frame_init();
		if (!read_frame(&data->fr)) {
			return 0;
		}
		data->param.down_sample = 0;
		data->fr.down_sample = 0;
		data->fr.single = -1;
		data->fr.down_sample_sblimit = SBLIMIT>>(data->fr.down_sample);
		data->sample_freq = freqs[data->fr.sampling_frequency]>>(data->param.down_sample);
		set_synth_functions(&data->fr);
		init_layer3(data->fr.down_sample_sblimit);
	
		obj->nr_frames = data->streaming ? 0 : calc_numframes(&data->fr);
		obj->frame_size = 4608;
		/* if (data->fr.stereo == 1) obj->frame_size = 2304; */
		/* Is this a constant? Dunno.. Well, we do now.. It's not constant for
		 * different layers and MPEG versions */
#if 1	
		switch(data->sample_freq) {
					case 22050: obj->frame_size >>= 1;
								break;
					case 11025: obj->frame_size >>= 2;
								break;
					default:
								break;
		}						
#endif
		pcm_point = 0; /* !!!!!!!!! */
	}
	return 1;
}


static void mpeg_close(input_object *obj)
{
	if (!obj)
			return;
	obj->nr_frames = 0;
	if (obj->local_data) {
			free(obj->local_data);
			obj->local_data = NULL;
			if (rd) rd->close(rd);
	}		
}

int mpeg_open(input_object *obj, char *path)
{
	struct mpeg_local_data *data;	
	int bla, i;
	struct id3tag tag;

	_internal_mpg123_error = 0;

	if (!decoder_init) {
		decoder_init = 1;
		initialise_decoder();
	}

	if (!obj)
			return 0;

	obj->local_data = malloc(sizeof(struct mpeg_local_data));
	if (!obj->local_data) {
		return 0;
	}
	data = (struct mpeg_local_data *)obj->local_data;

	data->streaming = 0;
	data->junk_size = 0;
	obj->nr_channels = 2; /* !!!!!!!!! */
	
#ifdef DEBUG	
	printf("Opening stream %s\n", path);	
#endif
	if (!open_stream(path, -1)) {
		printf("erorr opening stream\n");
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	}
	data->mpeg_name[0] = 0;
	
	if (strstr(path, "http://") != NULL) {
		sprintf(data->mpeg_name, "ShoutCast from %s\n", path);
		data->streaming = 1;
	}	
	if (!mpeg_get_frame_info(obj, path)) {
		printf("Info fout!\n");
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	}
	
	if (!data->streaming && (bla = open(path, O_RDONLY)) != -1) {
		lseek(bla, -128, SEEK_END);
		read(bla, &tag, sizeof(struct id3tag));
		if (!strncmp("TAG", tag.tag,3)) {
			tag.title[29] = tag.artist[29] = 0;
			for (i = 28; i > 0; i--) {
				if (tag.title[i] != ' ')  {
					tag.title[i+1] = 0;
					break;
				}
			}
			sprintf(data->mpeg_name, "%s%s%s", tag.title, 
				strlen(tag.artist) ? " by " : "",  tag.artist);
		}
		close(bla);
	}
	if (!strlen(data->mpeg_name) && !data->streaming) {
		char *ptr = strrchr(path, '/');
	
		if (ptr)
			ptr++; /* get rid of / */
		else
			ptr = path;
		if (strlen(ptr) > FILENAME_MAX) {
			strncpy(data->mpeg_name, ptr, FILENAME_MAX-1);
			data->mpeg_name[FILENAME_MAX-1] = 0;
		} else 
			strcpy(data->mpeg_name, ptr);
	}
	
	/* Set up object */
	
	obj->flags = P_SEEK;
	
#ifdef DEBUG
	printf("Name is %s\n", data->mpeg_name);
#endif
	return 1;
}


void set_synth_functions(struct frame *fr)
{
	typedef int (*func)(real *,int,unsigned char *,int *);
	typedef int (*func_mono)(real *,unsigned char *,int *);
	int ds = fr->down_sample;
	int p8=0;

	static func funcs[2][4] = { 
		{ synth_1to1,
		  synth_2to1,
		  synth_4to1,
		  synth_ntom } ,
		{ synth_1to1_8bit,
		  synth_2to1_8bit,
		  synth_4to1_8bit,
		  synth_ntom_8bit } 
	};

	static func_mono funcs_mono[2][2][4] = {    
		{ { synth_1to1_mono2stereo ,
		    synth_2to1_mono2stereo ,
		    synth_4to1_mono2stereo ,
		    synth_ntom_mono2stereo } ,
		  { synth_1to1_8bit_mono2stereo ,
		    synth_2to1_8bit_mono2stereo ,
		    synth_4to1_8bit_mono2stereo ,
		    synth_ntom_8bit_mono2stereo } } ,
		{ { synth_1to1_mono ,
		    synth_2to1_mono ,
		    synth_4to1_mono ,
		    synth_ntom_mono } ,
		  { synth_1to1_8bit_mono ,
		    synth_2to1_8bit_mono ,
		    synth_4to1_8bit_mono ,
		    synth_ntom_8bit_mono } }
	};

	if(0)
		p8 = 1;
	fr->synth = funcs[p8][ds];
	fr->synth_mono = funcs_mono[1][p8][ds];

	if(p8) {
		make_conv16to8_table(-1); /* FIX */
	}
}


unsigned long mpeg_frame_to_sec(input_object *obj, int frame)
{
	struct mpeg_local_data *data;	
	unsigned long secs = 0;
	if (!obj)
			return secs;
	data = (struct mpeg_local_data *)obj->local_data;
	if (data)
		secs = data->sample_freq ? frame * (obj->frame_size >> 2) / (data->sample_freq / 100) : 0;
	return secs;
}


static float mpeg_can_handle(const char *name)
{
	char *ext;	
	ext = strrchr(name, '.');
	
	if (!ext)
		return 0.0;
	ext++;
	if (!strcasecmp(ext, "mp3") ||
		!strcasecmp(ext, "mp2") ||
		strstr(name, "http://")) {
			return 0.8;
	} else {
			return 0.0;
	}
}


int mpeg_init()
{
	return 1;
}


int mpeg_stream_info(input_object *obj, stream_info *info)
{
	struct mpeg_local_data *data;	
	if (!obj || !info)
		return 0;
	data = (struct mpeg_local_data *)obj->local_data;
	if (data) {
#if 1
		sprintf(info->stream_type, "%d-bit %dKHz %s %d Kbit/s Audio MPEG", 16,
			data->sample_freq / 1000,
			obj->nr_channels == 2 ? "stereo" : "mono",
			tabsel_123[data->fr.lsf][data->fr.lay-1][data->fr.bitrate_index]);
		info->author[0] = 0;
		strcpy(info->title, data->mpeg_name);
#else
		info->title[0] = info->author[0] = info->stream_type[0] = 0;
#endif		
	}	
	return 1;
}


input_plugin mpg123_plugin = {
		INPUT_PLUGIN_VERSION,
		0,
		{ "mpg123 MPEG player v0.59r (obsolete)" },
		{ "Andy Lo A Foe <andy@alsa-project.org>" },
		NULL,
		mpeg_init,
		NULL,
		NULL,	
		mpeg_can_handle,
		mpeg_open,
		mpeg_close,
		mpeg_play_frame,
		mpeg_frame_seek,
		mpeg_frame_size,
		mpeg_nr_frames,
		mpeg_frame_to_sec,
		mpeg_sample_rate,
		mpeg_channels,
		mpeg_stream_info,
		NULL,
		NULL
				
};

input_plugin *input_plugin_info(void)
{
	return &mpg123_plugin;
}

