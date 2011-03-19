
/*  stream_info.h
 *  Copyright (C) 2007 Peter Lemenkov <lemenkov@gmail.com>
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

#ifndef __stream_info_h__
#define __stream_info_h__

#define SI_MAX_FIELD_LEN 128

// some arch don't define PATH_MAX
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/**
 * This structure is used to describe a stream/song 
 */
typedef struct _stream_info
{
	/**
	 * Should describe what type of stream this is (MP3, OGG, etc). May
	 * also contain format data and things like sample rate. Text
	 */
	char	stream_type[SI_MAX_FIELD_LEN+1];
	/** 
	 * Author of the stream. Usually the name of the Artist or Band
	 */
	char	artist[SI_MAX_FIELD_LEN+1];
	/**
	 * The song title.
	 */
	char	title[SI_MAX_FIELD_LEN+1];
	/**
	 * The album name.
	 */
	char	album[SI_MAX_FIELD_LEN+1];
	/**
	 * The genre of this song
	 */
	char	genre[SI_MAX_FIELD_LEN+1];
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
	char	comment[SI_MAX_FIELD_LEN+1];
	/**
	 * The status of the plugin. Can have something like "Seeking..."
	 * or perhaps "Buffering" depending on what the plugin instance is
	 * doing.
	 */
	char	status[SI_MAX_FIELD_LEN+1];
	/**
	 * The path of the stream
	 */
	char	path[PATH_MAX+1];
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

#endif // __stream_info_h__
