/*
 * AlsaPlayer VORBIS plugin. Based on:
 * Vorbis xmms module by Tony Arcieri, <bascule@inferno.tusculum.edu>
 *
 * Copyright (C) 2001-2002 Andy Lo A Foe <andy@alsaplayer.org>
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
 *
*/ 

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include "input_plugin.h"

#define BLOCK_SIZE 4096	/* We can use any block size we like */

struct vorbis_local_data {
		OggVorbis_File vf;
		char path[FILENAME_MAX+1];
		int last_section;
		int bigendianp;
};		

/* Stolen from Vorbis' lib/vorbisfile.c */
static int is_big_endian(void) 
{
	unsigned short pattern = 0xbabe;
	unsigned char *bytewise = (unsigned char *)&pattern;

	if (bytewise[0] == 0xba) return 1;
	return 0;
}



/* The following callbacks are not needed but are here for future
 * expansion, like net streaming or other storage retrieval methods
 */
static size_t ovcb_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
		return fread(ptr, size, nmemb, (FILE *)datasource);
}


static int ovcb_seek(void *datasource, int64_t offset, int whence)
{
		return fseek((FILE *)datasource, offset, whence);
}


static int ovcb_close(void *datasource)
{
		return fclose((FILE *)datasource);
}


static long ovcb_tell(void *datasource)
{
		return ftell((FILE *)datasource);
}


ov_callbacks vorbis_callbacks = 
{
		ovcb_read,
		ovcb_seek,
		ovcb_close,
		ovcb_tell
};


static int vorbis_frame_seek(input_object *obj, int frame)
{
	struct vorbis_local_data *data;
	int64_t pos;
	
	if (!obj)
		return 0;
	data = (struct vorbis_local_data *)obj->local_data;
	/* Thanks go out to the Vorbis folks for making life easier :) 
	 * NOTE: (BLOCK_SIZE >> 2) need to be modified to take into note
	 * the channel number and sample size. Right now it's hardcoded at
	 * stereo with 16 bits samples.............
	 */
	if (data) {
			pos = frame * (BLOCK_SIZE >> 2);
			if (ov_pcm_seek(&data->vf, pos) == 0) 
					return frame;
	}		
	return 0;
}

static int vorbis_frame_size(input_object *obj)
{
	if (!obj) {
				printf("No frame size!!!!\n");
				return 0;
	}
	return obj->frame_size;
}


static int vorbis_play_frame(input_object *obj, char *buf)
{
	char pcmout[BLOCK_SIZE];
	char pcmout2[BLOCK_SIZE];
	int pcm_index;
	int ret;
	int current_section;
	int bytes_needed;
	int mono2stereo = 0;
	struct vorbis_local_data *data;
	vorbis_info* vi;
	
	if (!obj)
			return 0;
	data = (struct vorbis_local_data *)obj->local_data;
	if (!data)
			return 0;
	memset(pcmout,0,sizeof(pcmout));

	bytes_needed = sizeof(pcmout);
	if (obj->nr_channels == 1) { /* mono stream */
			bytes_needed >>= 1;
			mono2stereo = 1;
	}		
	pcm_index = 0;
	while (bytes_needed > 0) {	
		ret = ov_read(&data->vf, pcmout + pcm_index, bytes_needed, 
			 data->bigendianp, 2, 1, &current_section);
		switch (ret) {
			case 0:
				return 0;
			case -1:
				return 0;
		}
		pcm_index += ret;
		bytes_needed -= ret;
	}	
	if (bytes_needed != 0) {
		printf("Incomplete frame! (%d)\n", sizeof(pcmout) - bytes_needed);
	}	
	if (current_section != data->last_section) {
		/*
		 * The info struct is different in each section.  vf
		 * holds them all for the given bitstream.  This
		 * requests the current one
		 */
		vorbis_info* vi = ov_info(&data->vf, -1);
		if (!vi)
			return 0;	
		obj->nr_channels = vi->channels;
		if (vi->channels > 2) {
			return 0;
		}
		data->last_section = current_section;
	}
	if (mono2stereo) {
		int16_t *src, *dest;
		int count = sizeof(pcmout) / 4;
		int c;
		src = (int16_t *)pcmout;
		dest = (int16_t *)pcmout2;
		for (c = 0; c < count; c++) {
				*dest++ = *src;
				*dest++ = *src++;
		}
		memcpy(buf, pcmout2, sizeof(pcmout));
	} else {		
		memcpy(buf, pcmout, sizeof(pcmout));	
	}	
	return 1;
}


static  long vorbis_frame_to_sec(input_object *obj, int frame)
{
	struct vorbis_local_data *data;
	int64_t l;
	int sec;

	if (!obj)
			return 0;
	data = (struct vorbis_local_data *)obj->local_data;
	if (!data) {
			return 0;
	}
	sec = (frame * BLOCK_SIZE ) / (44100 * 2 * 2 / 100); 
	return sec;
}

static int vorbis_nr_frames(input_object *obj)
{
		struct vorbis_local_data *data;
		vorbis_info *vi;
		int64_t	l;
		int frames = 10000;		
		if (!obj)
				return 0;
		data = (struct vorbis_local_data *)obj->local_data;
		if (!data) {
				return 0;
		}		
		vi = ov_info(&data->vf, -1);
		if (!vi)
				return 0;
		l = ov_pcm_total(&data->vf, -1);
		frames = (l / (BLOCK_SIZE >> 2));	
		return frames;
}


int vorbis_stream_info(input_object *obj, stream_info *info)
{
	struct vorbis_local_data *data;	
	vorbis_comment *comment;
	vorbis_info *vi;
	char *t, *a;
	if (!obj || !info)
			return 0;
	data = (struct vorbis_local_data *)obj->local_data;
	if (data) {
			vi = ov_info(&data->vf, -1);
			/* UTF translation needed */
			if ((comment = ov_comment(&data->vf, -1)) != NULL) {
				t = vorbis_comment_query(comment, "title", 0);
				a = vorbis_comment_query(comment, "artist", 0);
				sprintf(info->title, "%s by %s",
					t ? t : "Unkown song", a ? a : "Unknown artist");
			} else {
					strcpy(info->title, data->path);
			}
			if (vi)
				sprintf(info->stream_type, "%dKHz %dkbit ogg",
							vi->rate / 1000, vi->bitrate_nominal / 1000);
			else
				sprintf(info->stream_type, "Unkown OGG VORBIS");
			info->author[0] = 0;
			info->status[0] = 0;
	}
	return 1;
}


static int vorbis_sample_rate(input_object *obj)
{
		struct vorbis_local_data *data;
		vorbis_info *vi;
		
		if (!obj)
				return 44100;
		data = (struct vorbis_local_data *)obj->local_data;
		if (data) {
				vi = ov_info(&data->vf, -1);
				if (vi)
					return vi->rate;
				else
					return 44100;	
		}		
		return 44100;
}

static int vorbis_init() 
{
		return 1;
}		


static int vorbis_channels(input_object *obj)
{
		struct vorbis_local_data *data;
		vorbis_info *vi;

		if (!obj)
				return 2; /* Default to 2, this is flaky at best! */
		data = (struct vorbis_local_data *)obj->local_data;
		if (data) {
				vi = ov_info(&data->vf, -1);
				if (vi)
					return vi->channels;
				else
					return 2;
		}
		return 2;
}


static float vorbis_can_handle(const char *path)
{
		FILE *stream;
		OggVorbis_File vfile;
		char *ext;

		ext = strrchr(path, '.');

		if (!ext) /* Until we get MIME type support, this is the safest 
								 method to detect file types */
						return 0.0;
		ext++;
		if (!strcasecmp(ext, "ogg"))
						return 1.0;
		else
						return 0.0;
#if FANCY_CHECKING		
		if ((stream = fopen(path, "r")) == NULL) {
				return 0.0;
		}		
		if (ov_open(stream, &vfile, NULL, 0) < 0)	 {
				fclose(stream);
				return 0.0;
		}
		ov_clear(&vfile);
		return 1;
#endif		
}


static int vorbis_open(input_object *obj, char *path)
{
		FILE *stream = NULL;
		vorbis_info *vi; 
		void *datasource = NULL;
		OggVorbis_File vf_temp;
		struct vorbis_local_data *data;
		
		memset(&vf_temp, 0, sizeof(vf_temp));
			
		if (!obj)
				return 0;
	
		if ((stream = fopen(path, "r")) == NULL) {
				return 0;
		}
		datasource = (void *)stream;

		if (ov_open_callbacks(datasource, &vf_temp, NULL, 0, vorbis_callbacks) < 0) {
				fclose(stream);
				return 0;
		}
		vi = ov_info(&vf_temp, -1);
		if (!vi) {
				fclose(stream);
				return 0;
		}		
				
		if (vi->channels > 2) { /* Can't handle 2+ channel files (yet) */
				fclose(stream);
				return 0;
		}	
		obj->nr_channels = vi->channels;
		obj->frame_size = BLOCK_SIZE;
		obj->local_data = malloc(sizeof(struct vorbis_local_data));
		if (!obj->local_data) {
				fclose(stream);
				return 0;
		}
		data = (struct vorbis_local_data *)obj->local_data;
		data->last_section = -1;
		data->bigendianp = is_big_endian();
		memcpy(&data->vf, &vf_temp, sizeof(vf_temp));
		memcpy(data->path, path, sizeof(data->path)-1);
		return 1;
}

static void vorbis_close(input_object *obj)
{
	struct vorbis_local_data *data;
	
	if (!obj)
			return;
	data = (struct vorbis_local_data *)obj->local_data;

	if (data)
		ov_clear(&data->vf);
	if (obj->local_data) {
		free(obj->local_data);
		obj->local_data = NULL;
	}
}


input_plugin vorbis_plugin =
{
	INPUT_PLUGIN_VERSION,
	0,
	{ "Ogg Vorbis player v1.1" },
	{ "Andy Lo A Foe" },
	NULL,
	vorbis_init,
	NULL,
	NULL,
	vorbis_can_handle,
	vorbis_open,
	vorbis_close,
	vorbis_play_frame,
	vorbis_frame_seek,
	vorbis_frame_size,
	vorbis_nr_frames,
	vorbis_frame_to_sec,
	vorbis_sample_rate,
	vorbis_channels,
	vorbis_stream_info,
	NULL,
	NULL
};


extern "C" {

input_plugin *input_plugin_info (void)
{
	return &vorbis_plugin;
}

}
