/*  prefs.c - Preferences system
 *  Copyright (C) 2002-2004 Andy Lo A Foe <andy@alsaplayer.org>
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
 *  Inspired by xine's prefs 
 *
 *  $Id$
 *
*/ 
#include "prefs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alsaplayer_error.h"

#ifdef __cplusplus
extern "C" {
#endif	


int prefs_cmp(const void *p1, const void *p2) 
{
	int res;
	prefs_key_t *a = (prefs_key_t *)p1;
	prefs_key_t *b = (prefs_key_t *)p2;
	char *s1, *s2;

	s1 = (char *)malloc(strlen(a->section)+strlen(a->key)+1);
	s2 = (char *)malloc(strlen(b->section)+strlen(b->key)+1);
	sprintf(s1, "%s%s", a->section, a->key);
	sprintf(s2, "%s%s", b->section, b->key);
	
	res = strcmp(s1, s2);
	
	free(s1);
	free(s2);

	return res;
}

prefs_key_t *prefs_sort(prefs_handle_t *prefs)
{
	prefs_key_t *array;
	prefs_key_t *p;
	int c;
	
	array = (prefs_key_t *)malloc(prefs->count * sizeof(prefs_key_t));
	if (!array)
		return NULL;
	for (c=0, p = prefs->keys; c < prefs->count; c++, p = p->next) {
		array[c] = *p;
	}
	qsort(array, prefs->count, sizeof(prefs_key_t), prefs_cmp);	
	
	return array;
}

	
prefs_handle_t *prefs_load(const char *filename)
{
	FILE *fd;
	prefs_handle_t *prefs;
	char buf[1024];
	char *val;
	char *key;
	int error_count = 0;
	
	if (!filename)
		return NULL;
		
	prefs = (prefs_handle_t *)malloc(sizeof(prefs_handle_t));

	if (!prefs)
		return NULL;
	
	memset(prefs, 0, sizeof(prefs_handle_t));
	
	if ((fd = fopen(filename, "r")) == NULL) {
		if ((fd = fopen(filename, "w")) == NULL) {	
			free(prefs);
			return NULL;
		}	
	}
	while (fgets(buf, 1023, fd) && error_count < 5) {
		buf[1023] = 0;
		if (strlen(buf) < 1024)
			buf[strlen(buf)-1] = 0; /* get rid of '\n' */
		else {
			error_count++;
			continue;
		}	
		if (buf[0] == '#')
			continue;
		if ((val = strchr(buf, '='))) {
			*val = 0; /* replace value separator with 0 */
			val++;
			if ((key = strchr(buf, '.'))) {
				*key = 0; /* replace section separator with 0 */
				key++;	
				prefs_set_string(prefs, buf, key, val);
			} else {
				alsaplayer_error("Found old prefs format (%s), ignoring", buf);
				continue;
			}	
		} else {
			error_count++;
		}	
	}
	fclose(fd);

	if (error_count >= 5) { /* too many errors */
		fprintf(stderr, "*** WARNING: Too many errors in %s\n"
				            "*** WARNING: It is probably corrupted! Please remove it.\n",
					filename);
		return NULL;
	}	
	prefs->filename = strdup(filename);

	return prefs;
}

void prefs_set_int(prefs_handle_t *prefs, const char *section, const char *key, int val)
{
	char str[1024];

	assert(prefs);
	assert(key);
	assert(section);
	
	sprintf(str, "%d", val);
	prefs_set_string(prefs, section, key, str);
}


void prefs_set_bool(prefs_handle_t *prefs, const char *section, const char *key, int val)
{
	char str[1024];

	assert(prefs);
	assert(key);
	assert(section);
	
	sprintf(str, "%s", val ? "true" : "false");
	prefs_set_string(prefs, section, key, str);
}


static prefs_key_t *prefs_find_key(prefs_handle_t *prefs, const char *section, const char *key)
{
	prefs_key_t *entry;

	assert(prefs);
	assert(key);
	assert(section);
	
	entry = prefs->keys;
	while (entry) {
		if (strcmp(entry->section, section)==0 && strcmp(entry->key, key)==0)
			return entry;
		entry = entry->next;
	}	
	return NULL;
}

void prefs_set_string(prefs_handle_t *prefs, const char *section, const char *key, const char *val)
{
	prefs_key_t *entry;

	assert(prefs);
	assert(key);
	assert(section);

	if ((entry = prefs_find_key(prefs, section, key))) { /* Already in prefs, replace */
		free(entry->value);
		entry->value = strdup(val);
	} else { /* New entry */
		entry = (prefs_key_t *)malloc(sizeof(prefs_key_t));
		if (!entry)
			return;
		
		/* Set all field to NULL */
		memset(entry, 0, sizeof(prefs_key_t));

		entry->section = strdup(section);
		if (!entry->section) {
				free(entry);
				return;
		}

		entry->key = strdup(key);
		if (!entry->key) {
			free(entry->section);
			free(entry);
			return;
		}

		entry->value = strdup(val);
		if (!entry->value) {
			free(entry->section);
			free(entry->key);
			free(entry);
			return;
		}	

		/* XXX Need to protect the following with a mutex */	
		if (prefs->last) {
			prefs->last->next = entry;
		} else { /* First key */
			prefs->keys = entry;
		}
		prefs->count++;
		prefs->last = entry;		
	}		
}

void prefs_set_float(prefs_handle_t *prefs, const char *section, const char *key, float val)
{
	char str[1024];

	assert(prefs);
	assert(key);
	assert(section);
	
	sprintf(str, "%.6f", val);
	prefs_set_string(prefs, section, key, str);
}


int prefs_get_bool(prefs_handle_t *prefs, const char *section, const char *key, int default_val)
{
	char str[1024];
	const char *res;

	assert(prefs);
	assert(key);
	assert(section);
	
	sprintf(str, "%s", default_val ? "true" : "false");
	res = prefs_get_string(prefs, section, key, str);
	if (strncasecmp(res, "true", 4) == 0 ||
			strncasecmp(res, "yes", 3) == 0 ||
			strncasecmp(res, "1", 1) == 0) {
		return 1;
	}
	return 0;
}


int prefs_get_int(prefs_handle_t *prefs, const char *section, const char *key, int default_val)
{
	char str[1024];
	const char *res;
	int val;

	assert(prefs);
	assert(key);
	assert(section);
	
	sprintf(str, "%d", default_val);
	res = prefs_get_string(prefs, section, key, str);
	if (sscanf(res, "%d", &val) != 1)
		return default_val;
	return val;
}

const char *prefs_get_string(prefs_handle_t *prefs, const char *section, const char *key, const char *default_val)
{
	prefs_key_t *entry;

	assert(prefs);
	assert(key);
	assert(section);

	if ((entry = prefs_find_key(prefs, section, key))) {
		return entry->value;
	} else {
		prefs_set_string(prefs, section, key, default_val);
	}	
	return default_val;
}


float prefs_get_float(prefs_handle_t *prefs, const char *section, const char *key, float default_val)
{
	char str[1024];
	const char *res;
	float val;

	assert(prefs);
	assert(key);
	assert(section);
	
	sprintf(str, "%.6f", default_val);
	res = prefs_get_string(prefs, section, key, str);
	if (sscanf(res, "%f", &val) != 1)
		return default_val;
	return val;

}

int prefs_save(prefs_handle_t *prefs)
{
	FILE *fd;
	prefs_key_t *entry = NULL;
	prefs_key_t *sorted = NULL;
	int c;
	
	assert(prefs);

	if (!prefs->filename || !strlen(prefs->filename)) {
		return -1;
	}

	sorted = prefs_sort(prefs);
	
	if ((fd = fopen(prefs->filename, "w")) == NULL) {
		return -1;
	}	
	entry = prefs->keys;
	fprintf(fd,
		"#\n"
		"# alsaplayer config file\n"
		"#\n"
		"# Only edit this file if the application is not active.\n"
		"# Any modifications might (will!) be lost otherwise.\n"
		"#\n");
	if (sorted) {
		for (c=0; c < prefs->count; c++) {
			fprintf(fd, "%s.%s=%s\n",
				sorted[c].section, 
				sorted[c].key, 
				sorted[c].value);
		}
		free(sorted);
	} else {	
		while (entry) {
			fprintf(fd, "%s.%s=%s\n", entry->section, entry->key, entry->value);
			entry = entry->next;
		}
	}	
	fclose(fd);

	return 0;
}

// Function to free user created prefs
void prefs_free(prefs_handle_t *prefs)
{
	prefs_key_t *entry;
   
	assert(prefs);

	entry = prefs->keys;
	while (entry) {
		prefs_key_t *next = entry->next;

		free (entry->section);
		free (entry->key);
		free (entry->value);
	    
		free (entry);
		entry = next;
	}
    
	if (prefs->filename)  free (prefs->filename);
	free (prefs);
}

#ifdef __cplusplus
}
#endif	


