/*  input_plugin.h -  Use this to write input plugins
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#ifndef __input_plugin_h__
#define __input_plugin_h__

#include <pthread.h>

#define	P_SEEK		1
#define P_PERFECTSEEK	2
#define	P_REENTRANT 	4
#define P_TRACKS	8
#define P_FILEBASED	16
#define P_STREAMBASED	32

/*
 * Format of version number is 0x1000 + version
 * So 0x1001 is *binary* format version 1
 * THE VERSION NUMBER IS *NOT* A USER SERVICABLE PART!
 */

#define INPUT_PLUGIN_BASE_VERSION	0x1000
#define INPUT_PLUGIN_VERSION	(INPUT_PLUGIN_BASE_VERSION + 11)

/**
 * This is a structure that keeps frequently used parameters of the
 * an input instance. It also contains a pointer to any local_data
 * that might be allocated by the plugin itself.
 */ 
typedef struct _input_object
{
	int ready;			
	int flags;
	int nr_frames;
	int nr_tracks;
	int nr_channels;
	int frame_size;
	void *local_data;
	pthread_mutex_t	object_mutex;
} input_object;


/**
 * This structure is used to pass information about a stream/song from
 * the plugin to the host.
 */
typedef struct _stream_info
{
	char	stream_type[128];
	char	author[128];
	char	title[128];
	char	status[32];
} stream_info;

/* plugin binary version */
typedef int input_version_type;

/* capability flags for this plugin */
typedef int input_flags_type;

/* Init plugin */
typedef int(*input_init_type)();

/* Prepare the plugin for removal */
typedef void(*input_shutdown_type)();

/* Handle for plugin. Filled in by the host */
typedef void* input_plugin_handle_type;

/* Returns a rating between 0.0 and 1.0 for how
 * well this plugin can handle the given path
 * 1.0 = Excellent
 * 0.0 = Huh?
 */
typedef float(*input_can_handle_type)(const char *);

/* Open a source object */
typedef int(*input_open_type)(input_object *, char *);

/* Close, doh! */
typedef void(*input_close_type)(input_object *);

/* Play a single frame */
typedef int(*input_play_frame_type)(input_object *, char *buffer);

/* Seek to a specific frame number */
typedef int(*input_frame_seek_type)(input_object *,int);

/* Returns the frame size in bytes */
typedef int(*input_frame_size_type)(input_object *);

/* Number of frames */
typedef int(*input_nr_frames_type)(input_object *);

/* Frame to 100th of a second conversion (centiseconds) */
typedef  long(*input_frame_to_sec_type)(input_object *,int);

/* Returns the sample rate */
typedef int(*input_sample_rate_type)(input_object *);

/* Returns number of channels */
typedef int(*input_channels_type)(input_object *);

/* Return stream info */
typedef int(*input_stream_info_type)(input_object *,stream_info *);

/* Return number of tracks. Optional */
typedef int(*input_nr_tracks_type)(input_object *);

/* Seek to a track. Optional */
typedef int(*input_track_seek_type)(input_object *, int);


typedef struct _input_plugin
{
	input_version_type version;	
	input_flags_type	flags;
	char *name;
	char *author;
	void *handle;
	input_init_type init;
	input_shutdown_type shutdown;
	input_plugin_handle_type plugin_handle;
	input_can_handle_type can_handle;
	input_open_type open;
	input_close_type close;
	input_play_frame_type play_frame;
	input_frame_seek_type frame_seek;
	input_frame_size_type frame_size;
	input_nr_frames_type nr_frames;
	input_frame_to_sec_type frame_to_sec;
	input_sample_rate_type sample_rate;
	input_channels_type channels;
	input_stream_info_type stream_info;
	input_nr_tracks_type nr_tracks;
	input_track_seek_type track_seek;
} input_plugin;

typedef input_plugin*(*input_plugin_info_type)();

#endif
