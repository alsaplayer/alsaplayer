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

#define OUTPUT_PLUGIN_VERSION	0x1002

typedef int output_version_type;
typedef int(*output_init_type)();
typedef int(*output_open_type)(char *);
typedef void(*output_close_type)();
typedef int(*output_write_type)(void *data, int count);
typedef int(*output_set_buffer_type)(int, int, int);
typedef int(*output_set_sample_rate_type)(int);
typedef int(*output_get_queue_count_type)();
typedef int(*output_get_latency_type)();

typedef struct _output_plugin
{
	output_version_type version;
	char name[256];
	char author[256];
	output_init_type init;
	output_open_type open;
	output_close_type close;
	output_write_type write;
	output_set_buffer_type set_buffer;
	output_set_sample_rate_type set_sample_rate;
	output_get_queue_count_type get_queue_count;
	output_get_latency_type get_latency;
} output_plugin;

typedef output_plugin*(*output_plugin_info_type)();

#endif
