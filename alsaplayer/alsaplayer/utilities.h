/*  utilities.h
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
 *
 *  $Id$
 * 
 */ 

#ifndef __utilities_h__
#define __utilities_h__
#ifdef __cplusplus
extern "C" {
#endif

// Magic macros for stringizing
#define STRINGISE(a) _STRINGISE(a)	// Not sure why this is needed, but
					// sometimes fails without it.
#define _STRINGISE(a) #a		// Now do the stringising

// Sleep for specified number of micro-seconds
// Used by scopes and things, so use C-style linkage
void dosleep(unsigned int);
void parse_file_uri_free(char *);
char *get_homedir(void);
char *get_prefsdir(void);
char *parse_file_uri(const char *);
int is_playlist(const char *);

#ifdef __cplusplus
}
#endif
#endif
