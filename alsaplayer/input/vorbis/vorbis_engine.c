/*
 * AlsaPlayer VORBIS plugin.
 *
 * Copyright (C) 2001-2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include "alsaplayer_error.h"
#include "input_plugin.h"
#include "reader.h"

#define BLOCK_SIZE 4096	/* We can use any block size we like */

struct vorbis_local_data {
	OggVorbis_File vf;
	char path[FILENAME_MAX+1];
	int current_section;
	int last_section;
	long bitrate_instant;
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
	size_t result;
	if (!size)
		return 0;
	
	result = reader_read(ptr, size * nmemb, datasource);
	
	/* alsaplayer_error("request: %d * %d, result = %d", size, nmemb, result); */
	
	return (result / size);
}


static int ovcb_seek(void *datasource, int64_t offset, int whence)
{
	return (reader_seek(datasource, offset, whence));
}

static int ovcb_noseek(void *datasource, int64_t offset, int whence)
{
	/* When we are streaming without seeking support in reader plugin */
	return -1;
}


static int ovcb_close(void *datasource)
{
	reader_close(datasource);
	return 0;	
}


static long ovcb_tell(void *datasource)
{
	return reader_tell(datasource);
}


static ov_callbacks vorbis_callbacks = 
{
	ovcb_read,
	ovcb_seek,
	ovcb_close,
	ovcb_tell
};

static ov_callbacks vorbis_stream_callbacks =
{
	ovcb_read,
	ovcb_noseek,
	ovcb_close,
	NULL
};		

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
		if (obj->flags & P_STREAMBASED)
			return 0;
		pos = frame * (BLOCK_SIZE >> 2);
		if (ov_pcm_seek(&data->vf, pos) == 0) 
			return frame;
	}		
	return 0;
}

static int vorbis_frame_size(input_object *obj)
{
	if (!obj) {
		puts("No frame size!!!!");
		return 0;
	}
	return obj->frame_size;
}


static int vorbis_play_frame(input_object *obj, char *buf)
{
	int pcm_index;
	long ret;
	int bytes_needed;
	int mono2stereo = 0;
	struct vorbis_local_data *data;

	if (!obj || !obj->local_data)
		return 0;
	data = (struct vorbis_local_data *)obj->local_data;

	bytes_needed = BLOCK_SIZE;
	if (obj->nr_channels == 1) { /* mono stream */
		bytes_needed >>= 1;
		mono2stereo = 1;
	}		
	pcm_index = 0;
	while (bytes_needed > 0) {
		ret = ov_read(&data->vf, buf + pcm_index, bytes_needed, 
				data->bigendianp, 2, 1, &data->current_section);
		switch(ret) {
			case 0: /* EOF reached */
				return 0;
			case OV_EBADLINK:	
				alsaplayer_error("ov_read: bad link");
				return 0;
			case OV_HOLE:
				/* alsaplayer_error("ov_read: interruption in data (not fatal)"); */
				memset(buf + pcm_index, 0, bytes_needed);
				bytes_needed = 0;
				break;
			default:
				pcm_index += ret;
				bytes_needed -= ret;
		}		
	}
	if (data->current_section != data->last_section) {
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
			alsaplayer_error("vorbis_engine: no support for 2+ channels");
			return 0;
		}
		data->last_section = data->current_section;
	}
	if (mono2stereo) {
		char pcmout[BLOCK_SIZE];
		int16_t *src, *dest;
		int count = sizeof(pcmout) / 4;
		int c;

		memcpy(pcmout, buf, sizeof(pcmout) / 2);	
		src = (int16_t *)pcmout;
		dest = (int16_t *)buf;
		for (c = 0; c < count; c++) {
			*dest++ = *src;
			*dest++ = *src++;
		}
	}	
	return 1;
}


static  long vorbis_frame_to_sec(input_object *obj, int frame)
{
	struct vorbis_local_data *data;
	int sec;

	if (!obj || !obj->local_data)
		return 0;
	data = (struct vorbis_local_data *)obj->local_data;
	sec = (frame * BLOCK_SIZE ) / (vorbis_sample_rate(obj) * 2 * 2 / 100); 
	return sec;
}

static int vorbis_nr_frames(input_object *obj)
{
	struct vorbis_local_data *data;
	vorbis_info *vi;
	int64_t	l;
	int frames = 10000;		
	if (!obj || !obj->local_data)
		return 0;
	data = (struct vorbis_local_data *)obj->local_data;
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
	const char *t, *a, *l, *g, *y, *n, *c;

	if (!obj || !info)
		return 0;

	data = (struct vorbis_local_data *)obj->local_data;

	if (data) {
		strncpy (info->path, data->path, sizeof (info->path));
		if (obj->flags & P_SEEK && (comment = ov_comment(&data->vf, -1)) != NULL) {
			t = vorbis_comment_query(comment, "title", 0);
			a = vorbis_comment_query(comment, "artist", 0);
			l = vorbis_comment_query(comment, "album", 0);
			g = vorbis_comment_query(comment, "genre", 0);
			y = vorbis_comment_query(comment, "date", 0);
			n = vorbis_comment_query(comment, "tracknumber", 0);
			c = vorbis_comment_query(comment, "description", 0);

			snprintf(info->title, sizeof(info->title), "%s", t ? t : "");
			snprintf(info->artist, sizeof(info->artist), "%s", a ? a : "");
			snprintf(info->album, sizeof(info->album), "%s", l ? l : "");
			snprintf(info->genre, sizeof(info->genre), "%s", g ? g : "");
			snprintf(info->year, sizeof(info->year), "%s", y ? y : "");
			snprintf(info->track, sizeof(info->track), "%s", n ? n : "");
			snprintf(info->comment, sizeof(info->comment), "%s", c ? c : "");
		} else {
			snprintf(info->title, sizeof(info->title), "%s", data->path);
			info->artist [0] = '\0';
			info->album [0] = '\0';
			info->genre [0] = '\0';
			info->year [0] = '\0';
			info->track [0] = '\0';
			info->comment [0] = '\0';
		}

		vi = ov_info(&data->vf, -1);
		if (vi) {
			long br = ov_bitrate_instant(&data->vf);
			if (br > 0) 
				data->bitrate_instant = br;
			sprintf(info->stream_type, "Vorbis %dKHz %s %-3dkbit",
					(int)(vi->rate / 1000), 
					 obj->nr_channels == 1 ? "mono":"stereo",
					(int)(data->bitrate_instant / 1000));
		} else {
			strcpy(info->stream_type, "Unkown OGG VORBIS");
		}	
		info->status[0] = 0;
	}
	return 1;
}


static int vorbis_init(void) 
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
	char *ext;

	ext = strrchr(path, '.');

	if (ext)
		ext++;
//#if FANCY_CHECKING		
#if 0 // This piece breaks Icecast streams. Need to implemente MIME types
        OggVorbis_File vf_temp;
        reader_type *datasource;
	int stream;
	int rc;

	if ((datasource = reader_open(path, NULL, NULL)) == NULL)
	    return 0.0;

	memset(&vf_temp, 0, sizeof (vf_temp));
	stream = (strncmp(path, "http://", 7) == 0) ? 1 : 0;
	rc = ov_test_callbacks(datasource, &vf_temp, NULL, 0,
			       stream ? vorbis_stream_callbacks : vorbis_callbacks);

	return rc == 0 ? 1.0 : 0.0;
#else
	if (!ext) /* Until we get MIME type support, this is the safest 
		     method to detect file types */
		return 0.0;
	
	if (!strncasecmp(ext, "ogg", 3)) {
		return 1.0;
	} else
		return 0.0;
#endif		
}


static int vorbis_open(input_object *obj, const char *path)
{
	vorbis_info *vi; 
	void *datasource = NULL;
	OggVorbis_File vf_temp;
	struct vorbis_local_data *data;

	memset(&vf_temp, 0, sizeof(vf_temp));

	if (!obj)
		return 0;

	if ((datasource = reader_open(path, NULL, NULL)) == NULL) {
		return 0;
	}

	obj->flags = 0;
	if (reader_seekable (datasource)) {
		obj->flags |= P_SEEK;
		obj->flags |= P_PERFECTSEEK;
		obj->flags |= P_FILEBASED;
	} else {
		obj->flags |= P_STREAMBASED;
	}	

	if (ov_open_callbacks(datasource, &vf_temp, NULL, 0, 
		(obj->flags & P_STREAMBASED) ? vorbis_stream_callbacks : vorbis_callbacks) < 0) {
		ov_clear(&vf_temp);
		return 0;
	}

	vi = ov_info(&vf_temp, -1);
	if (!vi) {
		ov_clear(&vf_temp);
		return 0;
	}		

	if (vi->channels > 2) { /* Can't handle 2+ channel files (yet) */
		ov_clear(&vf_temp);
		return 0;
	}	
	obj->nr_channels = vi->channels;
	obj->frame_size = BLOCK_SIZE;
	obj->local_data = malloc(sizeof(struct vorbis_local_data));
	if (!obj->local_data) {
		ov_clear(&vf_temp);
		return 0;
	}
	data = (struct vorbis_local_data *)obj->local_data;
	data->current_section = -1;
	data->last_section = -1;
	data->bitrate_instant = 0;
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


static void vorbis_shutdown(void)
{
	return;
}



static input_plugin vorbis_plugin;

#ifdef __cplusplus
extern "C" {
#endif
	
input_plugin *input_plugin_info (void)
{
	memset(&vorbis_plugin, 0, sizeof(input_plugin));
	vorbis_plugin.version = INPUT_PLUGIN_VERSION;
	vorbis_plugin.name = "Ogg Vorbis player v1.2";
	vorbis_plugin.author = "Andy Lo A Foe";
	vorbis_plugin.init = vorbis_init;
	vorbis_plugin.shutdown = vorbis_shutdown;
	vorbis_plugin.can_handle = vorbis_can_handle;
	vorbis_plugin.open = vorbis_open;
	vorbis_plugin.close = vorbis_close;
	vorbis_plugin.play_frame = vorbis_play_frame;
	vorbis_plugin.frame_seek = vorbis_frame_seek;
	vorbis_plugin.frame_size = vorbis_frame_size;
	vorbis_plugin.nr_frames = vorbis_nr_frames;
	vorbis_plugin.frame_to_sec = vorbis_frame_to_sec;
	vorbis_plugin.sample_rate = vorbis_sample_rate;
	vorbis_plugin.channels = vorbis_channels;
	vorbis_plugin.stream_info = vorbis_stream_info;
	return &vorbis_plugin;
}

#ifdef __cplusplus
}
#endif
