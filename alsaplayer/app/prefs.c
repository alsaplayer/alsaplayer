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
*/ 
#include "prefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


prefs_handle_t *prefs_load(char *filename)
{
	FILE *fd;
	prefs_handle_t *prefs;
	char buf[1024];
	char *val;

	if (!filename)
		return NULL;
		
	prefs = (prefs_handle_t *)malloc(sizeof(prefs_handle_t));

	if (!prefs)
		return NULL;
	
	memset(prefs, 0, sizeof(prefs_handle_t));
	
	if ((fd = fopen(filename, "r")) == NULL) {
		printf("new config file: %s\n", filename);
		if ((fd = fopen(filename, "w")) == NULL) {	
			printf("failed to create: %s\n", filename);
			free(prefs);
			return NULL;
		}	
	}
	while (fgets(buf, 1023, fd)) {
		if (strlen(buf) < 1024)
			buf[strlen(buf)-1] = 0; /* get rid of '\n' */
		else
			continue;
		if (buf[0] == '#')
			continue;
		if ((val = strchr(buf, '='))) {
			*val = 0; /* replace separator with 0 */
			val++;
			prefs_set_string(prefs, buf, val);
		}
	}
	fclose(fd);

	prefs->filename = (char *)malloc(strlen(filename)+1);
	if (prefs->filename)
		strcpy(prefs->filename, filename);
	
	return prefs;
}

void prefs_set_int(prefs_handle_t *prefs, char *key, int val)
{
	char str[1024];
	
	if (!prefs || !key)
		return;
	sprintf(str, "%d", val);
	prefs_set_string(prefs, key, str);
}


static prefs_key_t *prefs_find_key(prefs_handle_t *prefs, char *key)
{
	prefs_key_t *entry;

	if (!prefs || !key)
		return NULL;
	entry = prefs->keys;
	while (entry && strcmp(entry->key, key))
		entry = entry->next;
	return entry;	
}

void prefs_set_string(prefs_handle_t *prefs, char *key, char *val)
{
	prefs_key_t *entry;
	int len;

	if (!prefs || !key)
		return;

	if ((entry = prefs_find_key(prefs, key))) { /* Already in prefs, replace */
		free(entry->value);
		len = strlen(val);
		entry->value = (char *)malloc(len+1);
		if (entry->value)
			strncpy(entry->value, val, len+1);
	} else { /* New entry */
		printf("inserting %s = %s\n", key, val);
		len = strlen(key);
		entry = (prefs_key_t *)malloc(sizeof(prefs_key_t));
		if (!entry)
			return;
		
		/* Set all field to NULL */
		memset(entry, 0, sizeof(prefs_key_t));
		
		len = strlen(key);
		entry->key = (char *)malloc(len+1);
		if (!entry->key) {
			free(entry);
			return;
		}
		strncpy(entry->key, key, len+1);
		
		len = strlen(val);
		entry->value = (char *)malloc(len+1);
		if (!entry->value) {
			free(entry->key);
			free(entry);
			return;
		}	
		strncpy(entry->value, val, len+1);
		
		/* XXX Need to protect the following with a mutex */	
		if (prefs->last)
			prefs->last = entry;
		else /* First key */
			prefs->keys = entry;
			
		prefs->last = entry;		
	}		
}

void prefs_set_float(prefs_handle_t *prefs, char *key, float val)
{
	char str[1024];
	
	if (!prefs || !key)
		return;
	sprintf(str, "%.6f", val);
	prefs_set_string(prefs, key, str);
}


int prefs_get_int(prefs_handle_t *prefs, char *key, int *res)
{
	char str[1024];
	int val;
	
	if (!prefs || !key)
		return -1;
	if (prefs_get_string(prefs, key, str) == -1)
		return -1;
	if (sscanf(str, "%d", &val) != 1)
		return -1;
	*res = val;
	return 0;
}

int prefs_get_string(prefs_handle_t *prefs, char *key, char *res)
{
	prefs_key_t *entry;

	if (!prefs || !key || !res)
		return -1;

	if ((entry = prefs_find_key(prefs, key))) {
		strcpy(res, entry->value);
		return 0;
	}
	return -1;
}


int prefs_get_float(prefs_handle_t *prefs, char *key, float *res)
{
	char str[1024];
	float val;
	
	if (!prefs || !key)
		return -1;
	if (prefs_get_string(prefs, key, str) == -1)
		return -1;
	if (sscanf(str, "%f", &val) != 1)
		return -1;
	*res = val;
	return 0;

}

int prefs_save(prefs_handle_t *prefs)
{
	FILE *fd;
	prefs_key_t *entry;
	
	if (!prefs || !prefs->filename || !strlen(prefs->filename)) {
		printf("not saving any prefs!!!!\n");
		return -1;
	}	
	if ((fd = fopen(prefs->filename, "w")) == NULL) {
		printf("failed to open %s for writing\n", prefs->filename);
		return -1;
	}	
	entry = prefs->keys;
	while (entry) {
		fprintf(fd, "%s=%s\n", entry->key, entry->value);
		entry = entry->next;
	}
	fclose(fd);
}
