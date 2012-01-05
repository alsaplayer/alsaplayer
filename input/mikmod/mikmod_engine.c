/*
 * mikmod_engine.c
 * Copyright (C) 1999 Paul N. Fisher <rao@gnu.org>
 * Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#include <unistd.h>
#include <string.h>
#include <mikmod.h>

#include "input_plugin.h"
#include "ap_string.h"
#include "ap_unused.h"

#define MIKMOD_BLOCK_SIZE	4608

extern MDRIVER drv_alsaplayer;

struct mikmod_local_data {
	MODULE *mf;
	char *fname;
	SBYTE *audio_buffer;
};

/* Unfortunately libmikmod is *NOT* reentrant :-( */
static pthread_mutex_t mikmod_mutex;

static int mikmod_init (void)
{
	static int inited = 0;
	if (inited) return 1;

	MikMod_RegisterDriver (&drv_alsaplayer);
	MikMod_RegisterAllLoaders();

	md_reverb = 0;
	md_mode = (DMODE_SOFT_MUSIC | DMODE_SOFT_SNDFX | DMODE_STEREO | DMODE_16BITS
			/*| DMODE_INTERP */);

	if (MikMod_Init ("")) {
		fprintf (stderr, "Could not initialize mikmod, reason: %s\n",
				MikMod_strerror (MikMod_errno));
		return 0;
	}

	pthread_mutex_init(&mikmod_mutex, NULL);

	inited = 1;
	return 1;
}


static void mikmod_shutdown(void)
{
	return;
}


static float mikmod_can_handle (const char *name)
{
	char *ext;

	ext = strrchr (name, '.');
	if (!ext)
		return 0.0;
	ext++;

	if (strstr(name, "MOD.") ||
			strstr(name, "mod.") ||
			strstr(name, "xm.") ||
			strstr(name, "XM.")) {
		return 1.0;
	}
	if (!strcasecmp (ext, "669") ||
			!strcasecmp (ext, "amf") ||
			!strcasecmp (ext, "dsm") ||
			!strcasecmp (ext, "far") ||
			!strcasecmp (ext, "gdm") ||
			!strcasecmp (ext, "imf") ||
			!strcasecmp (ext, "it")  ||
			!strcasecmp (ext, "med") ||
			!strcasecmp (ext, "mod") ||
			!strcasecmp (ext, "mtm") ||
			!strcasecmp (ext, "s3m") ||
			!strcasecmp (ext, "stm") ||
			!strcasecmp (ext, "stx") ||
			!strcasecmp (ext, "ult") ||
			!strcasecmp (ext, "uni") ||
			!strcasecmp (ext, "xm")) {
		return 1.0;
	}
	return 0.0;
}


static int mikmod_open (input_object *obj, const char *name)
{
	MODULE *mf = NULL;
	struct mikmod_local_data *data;

	if (pthread_mutex_trylock(&mikmod_mutex) != 0)  {
		/* printf("mikmod already in use :(\n"); */
		obj->local_data = NULL;
		return 0;
	}

	if (!(mf = Player_Load ((char *)name, 255, 0))) {
		printf ("error loading module: %s\n", name);
		obj->local_data = NULL;
		pthread_mutex_unlock(&mikmod_mutex);
		return 0;
	}
	obj->local_data = malloc(sizeof(struct mikmod_local_data));

	if (!obj->local_data) {
		Player_Free(mf);
		pthread_mutex_unlock(&mikmod_mutex);
		return 0;
	}
	data = (struct mikmod_local_data *)obj->local_data;

	if (!(data->audio_buffer = malloc(MIKMOD_BLOCK_SIZE))) {
		Player_Free(mf);
		free(obj->local_data);
		obj->local_data = NULL;
		pthread_mutex_unlock(&mikmod_mutex);
		return 0;
	}

	data->fname = strrchr(name, '/');
	data->fname = (data->fname) ? data->fname + 1 : (char *)name;
	data->mf = mf;
	Player_Start (data->mf);

	obj->flags = 0;

	return 1;
}


static void mikmod_close (input_object *obj)
{
	struct mikmod_local_data *data = (struct mikmod_local_data *)obj->local_data;
	if (data) {
		Player_Stop ();
		Player_Free (data->mf);
		free(data->audio_buffer);
		free(obj->local_data);
		obj->local_data = NULL;
		pthread_mutex_unlock(&mikmod_mutex);
	}
}


static int mikmod_play_block (input_object *obj, short *buf)
{
	struct mikmod_local_data *data =
		(struct mikmod_local_data *)obj->local_data;

	if (obj && obj->local_data == NULL) {
		printf("HUUUUUUUUUUUUUHHH??????????????????\n");
		return 0;
	}
	if (!Player_Active ())
		return 0;

	VC_WriteBytes(data->audio_buffer, MIKMOD_BLOCK_SIZE);

	if (buf)
		memcpy(buf, data->audio_buffer, MIKMOD_BLOCK_SIZE);
	return 1;
}


static int mikmod_block_seek (input_object * UNUSED (obj), int UNUSED (block))
{
	/* printf ("unable to seek in modules!\n"); */
	return 0;
}


static int mikmod_block_size (input_object * UNUSED (obj))
{
	return MIKMOD_BLOCK_SIZE / sizeof (short);
}


static int mikmod_nr_blocks (input_object * UNUSED (obj))
{
	return 0;
}

static int64_t mikmod_frame_count (input_object * UNUSED (obj))
{
	return -1;
}


static long mikmod_block_to_sec (input_object * UNUSED (obj), int UNUSED (block))
{
	return 0;
}


static int mikmod_sample_rate (input_object * UNUSED (obj))
{
	return 44100;
}


static int mikmod_channels (input_object * UNUSED (obj))
{
	return 2;
}


static int mikmod_stream_info (input_object *obj, stream_info *info)
{
	struct mikmod_local_data *data;

	if (!obj || !info)
		return 0;
	data = (struct mikmod_local_data *)obj->local_data;

	snprintf (info->stream_type, sizeof (info->stream_type), "%i channel %s",
			data->mf->numchn, data->mf->modtype);

	info->artist[0] = 0;
	ap_strlcpy (info->status, "No time data", sizeof (info->status));
	ap_strlcpy (info->title, (data->mf->songname[0]) ? data->mf->songname : data->fname, sizeof (info->title));

	return 1;
}

static input_plugin mikmod_plugin;


input_plugin *input_plugin_info (void)
{
	memset (&mikmod_plugin, 0, sizeof (mikmod_plugin));

	mikmod_plugin.version = INPUT_PLUGIN_VERSION;
	mikmod_plugin.name = "MikMod player v1.0";
	mikmod_plugin.author = "Paul Fisher <rao@gnu.org>";
	mikmod_plugin.init = mikmod_init;
	mikmod_plugin.shutdown = mikmod_shutdown;
	mikmod_plugin.can_handle = mikmod_can_handle;
	mikmod_plugin.open = mikmod_open;
	mikmod_plugin.close = mikmod_close;
	mikmod_plugin.play_block = mikmod_play_block;
	mikmod_plugin.block_seek = mikmod_block_seek;
	mikmod_plugin.block_size = mikmod_block_size;
	mikmod_plugin.nr_blocks = mikmod_nr_blocks;
	mikmod_plugin.frame_count = mikmod_frame_count;
	mikmod_plugin.block_to_sec = mikmod_block_to_sec;
	mikmod_plugin.sample_rate = mikmod_sample_rate;
	mikmod_plugin.channels = mikmod_channels;
	mikmod_plugin.stream_info = mikmod_stream_info;

	return &mikmod_plugin;
}
