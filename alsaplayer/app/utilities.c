/*  utilities.c
 *  Copyright (C) 1999 Richard Boulton <richard@tartarus.org>
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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "utilities.h"
#include "alsaplayer_error.h"

/* Threads and usleep does not work, select is very portable */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
void dosleep(unsigned int msec)
{
	struct timeval time;
	time.tv_sec=0;
	time.tv_usec=msec;

	select(0, NULL, NULL, NULL, &time);
}

void parse_file_uri_free(char *p)
{
	if (p)
		free(p);
}

char *get_homedir()
{
	char *homedir = NULL;

	if ((homedir = getenv("HOME")) == NULL) {
		homedir = strdup("/tmp");
	}
	return homedir;
}

char *get_prefsdir()
{
	static char *prefs_path;
	static int prefs_path_init = 0;
	char *homedir;

	if (prefs_path_init) {
		return prefs_path;
	}
	homedir = get_homedir();
	prefs_path = (char *)malloc(strlen(homedir) + 14);
	if (!prefs_path) 
		return NULL;
	sprintf(prefs_path, "%s/.alsaplayer", homedir);
	prefs_path_init = 1;
	return prefs_path;
}


char *parse_file_uri(const char *furi)
{
	char *res;
	char escape[4];
	int r,w, t, percent, e, val;
	if (!furi)
		return NULL;
	if ((strncmp(furi, "file:", 5) != 0)) {
		return NULL;
	}
	t=strlen(furi);
	res = (char *)malloc(t+1);
	r=5;
	w=0;
	e=0;
	percent = 0;
	while (r < t) {
		switch(furi[r]) {
			case '%':
				/*
				alsaplayer_error("found percent: (%s) (%s)",
						furi, res);
				*/				
				if (percent) {
					res[w++] = '%';
					percent = 0;
				} else {
					percent = 1;
					e = 0;
				}	
				break;
			default:
				if (percent) {
					escape[e++] = furi[r];
					escape[e]=0;
					if (e == 2) {
						if ((sscanf(escape, "%x", &val) == 1)) {
							res[w++] = val;
						} else {
							alsaplayer_error("unandled percent escape (%s)", escape);
						}
						percent = 0;
					}
				} else {
					res[w++] = furi[r];
				}
				break;
		}
		r++;
		res[w] = 0;
	}
	return res;
		
}


int is_playlist(const char *path)
{
	char *ext;

	if (!path)
		return 0;
	ext = strrchr(path, '.');
	if (!ext)
		return 0;
	ext++;
	if (strncasecmp(ext, "pls", 3) == 0 ||
		strncasecmp(ext, "m3u", 3) == 0) {
		return 1;
	}
	return 0;
}

