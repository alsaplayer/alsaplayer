/*  prefs.c - Preferences system
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
 *  $Id$
 *
*/ 
#include "prefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alsaplayer_error.h"

#ifdef __cplusplus
extern "C" {
#endif	

prefs_handle_t *prefs_load(char *filename)
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
	prefs->filename = (char *)malloc(strlen(filename)+1);
	if (prefs->filename)
		strcpy(prefs->filename, filename);
	
	return prefs;
}

void prefs_set_int(prefs_handle_t *prefs, char *section, char *key, int val)
{
	char str[1024];
	
	if (!prefs || !key || !section)
		return;
	sprintf(str, "%d", val);
	prefs_set_string(prefs, section, key, str);
}


void prefs_set_bool(prefs_handle_t *prefs, char *section, char *key, int val)
{
	char str[1024];

	if (!prefs || !key || !section)
		return;
	sprintf(str, "%s", val ? "true" : "false");
	prefs_set_string(prefs, section, key, str);
}


static prefs_key_t *prefs_find_key(prefs_handle_t *prefs, char *section, char *key)
{
	prefs_key_t *entry;

	if (!prefs || !key || !section)
		return NULL;
	entry = prefs->keys;
	while (entry) {
		if (strcmp(entry->section, section)==0 && strcmp(entry->key, key)==0)
			return entry;
		entry = entry->next;
	}	
	return NULL;
}

void prefs_set_string(prefs_handle_t *prefs, char *section, char *key, char *val)
{
	prefs_key_t *entry;
	int len;

	if (!prefs || !key || !section)
		return;

	if ((entry = prefs_find_key(prefs, section, key))) { /* Already in prefs, replace */
		free(entry->value);
		len = strlen(val);
		entry->value = (char *)malloc(len+1);
		if (entry->value)
			strncpy(entry->value, val, len+1);
	} else { /* New entry */
		entry = (prefs_key_t *)malloc(sizeof(prefs_key_t));
		if (!entry)
			return;
		
		/* Set all field to NULL */
		memset(entry, 0, sizeof(prefs_key_t));

		len = strlen(section);
		entry->section = (char *)malloc(len+1);
		if (!entry->section) {
				free(entry);
				return;
		}
		strncpy(entry->section, section, len+1);
		
		
		len = strlen(key);
		entry->key = (char *)malloc(len+1);
		if (!entry->key) {
			free(entry->section);
			free(entry);
			return;
		}
		strncpy(entry->key, key, len+1);
		
		len = strlen(val);
		entry->value = (char *)malloc(len+1);
		if (!entry->value) {
			free(entry->section);
			free(entry->key);
			free(entry);
			return;
		}	
		strncpy(entry->value, val, len+1);
		
		/* XXX Need to protect the following with a mutex */	
		if (prefs->last) {
			prefs->last->next = entry;
		} else { /* First key */
			prefs->keys = entry;
		}	
		prefs->last = entry;		
	}		
}

void prefs_set_float(prefs_handle_t *prefs, char *section, char *key, float val)
{
	char str[1024];
	
	if (!prefs || !key || !section)
		return;
	sprintf(str, "%.6f", val);
	prefs_set_string(prefs, section, key, str);
}


int prefs_get_bool(prefs_handle_t *prefs, char *section, char *key, int default_val)
{
	char str[1024];
	char *res;
	
	if (!prefs || !key || !section)
		return -1;
	sprintf(str, "%s", default_val ? "true" : "false");
	res = prefs_get_string(prefs, section, key, str);
	if (strncasecmp(res, "true", 4) == 0 ||
			strncasecmp(res, "yes", 3) == 0 ||
			strncasecmp(res, "1", 1) == 0) {
		return 1;
	}
	return 0;
}


int prefs_get_int(prefs_handle_t *prefs, char *section, char *key, int default_val)
{
	char str[1024];
	char *res;
	int val;
	
	if (!prefs || !key)
		return -1;
	sprintf(str, "%d", default_val);
	res = prefs_get_string(prefs, section, key, str);
	if (sscanf(res, "%d", &val) != 1)
		return default_val;
	return val;
}

char *prefs_get_string(prefs_handle_t *prefs, char *section, char *key, char *default_val)
{
	prefs_key_t *entry;

	if (!prefs || !key || !section)
		return default_val;

	if ((entry = prefs_find_key(prefs, section, key))) {
		return entry->value;
	} else {
		prefs_set_string(prefs, section, key, default_val);
	}	
	return default_val;
}


float prefs_get_float(prefs_handle_t *prefs, char *section, char *key, float default_val)
{
	char str[1024];
	char *res;
	float val;
	
	if (!prefs || !key)
		return default_val;
	sprintf(str, "%.6f", default_val);
	res = prefs_get_string(prefs, section, key, str);
	if (sscanf(res, "%f", &val) != 1)
		return default_val;
	return val;

}

int prefs_save(prefs_handle_t *prefs)
{
	FILE *fd;
	prefs_key_t *entry;
	
	if (!prefs || !prefs->filename || !strlen(prefs->filename)) {
		return -1;
	}	
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
			
	while (entry) {
		fprintf(fd, "%s.%s=%s\n", entry->section, entry->key, entry->value);
		entry = entry->next;
	}
	fclose(fd);

	return 0;
}

// Function to free user created prefs
void prefs_free(prefs_handle_t *prefs)
{
	prefs_key_t *entry;
    
	if (!prefs)  return;

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


