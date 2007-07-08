/*  interface_plugin.h
 *  Copyright (C) 2001-2002 Andy Lo A Foe <andy@alsaplayer.org>
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
 * $Id$
 * 
 */ 

#ifndef __interface_plugin_h__
#define __interface_plugin_h__

#include "CorePlayer.h"
#include "Playlist.h"

/*
 * Format of version number is 0x1000 + version
 * So 0x1001 is *binary* format version 1
 * THE VERSION NUMBER IS *NOT* A USER SERVICABLE PART!
 */

#define INTERFACE_PLUGIN_BASE_VERSION	0x1000
#define INTERFACE_PLUGIN_VERSION	(INTERFACE_PLUGIN_BASE_VERSION + 4)

typedef int interface_version_type;
typedef int(*interface_init_type)();
typedef int(*interface_start_type)(Playlist *, int,  char **);
typedef int(*interface_running_type)();
typedef int(*interface_stop_type)();
typedef void(*interface_close_type)();

typedef struct _interface_plugin
{
	interface_version_type version;		
	char *name;
	char *author;
	void *handle;
	interface_init_type init;
	interface_start_type start;
	interface_running_type running;
	interface_stop_type stop;
	interface_close_type close;
} interface_plugin;

typedef interface_plugin*(*interface_plugin_info_type)();

#endif
