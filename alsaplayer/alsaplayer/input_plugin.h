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

/**
 * Set this flag if your plugin is able to seek in the stream
 */
#define	P_SEEK		1

/** Set this flag if your plugin is able to do sample accurate seeking
 * in the stream. This is required for reverse speed playback.
 */
#define P_PERFECTSEEK	2

/**
 * Set this flag if your plugin is reentrant.
 */
#define	P_REENTRANT 	4

/**
 * Set this flag if the stream is file based (local disk file)
 */
#define P_FILEBASED	8

/**
 * Set this if the stream is a real stream e.g. HTTP or UDP based
 */
#define P_STREAMBASED	16

/**
 * Set minimal buffer 
 */
#define P_BUFFERING	32

/*
 * Format of version number is 0x1000 + version
 * So 0x1001 is *binary* format version 1
 * THE VERSION NUMBER IS *NOT* A USER SERVICABLE PART!
 */

/**
 * The base version number of the scope plugin. Set at 0x1000.
 */
#define INPUT_PLUGIN_BASE_VERSION	0x1000

/**
 *  The version of the input plugin API. This should be incremented
 *  whenever structural changes are made to the API. This value should
 *  only be changed by the maintainers.
 */
#define INPUT_PLUGIN_VERSION	(INPUT_PLUGIN_BASE_VERSION + 16)

/**
 * This is a structure that keeps frequently used parameters of an
 * input instance. It also contains a pointer to any local_data
 * that might be allocated by the plugin itself.
 */ 
typedef struct _input_object
{
	/**
	 * Flag that should be set to 1 if your plugin is ready to accept
	 * play_frame() callback
	 */
	int ready;
	/**
	 * Stream specific flags that should be set in the open() call.
	 * Read the description of the P_* definitions for details.
	 */
	int flags;
	/**
	 * The total number of frames in the stream. Should be set in the
	 * open() call.
	 */
	int nr_frames;
	/**
	 * The number of tracks, if any, in the stream. Should be set in
	 * the open() call.
	 */
	int nr_tracks;
	/**
	 * The number of PCM channels in the stream. Should always be 2
	 * at this time.
	 */
	int nr_channels;
	/**
	 * The frame size in bytes. play_frame() will be called with this
	 * value.
	 */
	int frame_size;
	/** If your plugin needs extra space for its own variables assign the
	 * allocated data structure to this pointer
	 */
	void *local_data;
	/** Path of the currently played file
	 * 
	 */
	char* path;
	/**
	 * The object mutex. Used to lock and unlock the data structures.
	 * Initialized and called from the HOST.
	 */
	pthread_mutex_t	object_mutex;
} input_object;


/**
 * This structure is used to pass information about a stream/song from
 * the plugin to the host.
 */
typedef struct _stream_info
{
	/**
	 * Should describe what type of stream this is (MP3, OGG, etc). May
	 * also contain format data and things like sample rate. Text
	 */
	char	stream_type[128];
	/** 
	 * Author of the stream. Usually the name of the Artist or Band
	 */
	char	artist[128];
	/**
	 * The song title.
	 */
	char	title[128];
	/**
	 * The album name.
	 */
	char	album[128];
	/**
	 * The genre of this song
	 */
	char	genre[128];
	/**
	 * The year of this song
	 */
	char	year[10];
	/**
	 * The track number of this song
	 */
	char	track[10];
	/**
	 * The comment of this song
	 */
	char	comment[128];
	/**
	 * The status of the plugin. Can have something like "Seeking..."
	 * or perhaps "Buffering" depending on what the plugin instance is
	 * doing.
	 */
	char	status[32];
	/**
	 * The path of the stream
	 */
	char	path[1024];
	/**
	 * The number of channels 
	 */
	int	channels;
	/**
	 * The number of tracks
	 */
	int	tracks;
	/**
	 * The current track;
	 */
	int 	current_track;
	/**
	 * The sampling rate
	 */
	int	sample_rate;
	/**
	 * The bitrate
	 */
	int	bitrate;
} stream_info;

/** 
 * input plugin binary version. Must be set to INPUT_PLUGIN_VERSION 
 */
typedef int input_version_type;

/**
 * Capability flags for this plugin
 **/
typedef int input_flags_type;

/**
 * Init plugin 
 */
typedef int(*input_init_type)(void);

/**
 * Prepare the plugin for removal
 */
typedef void(*input_shutdown_type)(void);

/**
 * Handle for plugin. Filled in by the host
 */
typedef void* input_plugin_handle_type;

/**
 * @param path Path to stream
 *
 * Returns a rating between 0.0 and 1.0 for how
 * well this plugin can handle the given path
 * 1.0 = Excellent
 * 0.0 = Huh?
 */
typedef float(*input_can_handle_type)(const char *path);

/**
 * @param obj input object
 * @param path path of stream to open
 *
 * Open stream */
typedef int(*input_open_type)(input_object *obj, const char *path);

/**
 * @param obj input object
 * 
 * Close stream */
typedef void(*input_close_type)(input_object *obj);

/**
 * @param obj input object
 * @param buffer buffer where we should write the frame to
 *
 * Play/decode a single frame. This function should write exactly one frame
 * to the buffer. If there is not enough PCM data to fill the frame
 * it should be padded with zeros (silence).
 */
typedef int(*input_play_frame_type)(input_object *obj, char *buffer);

/**
 * @param obj input object
 * @param frame
 *
 * Seek to a specific frame number
 */
typedef int(*input_frame_seek_type)(input_object *obj, int frame);

/**
 * @param obj input object
 *
 * Returns the frame size in bytes
 */
typedef int(*input_frame_size_type)(input_object *obj);

/**
 * @param obj input object
 *
 * Returns the total number of frames in the stream */
typedef int(*input_nr_frames_type)(input_object *obj);

/**
 * @param obj input object
 * @param frame frame number
 *
 * Returns the offset from the start time in centiseconds (100th of a second)
 * for the frame given.  
 */
typedef  long(*input_frame_to_sec_type)(input_object *obj ,int frame);

/**
 * @param obj input object
 *
 * Returns the sample rate of the stream
 */
typedef int(*input_sample_rate_type)(input_object *obj);

/**
 * @param obj input object
 *
 * Returns number of channels in the stream
 */
typedef int(*input_channels_type)(input_object *obj);

/**
 * @param obj input object
 * @param info pointer to stream_info structure
 *
 * Return stream info of the current stream. You should not allocate
 * space for the stream_info structure. The HOST will take care of that.
 */
typedef int(*input_stream_info_type)(input_object *obj,stream_info *info);

/**
 * @param obj input object
 *
 * Return number of tracks. Optional */
typedef int(*input_nr_tracks_type)(input_object *obj);

/* @param obj input object
 * @param track track to seek to
 * 
 * Seek to a track. Optional
 */
typedef int(*input_track_seek_type)(input_object *obj, int track);


typedef struct _input_plugin
{
	/**
	 * Must be set to INPUT_PLUGIN_VERSION
	 */
	input_version_type version;
	/**
	 * Fixed flags for the plugin (P_*)
	 */
	input_flags_type	flags;
	/**
	 * Should point the a character array containing the name of this plugin
	 */
	char *name;
	/** 
	 * Should point to a character array containing the name of the 
	 * author(s) of this plugin.
	 */
	char *author;
	/**
	 * dlopen() handle of this plugin. Filled in by the HOST.
	 */
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

/**
 * Every input plugin should have an input_plugin_info() function that
 * returns a pointer to an input_plugin structure that is set up with
 * pointers to your implementations. If your plugin is compiled using
 * C++ make sure you 'extern "C"' the input_plugin_info() function or
 * else the HOST will not be able to load the plugin.
 */
typedef input_plugin*(*input_plugin_info_type)(void);

#endif
