/*  scope_plugin.h
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
 *
 *  $Id$
 *  
*/ 

#ifndef __scope_plugin_h__
#define __scope_plugin_h__

/*
 * Format of version number is 0x1000 + version
 * So 0x1001 is *binary* format version 1
 * THE VERSION NUMBER IS *NOT* A USER SERVICABLE PART!
 */

/**
 * The base version number of the scope plugin. Set at 0x1000
 */
#define SCOPE_PLUGIN_BASE_VERSION 0x1000

/**
 * The version of the scope plugin API. This should be increased
 * whenever structural changes are made to the API. This value should
 * only be changed by the maintainers.
 */
#define SCOPE_PLUGIN_VERSION    (SCOPE_PLUGIN_BASE_VERSION + 6)

/**
 * The default nice level scope plugins should be set at. Most scope plugins
 * are just eye candy and as such should not interfere with other processes
 * on your system. They should only use CPU cycles that would otherwise be
 * wasted. Setting the scopes to a nice level of 10 or higher pretty much
 * insures this. If you don't like this policy you can lower the value. Keep
 * in mind that negative values will only work if you run the HOST as root
 */ 
#define SCOPE_NICE	10

/**
 * The default sleep time in microseconds for scopes. After every render
 * iteration a scope should sleep for this amount of time. You should use the
 * dosleep() call i.e. dosleep(SCOPE_SLEEP). A value of 20000 will let scopes
 * run at 100000/20000 = 50 frames per second. If the scopes are consuming
 * too much CPU consider raising this value.
 */ 
#define SCOPE_SLEEP 20000

/**
 * The value of the RED component of the default background
 * color for scope windows. Value should be from 0-255
 */
#define SCOPE_BG_RED	0

/**
 * The value of the GREEN component of the default background
 * color for scope windows. Value should be from 0-255
 */
#define SCOPE_BG_GREEN	0

/**
 * The value of the BLUE component of the default background
 * color for scope windows. Value should be from 0-255
 */
#define SCOPE_BG_BLUE	0

/**
 * The API this scope was compiled against. It should always be set
 * to SCOPE_PLUGIN_VERSION. Failing to set this will most likely result
 * in a scope plugin that won't load.
 */
typedef int scope_version_type;

/**
 * The init function of a scope plugin. This function should initialize 
 * all data structures needed for the scope plugin. Return value should be
 * 1 on success, 0 if initialization fails.
 */
typedef int(*scope_init_type)(void);

/**
 * This function will be called when the HOST wants to activate the scope.
 * It should pop up the scope window and start rendering the PCM or FFT data
 */
typedef void(*scope_start_type)(void);

/**
 * This function should tell the HOST if the scope is running i.e. on-screen
 * and rendering. A value of 1 should be returned if this is the case, 0 if 
 * the scope is not active.
 */
typedef int(*scope_running_type)(void);

/**
 * This function should stop and close the scope window if it was running.
 * It should just return if the scope is not running.
 */
typedef void(*scope_stop_type)(void);

/**
 * The shutdown function is called just before the plugin is unloaded or just
 * before the HOST decides to exit. All data structures allocated in the init
 * routine should be freed here. 
 */
typedef void(*scope_shutdown_type)(void);

/**
 * @param buffer pointer to buffer data
 * @param count number of short (int16) samples in buffer
 * 
 * The set_data function should be defined if your scope wants to get it hands
 * on PCM data. The format of the buffer is short (int16) interleaved stereo
 * data. A count value of 1024 means there are 2048 short samples in the
 * buffer. These samples are interleaved, so  even sample positions are
 * from the left channel, uneven sample positions from the right channel.
 * The API will be changed to accommodate variable channels in the not too
 * distant future.
 */
typedef void(*scope_set_data_type)(void *buffer, int count);

/**
 * @param buffer buffer with FFT values
 * @param samples number of FFT values per channel
 * @param channels number of channels
 *
 * This function should be defined if your scope wants to get FFT data.
 * The HOST typically calculates 256 FFT values per channel (going from
 * low frequency range to high). The value is betwee 0-256. The buffer format 
 * is NON-interleaved int (int32). So if samples = 256 and channels = 2 then
 * there are 2 * 256 number of samples in the buffer. The first 256 are for
 * channel 1, the other 256 for channel 2.
 */
typedef void(*scope_set_fft_type)(void *buffer, int samples, int channels);

/**
 * You should declare a scope_plugin variable and populate it with pointers
 * of the specific functions implemented by your scope
 */
typedef struct _scope_plugin
{
	/**
	 * Set to SCOPE_PLUGIN_VERSION
	 */
	scope_version_type version;
	/**
	 * Point to a character array with the name of the scope
	 */
	char *name;
	/**
	 * Point to a character array with the name of the author(s) of
	 * the scope
	 */
	char *author;
	/**
	 * Pointer to a dlopen() handle. This is filled in by the HOST.
	 * Set to NULL.
	 */
	void *handle;
	/**
	 * Should point to the implentation of your init() function.
	 * Required by the HOST.
	 */
	scope_init_type init;
	/**
	 * Should point to the implementation of your start() function.
	 * Required by the HOST.
	 */
	scope_start_type start;
	/**
	 * Should point to the implementation of your running() function.
	 * Required by the HOST.
	 */
	scope_running_type running;
	/**
	 * Should point to the implementation of your stop() function.
	 * Required by the HOST.
	 */
	scope_stop_type stop;
	/**
	 * Should point to the implementation of your shutdown() function.
	 * Required by the HOST.
	 */
	scope_shutdown_type shutdown;
	/** Should point to the function that collects PCM data. If you
	 * don't want PCM data set to NULL.
	 */
	scope_set_data_type set_data;
	/** Should point to the function that collects FFT data. If you
	 * don't want FFT data set to NULL. NB. set_data and set_fft can't 
	 * both be NULL, at least one must be set.
	 */
	scope_set_fft_type set_fft;
} scope_plugin;

/**
 * Every scope plugin should have a scope_plugin_info() function that
 * returns a pointer to a scope_plugin structure that is filled with pointers 
 * to your function implementations.
 */
typedef scope_plugin*(*scope_plugin_info_type)(void);

#endif
