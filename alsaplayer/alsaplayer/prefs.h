/*  prefs.h - Preferences system header
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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
 *  Inspired by xine's prefs 
 *
 *
 *
 *  $Id$
 *
*/ 
#ifndef __prefs_h__
#define __prefs_h__

#ifdef __cplusplus
extern "C" {
#endif

struct _prefs_key {
	char *section;
	char *key;
	char *value;
	struct _prefs_key *next;
};

typedef struct _prefs_key prefs_key_t;

struct _prefs_handle {
	char *filename;
	int loaded;
	prefs_key_t *keys;
	prefs_key_t *last;
};

typedef struct _prefs_handle prefs_handle_t;

prefs_handle_t *prefs_load(char *filename);

void prefs_set_int(prefs_handle_t *prefs, char *section, char *key, int val);
void prefs_set_string(prefs_handle_t *prefs, char *section, char *key, char *val);
void prefs_set_float(prefs_handle_t *prefs, char *section, char *key, float val);
void prefs_set_bool(prefs_handle_t *prefs, char *section, char *key, int val);

int prefs_get_int(prefs_handle_t *prefs, char *section, char *key, int default_val);
char *prefs_get_string(prefs_handle_t *prefs, char *section, char *key, char *default_val);
float prefs_get_float(prefs_handle_t *prefs, char *section, char *key, float default_val);
int prefs_get_bool(prefs_handle_t *prefs, char *section, char *key, int default_val);

int prefs_save(prefs_handle_t *prefs);


/* Global alsaplayer prefs handle. Only use if !NULL of course! */

extern prefs_handle_t *ap_prefs;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __prefs_h__ */
