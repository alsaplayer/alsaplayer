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

#define MIKMOD_FRAME_SIZE	4608

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

	if (!(data->audio_buffer = (SBYTE *)malloc(MIKMOD_FRAME_SIZE))) {
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


static int mikmod_play_frame (input_object *obj, char *buf)
{
	struct mikmod_local_data *data =
		(struct mikmod_local_data *)obj->local_data;

	int length = 0;
	/*  printf ("playing frame\n"); */

	if (obj && obj->local_data == NULL) {
		printf("HUUUUUUUUUUUUUHHH??????????????????\n");
		return 0;
	}
	if (!Player_Active ())
		return 0;

	length = VC_WriteBytes((SBYTE *) data->audio_buffer, MIKMOD_FRAME_SIZE);

	if (buf)
		memcpy(buf, data->audio_buffer, MIKMOD_FRAME_SIZE);
	return 1;
}


static int mikmod_frame_seek (input_object *obj, int frame)
{
	/* printf ("unable to seek in modules!\n"); */
	return 0;
}


static int mikmod_frame_size (input_object *obj)
{
	return MIKMOD_FRAME_SIZE;
}


static int mikmod_nr_frames (input_object *obj)
{
	return 0;
}


static long mikmod_frame_to_sec (input_object *obj, int frame)
{
	return 0;
}


static int mikmod_sample_rate (input_object *obj)
{
	return 44100;
}


static int mikmod_channels (input_object *obj)
{
	return 2;
}


static int mikmod_stream_info (input_object *obj, stream_info *info)
{
	struct mikmod_local_data *data;

	if (!obj || !info)
		return 0;
	data = (struct mikmod_local_data *)obj->local_data;

	sprintf (info->stream_type, "%i channel %s", 
			data->mf->numchn, data->mf->modtype);

	info->artist[0] = 0;
	strcpy (info->status, "No time data");
	strcpy (info->title, (data->mf->songname[0]) ? data->mf->songname : data->fname);

	return 1;
}


input_plugin mikmod_plugin = {
	INPUT_PLUGIN_VERSION,
	0,
	"MikMod player v1.0",
	"Paul Fisher <rao@gnu.org>",
	NULL,
	mikmod_init,
	mikmod_shutdown,
	NULL,
	mikmod_can_handle,
	mikmod_open,
	mikmod_close,
	mikmod_play_frame,
	mikmod_frame_seek,
	mikmod_frame_size,
	mikmod_nr_frames,
	mikmod_frame_to_sec,
	mikmod_sample_rate,
	mikmod_channels,
	mikmod_stream_info,
	NULL,
	NULL
};


input_plugin *input_plugin_info (void)
{
	return &mikmod_plugin;
}
