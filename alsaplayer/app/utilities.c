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

char *parse_file_uri(const char *furi)
{
	char *res;
	char escape[4];
	int r,w, t, percent, e;
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
					if (isdigit(furi[r])) {
						escape[e++] = furi[r];
						escape[e]=0;
					}
					if (e == 2) {
						if (atoi(escape) == 20) {
							res[w++] = ' ';
						} else {
							alsaplayer_error("unandled percent escape (%d,%c)", atoi(escape), atoi(escape));
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


