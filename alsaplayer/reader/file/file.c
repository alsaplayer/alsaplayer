/*   file.c
 *   Copyright (C) 2002 Evgeny Chukreev <codedj@echo.ru>
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

#include <malloc.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "reader.h"
#include "string.h"

/* open stream */
static void *file_open(const char *uri)
{
    FILE *f;

    f = fopen (&uri[5], "r");
    if (!f)  return NULL;
    
    return f;
}

/* close stream */
static void file_close(void *d)
{
    fclose ((FILE*)d);
}

/* test function */
static float file_can_handle(const char *uri)
{
    struct stat buf;
    
    /* Check for prefix */
    if (strncmp (uri, "file:", 5))  return 0.0;
    
    if (stat(&uri[5], &buf))   return 0.0;
    
    /* Is it a type we might have a chance of reading?
     * (Some plugins may cope with playing special devices, eg, a /dev/scd) */
    if (!(S_ISREG(buf.st_mode) ||
	  S_ISCHR(buf.st_mode) ||
	  S_ISBLK(buf.st_mode) ||
	  S_ISFIFO(buf.st_mode) ||
	  S_ISSOCK(buf.st_mode))) return 0.0;
    
    return 1.0;
}

/* init plugin */
static int file_init()
{
    return 1;
}

/* shutdown plugin */
static void file_shutdown()
{
    return;
}

/* read from stream */
static size_t file_read (void *ptr, size_t size, void *d)
{
    return fread (ptr, 1, size, (FILE*)d) ;
}

/* seek in stream */
static int file_seek (void *d, long offset, int whence)
{
    return fseek ((FILE*)d, offset, whence);
}

/* directory test */
static float file_can_expand (const char *uri)
{
    const char *path;
    struct stat buf;
    
    /* Check for prefix */
    if (strncmp (uri, "file:", 5))  return 0.0;
 
    /* Real path */
    path = &uri[5];
    if (!*path)  return 0.0;

    // Stat file, and don't follow symlinks
    if (lstat(path, &buf))  return 0.0;
    if (!S_ISDIR(buf.st_mode))  return 0.0;
    
    return 1.0;
}

/* expand directory */
static char **file_expand (const char *uri)
{
    struct dirent *entry;
    DIR *dir = opendir (&uri[5]);
    char **expanded = NULL;
    int count = 0;
    char *s;
   
    /* Allocate memory for empty list */
    expanded = malloc (sizeof(char*));
    *expanded = NULL;
   
    /* return empty list on error */
    if (!dir)  return expanded;
       
    /* iterate over a dir */
    while ((entry = readdir(dir)) != NULL) {
	/* don't include . and .. entries */
	if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)  continue;

	/* compose */
	s = malloc (sizeof(char) * (2 + strlen(&uri[5]) + strlen(entry->d_name)));
	strcpy (s, &uri[5]);
	strcat (s, "/");
	strcat (s, entry->d_name);
	expanded [count++] = s;

	/* grow up our list */
	expanded = realloc (expanded, (count+1)*sizeof(char*));
    }
    
    /* set the end mark */
    expanded [count] = NULL;
    
    closedir(dir);  

    return expanded;
}

/* info about this plugin */
reader_plugin file_plugin = {
	READER_PLUGIN_VERSION,
	"File reader v1.0",
	"Evgeny Chukreev",
	NULL,
	file_init,
	file_shutdown,
	file_can_handle,
	file_open,
	file_close,
	file_read,
	file_seek,
	file_can_expand,
	file_expand
};

/* return info about this plugin */
reader_plugin *reader_plugin_info(void)
{
	return &file_plugin;
}
