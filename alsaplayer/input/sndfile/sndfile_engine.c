/*
 *  sndfile_engine.c (C) 2002 by Andy Lo A Foe <andy@alsaplayer.org>
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
#include <alloca.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sndfile.h>
#include "input_plugin.h"
#include "alsaplayer_error.h"
static const int FRAME_SIZE = 512;

struct sf_local_data
{
	SNDFILE* sfhandle;
	SF_INFO	 sfinfo;
	int framesize;
	char filename[1024];
	char path[1024];
};


static int sndfile_open (input_object *obj, const char *name)
{
	struct sf_local_data *data;
	char *p;

	if (!obj)
		return 0;

	obj->local_data = malloc(sizeof(struct sf_local_data));

	if (!obj->local_data)
	{
		return 0;
	}
	obj->nr_frames = 0;

	data = (struct sf_local_data *) obj->local_data;

	data->sfhandle = sf_open(name, SFM_READ, &data->sfinfo);
	data->framesize = FRAME_SIZE;

	if (data->sfhandle == NULL)
	{
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	}
	p = strrchr(name, '/');
	if (p) {
		strcpy(data->filename, ++p);
	} else {
		strcpy(data->filename, name);
	}

	strcpy(data->path, name);
	if (data->sfinfo.seekable)
		obj->flags = P_SEEK;
	return 1;
}


void sndfile_close (input_object *obj)
{
	if (obj == NULL)
		return;

	if (obj->local_data) {
		struct sf_local_data *data =
			(struct sf_local_data *) obj->local_data;
		sf_close(data->sfhandle);
		free(obj->local_data);
		obj->local_data = NULL;
	}

	return;
}


static int sndfile_play_frame (input_object *obj, char *buf)
{
	size_t	bytes_to_read;
	size_t	items_read;
	size_t	samples;
	size_t	i;
	void	*buffer;

	struct sf_local_data	*data;

	if (!obj)
		return 0;

	data = (struct sf_local_data *) obj->local_data;

	if (!data)
		return 0;

	buffer = alloca(FRAME_SIZE);

	if (!buffer)
		return 0;

	if (data->sfinfo.channels == 1) { /* Mono, so double */
		short *dest;
		short *src;

		bytes_to_read = FRAME_SIZE / 2;
		samples = bytes_to_read / sizeof(short);
		items_read = sf_read_short(data->sfhandle, (short *)buffer, samples);

		if (buf) {
			src = (short *)buffer;
			dest = (short *)buf;
			for (i = 0; i < items_read; i++) {
				*(dest++) = *src;
				*(dest++) =	*(src++);
			}
			if (items_read == 0) {
				return 0;
			}
		}
	} else {
		bytes_to_read = FRAME_SIZE;

		items_read = sf_read_short(data->sfhandle, (short *)buffer,
				bytes_to_read / sizeof(short));
		if (buf)
			memcpy(buf, buffer,  FRAME_SIZE);
		else
			return 0;
		if (items_read != (bytes_to_read / sizeof(short)))
			return 0;
	}
	return 1;
}


static int sndfile_frame_seek (input_object *obj, int frame)
{
	struct sf_local_data	*data;
	sf_count_t pos;
	int result = 0;

	if (!obj)
		return result;

	data = (struct sf_local_data *) obj->local_data;

	if (data->sfhandle == NULL)
	{
		return result;
	}
	pos = (frame * FRAME_SIZE) / (sizeof (short) * data->sfinfo.channels) ;
	//alsaplayer_error("pos = %d", pos);
	if (sf_seek(data->sfhandle, pos, SEEK_SET) != pos)
		return 0;
	return frame;
}


static int sndfile_nr_frames (input_object *obj)
{
	struct sf_local_data    *data;
	sf_count_t samples;

	if (!obj)
		return 0;
	data = (struct sf_local_data *) obj->local_data;
	samples = data->sfinfo.frames;

	if (samples > 0)  {
		return ((int)data->sfinfo.frames * 2 *
			     sizeof (short)
				/ FRAME_SIZE);
	}
	return 0;
}


static int sndfile_frame_size (input_object *obj)
{
	return FRAME_SIZE;
}


static int sndfile_sample_rate (input_object *obj)
{
	struct sf_local_data *data;

	if (!obj)
		return 0;

	data = (struct sf_local_data *) obj->local_data;

	return data->sfinfo.samplerate;
}


static int sndfile_channels (input_object *obj)
{
	if (!obj)
		return 0;

	return obj->nr_channels;
}


static  long sndfile_frame_to_sec (input_object *obj, int frame)
{
	unsigned long	result = 0;

	struct sf_local_data	*data;

	if (!obj)
		return result;

	data = (struct sf_local_data *) obj->local_data;

	if (!data)
		return result;
	result = (unsigned long) (frame * FRAME_SIZE / 2 /
			(data->sfinfo.samplerate * sizeof (short) / 100));

	return result;
}

static float sndfile_can_handle (const char *name)
{
	const char *fname = strrchr(name, '/');
	const char *dot;

	if (!fname)
		fname = name;
	if ((dot = strrchr(fname, '.')) != NULL)
	{
		dot++;

		if (!strcasecmp(dot, "wav")
		||  !strcasecmp(dot, "au")
		||  !strcasecmp(dot, "aif")
		||  !strcasecmp(dot, "aiff"))
			return 0.8;
	}
	return 0.0;
}


static int sndfile_stream_info (input_object *obj, stream_info *info)
{
	struct sf_local_data	*data;

	if (!obj || !info)
		return 0;

	data = (struct sf_local_data *) obj->local_data;

	if (data->sfhandle == NULL)
	{
		return 0;
	}


	strcpy(info->stream_type, "sndfile supported format");
	strcpy(info->status, "");
	strcpy(info->artist, "");
	strcpy(info->title, data->filename);

	return 1;
}

static int sndfile_init (void)
{
	return 1;
}

static void sndfile_shutdown (void)
{
	return;
}


static input_plugin sndfile_plugin;


input_plugin *input_plugin_info (void)
{
	memset(&sndfile_plugin, 0, sizeof(input_plugin));
	sndfile_plugin.version = INPUT_PLUGIN_VERSION;
	sndfile_plugin.name = "libsndfile plugin v0.1";
	sndfile_plugin.author = "Andy Lo A Foe";
	sndfile_plugin.init = sndfile_init;
	sndfile_plugin.shutdown = sndfile_shutdown;
	sndfile_plugin.can_handle = sndfile_can_handle;
	sndfile_plugin.open = sndfile_open;
	sndfile_plugin.close = sndfile_close;
	sndfile_plugin.play_frame = sndfile_play_frame;
	sndfile_plugin.frame_seek = sndfile_frame_seek;
	sndfile_plugin.frame_size = sndfile_frame_size;
	sndfile_plugin.nr_frames = sndfile_nr_frames;
	sndfile_plugin.frame_to_sec = sndfile_frame_to_sec;
	sndfile_plugin.sample_rate = sndfile_sample_rate;
	sndfile_plugin.channels = sndfile_channels;
	sndfile_plugin.stream_info = sndfile_stream_info;
	return &sndfile_plugin;
}
