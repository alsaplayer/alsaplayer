/*  output_plugin.h
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

#ifndef __output_plugin_h__
#define __output_plugin_h__

#define OUTPUT_PLUGIN_BASE_VERSION	0x1000
#define OUTPUT_PLUGIN_VERSION	(OUTPUT_PLUGIN_BASE_VERSION + 4)

typedef int output_version_type;
typedef int(*output_init_type)();
typedef int(*output_open_type)(char *path);
typedef void(*output_close_type)();
typedef int(*output_write_type)(void *data, int byte_count);
typedef int(*output_start_callbacks_type)(void *data);
typedef int(*output_set_buffer_type)(int frag_size, int frag_count, int channels);
typedef int(*output_set_sample_rate_type)(int rate);
typedef int(*output_get_queue_count_type)();
typedef int(*output_get_latency_type)();

typedef struct _output_plugin
{
	/**
	 * Version of output plugin. Must be OUTPUT_PLUGIN_VERSION
	 */ 
	output_version_type version;

	/**
	 * Name of output plugin
	 */ 
	char *name;

	/**
	 * Author of the plugin
	 */ 
	char *author;

	/**
	 * Initialize output plugin. Called before the plugin is 
	 * opened for use
	 */
	output_init_type init;

	/**
	 * @param path The path or device designation that should be used
	 *
	 * Opens the output plugin. A value of 1 should be returned on 
	 * success, 0 on failure.
	 */
	output_open_type open;

	/**
	 * Close the output plugin
	 */ 
	output_close_type close;

	/**
	 * @param data Buffer that contains the data
	 * @byte_count Number of bytes that should be read from the buffer
	 *
	 * Write out data to the output device. This is a byte count and
	 * will typically be the same size as a fragment. A value of 1 should
	 * be returned on success, 0 on failure.
	 */ 
	output_write_type write;

	/**
	 * @param data pointer to bufs structure in AlsaNode
	 *
	 * This function is used for callback based plugins like JACK
	 */
	output_start_callbacks_type start_callbacks;
	
	/**
	 * @param frag_size Fragment size to use (in bytes)
	 * @param frag_count Fragment count to use (in bytes)
	 * @param channels Number of channels to use
	 *
	 * Set up the output device with the given parameters. Some output
	 * devices do not accept such configurations in which case they
	 * should just be ignored, but still expect frag_size data chunks
	 * in the write function. A value of 1 should be returned on success,
	 * 0 on failure.
	 */
	output_set_buffer_type set_buffer;
	
	/**
	 * @param rate Sample rate to use
	 *
	 * Set the sample rate of the output device. A value of 1 should be
	 * returned on success, 0 on failure.
	 */
	output_set_sample_rate_type set_sample_rate;

	/**
	 * Returns the number of bytes pending in the hardware buffer of
	 * output device. This function is optional.
	 */
	output_get_queue_count_type get_queue_count;
	
	/**
	 * Returns the latency of the output device in bytes. This function
	 * is optional.
	 */ 
	output_get_latency_type get_latency;
} output_plugin;

typedef output_plugin*(*output_plugin_info_type)();

#endif
