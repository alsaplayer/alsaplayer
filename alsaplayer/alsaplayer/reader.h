/*  reader.h
 *  Copyright (C) 2002 Evgeny Chukreev <codedj@echo.ru>
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
 *  $Id$
 *
*/ 
#ifndef __READER_H__
#define __READER_H__

#include <stdio.h>
   
/*
 * Format of version number is 0x1000 + version
 * So 0x1001 is *binary* format version 1
 * THE VERSION NUMBER IS *NOT* A USER SERVICABLE PART!
 */

/**
 * The base version number of the reader plugin. Set at 0x1000.
 */
#define READER_PLUGIN_BASE_VERSION	0x1000

/**
 *  The version of the reader plugin API. This should be incremented
 *  whenever structural changes are made to the API. This value should
 *  only be changed by the maintainers.
 */
#define READER_PLUGIN_VERSION	(READER_PLUGIN_BASE_VERSION + 2)

/** 
 * reader plugin binary version. Must be set to READER_PLUGIN_VERSION 
 */
typedef int reader_version_type;

/**
 * Init plugin 
 */
typedef int (*reader_init_type)();   

/**
 * Prepare the plugin for removal
 */
typedef void(*reader_shutdown_type)();

/**
 * @param uri URI to stream
 *
 * Returns a rating between 0.0 and 1.0 for how
 * well this plugin can handle the given URI
 * 1.0 = Excellent
 * 0.0 = Huh?
 */
typedef float (*reader_can_handle_type)(const char *uri);

/**
 * @param uri URI of stream to open
 *
 * Open stream */
typedef void *(*reader_open_type)(const char *uri);

/**
 * @param d stream descriptor (returned by reader_open_type function).
 * 
 * Close stream */
typedef void(*reader_close_type)(void *d);

/* TODO: describe */
typedef size_t (*reader_read_type)(void *ptr, size_t size, void *d);
typedef int (*reader_seek_type)(void *d, long offset, int whence);
typedef long (*reader_tell_type)(void *d);
typedef float (*reader_can_expand_type)(const char *uri);
typedef char **(*reader_expand_type)(const char *uri);

typedef struct _reader_plugin
{
	/**
	 * Must be set to INPUT_PLUGIN_VERSION
	 */
	reader_version_type version;
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

	reader_init_type init;
	reader_shutdown_type shutdown;
	reader_can_handle_type can_handle;
	reader_open_type open;
	reader_close_type close;
	reader_read_type read;
	reader_seek_type seek;
	reader_tell_type tell;
	reader_can_expand_type can_expand;
	reader_expand_type expand;
} reader_plugin;
  
typedef struct _reader_type {
    void *fd;
    reader_plugin *plugin;
} reader_type;
 
void reader_init (void);

/* ******************************** API for input plugins ************** */
#ifdef __cplusplus
extern "C" {
#endif

reader_type *reader_open (const char *uri);
int reader_close (reader_type *h);

size_t reader_read (void *ptr, size_t size, reader_type *h);
int reader_seek (reader_type *h, long offset, int whence);
long reader_tell (reader_type *h);

char **reader_expand (const char *uri);
void reader_free_expanded (char **list);

#ifdef __cplusplus
}
#endif
/* ********************************************************************* */

/**
 * Every reader plugin should have an reader_plugin_info() function that
 * returns a pointer to an reader_plugin structure that is set up with
 * pointers to your implementations. If your plugin is compiled using
 * C++ make sure you 'extern "C"' the reader_plugin_info() function or
 * else the HOST will not be able to load the plugin.
 */
typedef reader_plugin *(*reader_plugin_info_type)();

#endif
