/*  scope_plugin.h
 *  Copyright (C) 1999 Andy Lo A Foe <andy@alsa-project.org>
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
*/ 

#ifndef __scope_plugin_h__
#define __scope_plugin_h__

/*
 * Format of version number is 0x1000 + version
 * So 0x1001 is *binary* format version 1
 * THE VERSION NUMBER IS *NOT* A USER SERVICABLE PART!
 */

#define SCOPE_PLUGIN_VERSION    0x1001

#define SCOPE_NICE	15
#define SCOPE_SLEEP 20000
#define SCOPE_BG_RED	0
#define SCOPE_BG_BLUE	0
#define SCOPE_BG_GREEN	0

typedef int scope_version_type;
typedef int(*scope_init_type)();
typedef int(*scope_open_type)();
typedef void(*scope_start_type)(void *);
typedef int(*scope_running_type)();
typedef void(*scope_stop_type)();
typedef void(*scope_close_type)();
typedef void(*scope_set_data_type)(void *, int);

typedef struct _scope_plugin
{
	scope_version_type version;		
	char name[256];
	char author[256];
	scope_init_type init;
	scope_open_type open;
	scope_start_type start;
	scope_running_type running;
	scope_stop_type stop;
	scope_close_type close;
	scope_set_data_type set_data;
} scope_plugin;

typedef scope_plugin*(*scope_plugin_info_type)();

typedef struct _scope_entry
{
	scope_plugin *sp;
	struct _scope_entry *next;
	struct _scope_entry *prev;
	int active;
} scope_entry;

#endif
