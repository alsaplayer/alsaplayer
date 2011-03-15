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
static const int BLOCK_SAMPLES = 256;

struct sf_local_data
{
	SNDFILE* sfhandle;
	SF_INFO	 sfinfo;
	int blocksize;
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
	obj->nr_blocks = 0;

	data = (struct sf_local_data *) obj->local_data;

	data->sfhandle = sf_open(name, SFM_READ, &data->sfinfo);

	if (data->sfhandle == NULL)
	{
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	}

	if (data->sfinfo.channels > 2)
	{
		alsaplayer_error("sndfile_engine: no support for 2+ channels");
		sf_close(data->sfhandle);
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	}

	data->blocksize = BLOCK_SAMPLES;

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


static int sndfile_play_block (input_object *obj, short *buf)
{
	size_t	samples_to_read;
	size_t	items_read;
	size_t	samples;
	size_t	i;
	short	*buffer;

	struct sf_local_data	*data;

	if (!obj)
		return 0;

	data = (struct sf_local_data *) obj->local_data;

	if (!data)
		return 0;

	buffer = alloca(BLOCK_SAMPLES * sizeof (buffer [0]));

	if (!buffer)
		return 0;

	if (data->sfinfo.channels == 1) { /* Mono, so double */
		short *dest;
		short *src;

		samples_to_read = BLOCK_SAMPLES / 2 ;
		items_read = sf_read_short(data->sfhandle, buffer, samples_to_read);

		if (buf) {
			src = buffer;
			dest = buf;
			for (i = 0; i < items_read; i++) {
				dest [2 * i] = src [i];
				dest [2 * i + 1] =	src [i];
			}
			if (items_read < samples_to_read) {
				return 0;
			}
		}
	} else {
		samples_to_read = BLOCK_SAMPLES;

		items_read = sf_read_short(data->sfhandle, buffer, samples_to_read);
		if (buf)
			memcpy(buf, buffer, BLOCK_SAMPLES * sizeof (buffer [0]));
		else
			return 0;
		if (items_read < samples_to_read)
			return 0;
	}
	return 1;
}


static int sndfile_block_seek (input_object *obj, int block)
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
	pos = (block * BLOCK_SAMPLES) / data->sfinfo.channels ;
	//alsaplayer_error("pos = %d", pos);
	if (sf_seek(data->sfhandle, pos, SEEK_SET) != pos)
		return 0;
	return block;
}


static int sndfile_nr_blocks (input_object *obj)
{
	struct sf_local_data    *data;
	sf_count_t samples;

	if (!obj)
		return 0;
	data = (struct sf_local_data *) obj->local_data;
	samples = data->sfinfo.frames;

	if (samples > 0)  {
		return ((int)data->sfinfo.frames * 2 / BLOCK_SAMPLES);
	}
	return 0;
}


static int sndfile_block_size (input_object *obj)
{
	return BLOCK_SAMPLES;
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


static  long sndfile_block_to_sec (input_object *obj, int block)
{
	unsigned long	result = 0;

	struct sf_local_data	*data;

	if (!obj)
		return result;

	data = (struct sf_local_data *) obj->local_data;

	if (!data)
		return result;
	result = (unsigned long) (block * BLOCK_SAMPLES /
			(data->sfinfo.samplerate * sizeof (short) / 100));

	return result;
}

static float sndfile_can_handle (const char *name)
{
	const char *fname = strrchr(name, '/');
	const char *dot;

	/* sndfile input plugin doesn't handle streaming. */
	if (strncasecmp (name, "http://", 7) == 0)
		return 0.0;

	if (!fname)
		fname = name;
	if ((dot = strrchr(fname, '.')) != NULL)
	{
		dot++;

		if (!strcasecmp(dot, "aif")
			|| !strcasecmp(dot, "aiff")
			|| !strcasecmp(dot, "au")
			|| !strcasecmp(dot, "svx")
			|| !strcasecmp(dot, "voc")
			|| !strcasecmp(dot, "wav")
			)
			return 1.0;

		if (strcasecmp(dot, "flac") == 0)
		{
			SNDFILE *file;
			SF_INFO info;
			memset (&info, 0, sizeof (info));
			file = sf_open (name, SFM_READ, &info);
			sf_close (file);
			return file ? 1.0 : 0.0 ;
		}

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
	sndfile_plugin.play_block = sndfile_play_block;
	sndfile_plugin.block_seek = sndfile_block_seek;
	sndfile_plugin.block_size = sndfile_block_size;
	sndfile_plugin.nr_blocks = sndfile_nr_blocks;
	sndfile_plugin.block_to_sec = sndfile_block_to_sec;
	sndfile_plugin.sample_rate = sndfile_sample_rate;
	sndfile_plugin.channels = sndfile_channels;
	sndfile_plugin.stream_info = sndfile_stream_info;
	return &sndfile_plugin;
}
