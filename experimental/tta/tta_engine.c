/*
 *  tta_engine.cpp (C) 2005 by Peter Lemenkov <lemenkov@newmail.ru>
 *  version: 0.0.0.1
 *
 *  WARNING!!! This is a development version - DO NOT USE!
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 * p
typedef struct {
	FILE           *HANDLE;		// file handle
	unsigned int   FILESIZE;	// compressed size
	unsigned short NCH;		// number of channels
	unsigned short BPS;		// bits per sample
	unsigned short BSIZE;		// byte size
	unsigned short FORMAT;		// audio format
	unsigned int   SAMPLERATE;	// samplerate (sps)
	unsigned int   DATALENGTH;	// data length in samples
	unsigned int   FRAMELEN;	// frame length
	unsigned int   LENGTH;		// playback time (sec)
	unsigned int   STATE;		// return code
	unsigned int   DATAPOS;		// size of ID3v2 header
	unsigned int   BITRATE;		// average bitrate (kbps)
	double         COMPRESS;	// compression ratio
	id3_info       ID3;		// ID3 information
} tta_info;
 * */
#include <stdlib.h>
#include <string.h> /* memset */

#include <alsaplayer/alsaplayer_error.h>
#include <alsaplayer/input_plugin.h>
#include <alsaplayer/reader.h>

#include "ttalib.h"

static tta_info info;	// currently playing file info
#define BLOCK_SIZE (4608 * 2 * 2)	/* We can use any block size we like */
static char static_buffer[BLOCK_SIZE];
int flag = 0;

static int tta_init(void) 
{
	return 1;
}

static void tta_shutdown(void)
{
	return;
}

static float tta_can_handle(const char *path)
{
	tta_info temp;
	
	if (open_tta_file (path, &temp, 0) == 0) {
		printf("\nTTA Decoder OK - %s\n", get_error_str(temp.STATE));
		close_tta_file (&info);
		return 1.0;
	}
	
	printf("\nTTA Decoder Error - %s\n", get_error_str(temp.STATE));
	close_tta_file (&temp);	
	return 0.0;
}


static int tta_open(input_object *obj, const char *path)
{
	void* datasource = NULL;
	
	if ((datasource = reader_open(path, NULL, NULL)) == NULL) {
		return 0;
	}

	obj->flags = 0;
	if (reader_seekable ((reader_type*)datasource)) {
		obj->flags |= P_SEEK;
		obj->flags |= P_PERFECTSEEK;
		obj->flags |= P_FILEBASED;
	} else {
		obj->flags |= P_STREAMBASED;
	}	
	
	if (open_tta_file (path, &info, 0) < 0) {
		printf("\nTTA Decoder Error - %s\n", get_error_str(info.STATE));
		close_tta_file (&info);
		return 0;
	}

	/*************************/
	/* printf ("\n\n debug: \n >> NCH: %d\n >> BPS: %d\n >> SAMPLERATE: %d\n >> FRAMELEN: %d\n\n", info.NCH, info.BPS, info.SAMPLERATE, info.FRAMELEN);*/
	/*************************/
	obj->nr_channels = info.NCH;
	obj->nr_tracks = 1;
	
	obj->frame_size = (BLOCK_SIZE>>1);

	player_init(&info);

	return 1;
}

static void tta_close(input_object *obj)
{
	if (!obj)
		return;
	if (obj->local_data) {
		free(obj->local_data);
		obj->local_data = NULL;
	}
	/*******************/
	close_tta_file (&info);
	/*******************/
}

static long tta_frame_to_sec (input_object *obj, int frame)
{
	return  (100 * frame * (BLOCK_SIZE>>1)) / (info.SAMPLERATE * info.NCH * (info.BPS>>3));
}

static int tta_sample_rate(input_object *obj)
{
	return info.SAMPLERATE;
}

static int tta_channels(input_object *obj)
{
	return info.NCH;
}

static int tta_stream_info (input_object *obj, stream_info *inf)
{
	if (!obj || !inf)
		return 0;

	sprintf(inf->stream_type, "%d channels, %dHz %s",
		info.NCH,
		info.SAMPLERATE,
		"stereo"); /* TODO */
	strcpy(inf->status, "");
	strcpy(inf->artist, "");
	strcpy(inf->title, "");
	
	return 1;
}

static int tta_nr_frames(input_object *obj)
{
	/*
	printf ("[debug] ret: %d, datalen: %d\n", 
			(int) (info.DATALENGTH / (BLOCK_SIZE>>1) * info.NCH * (info.BPS>>3) + 0.5),
			info.DATALENGTH);
	*/
	return (int) (info.DATALENGTH / (BLOCK_SIZE>>1) * info.NCH * (info.BPS>>3) + 0.5);
}

static int tta_frame_size(input_object *obj)
{
	return (BLOCK_SIZE>>1);
}

static int tta_frame_seek(input_object *obj, int frame)
{
	set_position ((1000 * frame * (BLOCK_SIZE>>1)) / (info.SAMPLERATE * info.NCH * (info.BPS>>3) * SEEK_STEP));
	flag = 0;
	return 1;
}

static int tta_play_frame (input_object *obj, char *buf)
{
	if (!obj)
		return 0;

	memset (buf, 0, sizeof (char) * BLOCK_SIZE>>1);
	
	if (flag == 0){
		get_samples ((unsigned char*)static_buffer);
		memcpy (buf, static_buffer, BLOCK_SIZE>>1);
		flag = 1;
	}
	else{
		flag = 0;
		memcpy (buf, static_buffer + (BLOCK_SIZE>>1), BLOCK_SIZE>>1);
	}
	
	return 1;
}

static input_plugin tta_plugin;

#ifdef __cplusplus
extern "C" {
#endif

input_plugin *input_plugin_info (void)
{
	memset(&tta_plugin, 0, sizeof(input_plugin));

	tta_plugin.version 	= INPUT_PLUGIN_VERSION;
	tta_plugin.name 	= "TTA plugin ver. 0.0.0.1";
	tta_plugin.author 	= "Peter Lemenkov";
	tta_plugin.init 	= tta_init; 
	tta_plugin.shutdown 	= tta_shutdown; 
	tta_plugin.can_handle 	= tta_can_handle;
	tta_plugin.open 	= tta_open;
	tta_plugin.close 	= tta_close;
	tta_plugin.play_frame 	= tta_play_frame;  /* TODO */
	tta_plugin.frame_seek 	= tta_frame_seek; /* TODO */
	tta_plugin.frame_size 	= tta_frame_size; /* TODO */
	tta_plugin.nr_frames 	= tta_nr_frames;  /* TODO */
	tta_plugin.frame_to_sec = tta_frame_to_sec; /* TODO */
	tta_plugin.sample_rate 	= tta_sample_rate; /* TODO */
	tta_plugin.channels 	= tta_channels;  /* TODO */
	tta_plugin.stream_info 	= tta_stream_info; /* TODO */ 

	return &tta_plugin;
}

#ifdef __cplusplus
}
#endif

