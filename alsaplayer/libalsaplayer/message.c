/*  message.c - external control for alsaplayer
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
 *
 *  $Id$
 *
 */ 
#include "message.h"
#include "control.h"
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif


int ap_connect_session (int session)
{
	int socket_fd;
	struct sockaddr_un saddr;
	struct passwd *pwd;

	pwd = getpwuid(geteuid());

	if ((socket_fd = socket (AF_UNIX, SOCK_STREAM, 0)) != -1) {
		saddr.sun_family = AF_UNIX;
		sprintf (saddr.sun_path, "/tmp/alsaplayer_%s_%d", pwd == NULL ?
				"anonymous" : pwd->pw_name, session);
		if (connect (socket_fd, (struct sockaddr *) &saddr, sizeof (saddr)) != -1) {
			return socket_fd;
		}
	}
	return -1;
}


ap_message_t *ap_message_new()
{
	ap_message_t *msg;

	msg = (ap_message_t *)malloc(sizeof(ap_message_t));
	if (msg) {
		memset(msg, 0, sizeof(ap_message_t));
		msg->header.version = AP_CONTROL_VERSION;
		return msg;
	}
	return NULL;
}


ap_key_t *ap_key_new(const char *key_id)
{
	ap_key_t *key;
	key = (ap_key_t *)malloc(sizeof(ap_key_t));
	
	if (key) {
		memset(key, 0, sizeof(ap_key_t));
		if (strlen(key_id) > KEYID_LEN) {
			strncpy(key->key_id, key_id, KEYID_LEN-1);
			key->key_id[KEYID_LEN] = 0;
		} else {
			strcpy(key->key_id, key_id);
		}	
		return key;
	}	
	return NULL;
}


void ap_key_delete(ap_key_t *key)
{
	if (key) {
		if (key->data) {
			free(key->data);
		}
		free(key);
	}	
}


void ap_message_delete(ap_message_t *msg)
{
	int c;
	ap_key_t *last;
	
	if (msg) {
		//printf("going to delete msg (%d keys)\n", msg->header.nr_keys);
		for (c = 0, msg->current = msg->root; c < msg->header.nr_keys; c++) {
			last = msg->current;
			msg->current = msg->current->next;
			ap_key_delete(last);
		}	
		free(msg);
		//puts("delete msg successful");
	}	
}


void ap_message_add_key(ap_message_t *msg, ap_key_t *key)
{
	if (!msg || !key)
		return;
	
	if (msg->root == NULL) {
		//printf("adding first key... (%s)\n", key->key_id);
		msg->root = key;
		msg->tail = msg->root;
		msg->current = msg->tail;
	} else {
		//printf("adding key...(%s)\n", key->key_id);
		msg->tail->next = key;
		msg->tail = key;
	}
	msg->header.nr_keys++;

}


ap_message_t *ap_message_receive(int fd)
{
	int c, keys_to_read;
	ap_key_t *key;
	ap_message_t *msg;

	msg = ap_message_new();
	
	if (!msg)
		return NULL;
	
	memset(msg, 0, sizeof(ap_message_t));
	
	if (read (fd, msg, sizeof (ap_message_t)) != sizeof (ap_message_t)) {
		ap_message_delete(msg);
		return NULL;
	}	
	
	// Check version before parsing	
	if (msg->header.version != AP_CONTROL_VERSION) {
		fprintf(stderr, "protocol version mismatch (client): %x != %x",
			msg->header.version, AP_CONTROL_VERSION);
		ap_message_delete(msg);
		return NULL;
	}	
	// List variables are invalid for this address space so reset them
	msg->root = msg->tail = msg->current = NULL;
	
	// Also reset nr_keys
	keys_to_read = msg->header.nr_keys;
	msg->header.nr_keys = 0;
	
	//printf("receiving message with %d keys (msg->root = %x)\n",
	//		keys_to_read, msg->root);
	for (c = 0; c < keys_to_read; c++) {
		//printf("in key read loop (c=%d)\n", c);
		key = (ap_key_t *)malloc(sizeof(ap_key_t));
		memset(key, 0, sizeof(ap_key_t));
		if (read(fd, key, sizeof(ap_key_t)) != sizeof(ap_key_t)) {
			fprintf(stderr, "could not read key\n");
			ap_message_delete(msg);
			return NULL;
		}
		if (!key->length) {
			fprintf(stderr, "invalid key (no data)\n");
			ap_message_delete(msg);
			return NULL;
		}
		key->data = malloc(key->length);
		if (!key->data) {
			fprintf(stderr, "could not allocate data buffer\n");
			ap_message_delete(msg);
			return NULL;
		}
		if (read(fd, key->data, key->length) != key->length) {
			fprintf(stderr, "could not read data\n");
			free(key->data);
			ap_message_delete(msg);
			return NULL;
		}
		// Assign key
		ap_message_add_key(msg, key);
	}
	return msg;
}


int ap_message_add_float(ap_message_t *msg, char *key, float val)
{
	ap_key_t *new_key;

	new_key = ap_key_new(key);

	new_key->key_type = KTYPE_FLOAT;
	new_key->data = (void *)malloc(sizeof(float));
	*((float*)new_key->data) = val;
	new_key->length = sizeof(float);

	ap_message_add_key(msg, new_key);	
	
	return 1;
}


int ap_message_add_int32(ap_message_t *msg, char *key, int32_t val)
{
	ap_key_t *new_key;

	new_key = ap_key_new(key);

	new_key->key_type = KTYPE_INT32;
	new_key->data = (void *)malloc(sizeof(int32_t));
	*((int32_t*)new_key->data) = val;
	//printf("key value = %d (%d)\n", *((int32_t*)new_key->data), val);
	new_key->length = sizeof(int32_t);
	
	ap_message_add_key(msg, new_key);	
	
	return 1;
}


ap_key_t *ap_message_find_key(ap_message_t *msg, char *key, int32_t key_type)
{
	ap_key_t *current;
	
	if (msg) {
		current = msg->root;
		while (current) {
			if ((strcmp(current->key_id, key) == 0) &&
					current->key_type == key_type) {
				return current;
			}
			current = current->next;
		}
	}
	return NULL;
}


float *ap_message_find_float(ap_message_t *msg, char *key_id)
{
	ap_key_t *key;

	if ((key = ap_message_find_key(msg, key_id, KTYPE_FLOAT))) {
		return ((float*)key->data);
	}
	return NULL;
}


int32_t *ap_message_find_int32(ap_message_t *msg, char *key_id)
{
	ap_key_t *key;

	if ((key = ap_message_find_key(msg, key_id, KTYPE_INT32))) {
		return ((int32_t*)key->data);
	}
	return NULL;
}


char *ap_message_find_string(ap_message_t *msg, char *key_id)
{
	ap_key_t *key;

	if ((key = ap_message_find_key(msg, key_id, KTYPE_STRING))) {
		return (char*)key->data;
	}
	return NULL;
}


int ap_message_add_string(ap_message_t *msg, char *key_id, const char *val)
{
	ap_key_t *new_key;
	char *str;
	
	new_key = ap_key_new(key_id);
	new_key->key_type = KTYPE_STRING;
	new_key->data = str = strdup(val);
	new_key->length = strlen(val)+1; // Also copy Null pointer!

	ap_message_add_key(msg, new_key);
	
	return 1;
}



int ap_message_send(int fd, ap_message_t *msg)
{
	int c;
	ap_key_t *current;	
	
	if (!msg)
		return 0;

	msg->header.version = AP_CONTROL_VERSION;
	if (write (fd, msg, sizeof (ap_message_t)) != sizeof (ap_message_t))
		return 0;
	// NOTE: list structure will be rebuild on the other side
	for (c = 0, current = msg->root; c < msg->header.nr_keys; c++,
			current = current->next) {
		if (current == NULL) {
			fprintf(stderr, "problem!\n");
			continue;
		}	
		if (write(fd, current, sizeof(ap_key_t)) != sizeof(ap_key_t)) {
			fprintf(stderr, "error writing key\n");
			continue;
		}
		if (write(fd, current->data, current->length) != current->length) {
			fprintf(stderr, "error writing key data\n");
			continue;
		}	
	}
	//printf("successfully sent message with %d keys\n", msg->header.nr_keys);
	return 1;
}


int ap_session_running(int session)
{
	struct stat statbuf;
	struct passwd *pwd;
	char path[1024];

	pwd = getpwuid(geteuid());

	sprintf(path, "/tmp/alsaplayer_%s_%d", pwd == NULL ? "anonymous" :
			pwd->pw_name, session);
	if (stat(path, &statbuf) != 0) 
		return 0;
	if (S_ISSOCK(statbuf.st_mode)) {
		if (ap_ping(session))
			return 1;
	}
	return 0;
}


int ap_find_session(char *session_name, int *session)
{
	int i = 0;
	char remote_name[AP_SESSION_MAX];
	char test_path[1024];
	char tmp[1024];
	char username[512];
	struct passwd *pwd;
	DIR *dir;
	struct dirent *entry;
	int session_id;

	if (!session_name)
		return 0;
	dir = opendir("/tmp");

	pwd = getpwuid(geteuid());

	sprintf(username, pwd == NULL ? "anonymous" : pwd->pw_name);

	sprintf(test_path, "alsaplayer_%s_", username);

	if (dir) {
		while ((entry = readdir(dir)) != NULL) {
			if (strncmp(entry->d_name, test_path, strlen(test_path)) == 0) {
				sprintf(tmp, "%s%%d", test_path);
				if (sscanf(entry->d_name, tmp, &session_id) == 1) {
					if (ap_session_running(i) == 1) {
						if (ap_get_session_name(i, remote_name)) {
							if (strcmp(remote_name, session_name) == 0) {
								*session = i;
								return 1;
							}
						}
					}
				}
			}	
		}	
	}
	return 0;
}


/* Implementation of public function starts here */


int ap_version()
{
	return AP_CONTROL_VERSION;
}


int ap_set_speed(int session, float speed)
{
	int fd;
	ap_message_t *msg, *reply;
	int32_t *result;
	
	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = AP_SET_SPEED;
	ap_message_add_float(msg, "speed", speed);
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;
	
	reply = ap_message_receive(fd);
	close(fd);
		
	if ((result = ap_message_find_int32(reply, "ack"))) {
		ap_message_delete(reply);
		return 1;
	}
	ap_message_delete(reply);
	return 0;
}


int ap_get_speed(int session, float *val)
{
	int fd;
	ap_message_t *msg, *reply;
	float *result;
	
	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = AP_GET_SPEED;
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;

	reply = ap_message_receive(fd);
	close(fd);
	
	if ((result = ap_message_find_float(reply, "speed"))) {
		*val = *result;
		ap_message_delete(reply);
		return 1;
	}
	ap_message_delete(reply);
	return 0;
}


/* Convenience function for commands that take a single int */
int ap_cmd_set_int(int session, int32_t cmd, int val)
{
	int fd;
	ap_message_t *msg, *reply;
	int32_t *result;
	
	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = cmd;
	ap_message_add_int32(msg, "int", val);

	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;

	reply = ap_message_receive(fd);
	close(fd);

	if ((result = ap_message_find_int32(reply, "ack"))) {
		ap_message_delete(reply);
		return 1;
	}
	ap_message_delete(reply);
	return 0;
}



/* Convenience function for commands that return a single int */
int ap_cmd_get_int(int session, int32_t cmd, int *val)
{
	int fd;
	ap_message_t *msg, *reply;
	int32_t *result;
	
	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = cmd;

	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;

	reply = ap_message_receive(fd);
	close(fd);

	if ((result = ap_message_find_int32(reply, "int"))) {
		*val = *result;
		ap_message_delete(reply);
		return 1;
	}
	ap_message_delete(reply);
	return 0;
}

int ap_set_volume(int session, int volume)
{
	return (ap_cmd_set_int(session, AP_SET_VOLUME, volume));
}

int ap_set_pan(int session, int pan)
{
	return (ap_cmd_set_int(session, AP_SET_PAN, pan));
}

int ap_set_position(int session, int pos)
{
	return (ap_cmd_set_int(session, AP_SET_POS_SECOND, pos));
}

int ap_set_position_relative(int session, int pos)
{
	return (ap_cmd_set_int(session, AP_SET_POS_SECOND_RELATIVE, pos));
}

int ap_get_position(int session, int *val)
{
	return (ap_cmd_get_int(session, AP_GET_POS_SECOND, val));
}

int ap_get_volume(int session, int *volume)
{
	return (ap_cmd_get_int(session, AP_GET_VOLUME, volume));
}

int ap_get_pan(int session, int *pan)
{
	return (ap_cmd_get_int(session, AP_GET_PAN, pan));
}

int ap_get_length(int session, int *length)
{
	return (ap_cmd_get_int(session, AP_GET_SONG_LENGTH_SECOND, length));
}

int ap_set_frame(int session, int frame)
{
	return (ap_cmd_set_int(session, AP_SET_POS_FRAME, frame));
}

int ap_jump_to(int session, int pos)
{
	return (ap_cmd_set_int(session, AP_JUMP_TO, pos));
}

int ap_get_frame(int session, int *val)
{
	return (ap_cmd_get_int(session, AP_GET_POS_FRAME, val));
}

int ap_get_frames(int session, int *val)
{
	return (ap_cmd_get_int(session, AP_GET_SONG_LENGTH_FRAME, val));
}

int ap_is_playing(int session, int *val)
{
	return (ap_cmd_get_int(session, AP_IS_PLAYING, val));
}

/* Convenience function for commands that return a single string */
int ap_get_single_string_command(int session, int32_t cmd, char *str, int maxlen)
{
	int fd;
	ap_message_t *msg, *reply;
	char *result;

	if (!str)
		return 0;

	str[0] = 0; // Make sure the string is always NULL terminated
	            // Even if we fail to get the data
	
	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = cmd;
	
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;

	reply = ap_message_receive(fd);
	close(fd);
	
	if ((result = ap_message_find_string(reply, "string"))) {
		if (strlen(result) > (size_t)maxlen) {
			strncpy(str, result, maxlen-1);
			str[maxlen] = 0;
		} else 	
			strcpy(str, result);
		ap_message_delete(reply);
		return 1;
	}
	ap_message_delete(reply);
	return 0;
}


int ap_get_session_name(int session, char *str)
{
	return (ap_get_single_string_command(session, 
				AP_GET_SESSION_NAME, str, AP_SESSION_MAX));
}

int ap_get_title(int session, char *str)
{
	return (ap_get_single_string_command(session,
				AP_GET_TITLE, str, AP_TITLE_MAX));
}

int ap_get_file_path(int session, char *str)
{
	return (ap_get_single_string_command(session,
				AP_GET_FILE_PATH, str, AP_FILE_PATH_MAX));
}

int ap_get_artist(int session, char *str)
{
	return (ap_get_single_string_command(session, 
				AP_GET_ARTIST, str, AP_ARTIST_MAX));
}

int ap_get_genre(int session, char *str)
{
	return (ap_get_single_string_command(session,
				AP_GET_GENRE, str, AP_GENRE_MAX));
}

int ap_get_comment(int session, char *str)
{
	return (ap_get_single_string_command(session,
				AP_GET_COMMENT, str, AP_COMMENT_MAX));
}

int ap_get_album(int session, char *str)
{
	return (ap_get_single_string_command(session, 
				AP_GET_ALBUM, str, AP_ALBUM_MAX));
}

int ap_get_year(int session, char *str)
{
	return (ap_get_single_string_command(session, 
				AP_GET_YEAR, str, AP_YEAR_MAX));
}

int ap_get_track_number(int session, char *str)
{
	return (ap_get_single_string_command(session, 
				AP_GET_TRACK_NUMBER, str, AP_TRACK_NUMBER_MAX));
}

int ap_get_stream_type(int session, char *str)
{
	return (ap_get_single_string_command(session,
				AP_GET_STREAM_TYPE, str, AP_STREAM_TYPE_MAX));
}

int ap_get_status(int session, char *str)
{
	return (ap_get_single_string_command(session,
				AP_GET_STATUS, str, AP_STATUS_MAX));
}

int ap_ping(int session)
{
	int fd;
	int32_t *pong;
	int32_t ret_val;
	ap_message_t *msg, *reply;

	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = AP_PING;
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;
	
	reply = ap_message_receive(fd);
	close(fd);
		
	if ((pong = ap_message_find_int32(reply, "pong"))) {
		ret_val = *pong;
		ap_message_delete(reply);
		// ret_val not used
		return 1;
	}
	ap_message_delete(reply);
	return 0;
}

int ap_add_and_play(int session, const char *path)
{
	int fd;
	int32_t *result, ret_val;
	ap_message_t *msg, *reply;

	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = AP_ADD_AND_PLAY;
	ap_message_add_string(msg, "path1", path);
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;
	
	reply = ap_message_receive(fd);
	close(fd);
	
	if ((result = ap_message_find_int32(reply, "ack"))) {
		ret_val = *result;
		ap_message_delete(reply);
		return 1;
	}
	puts("ap_add_and_play() failed for some reason");
	ap_message_delete(reply);
	return 0;
}


int ap_add_path(int session, const char *path)
{
	int fd;
	int32_t *result, ret_val;
	ap_message_t *msg, *reply;

	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = AP_ADD_PATH;
	ap_message_add_string(msg, "path1", path);
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;
	
	reply = ap_message_receive(fd);
	close(fd);
	
	if ((result = ap_message_find_int32(reply, "ack"))) {
		ret_val = *result;
		ap_message_delete(reply);
		return 1;
	}
	puts("ap_add_path() failed for some reason");
	ap_message_delete(reply);
	return 0;
}


int ap_add_playlist(int session, const char *playlistfile)
{
	int fd;
	int32_t *result, ret_val;
	ap_message_t *msg, *reply;

	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = AP_ADD_PLAYLIST;
	ap_message_add_string(msg, "path1", playlistfile);
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;

	reply = ap_message_receive(fd);
	close(fd);

	if ((result = ap_message_find_int32(reply, "ack"))) {
		ret_val = *result;
		ap_message_delete(reply);
		return 1;
	}
	puts("ap_add_playlist() failed for some reason");
	ap_message_delete(reply);
	return 0;
}


int ap_sort (int session, char *seq)
{
	int fd;
	int32_t *result, ret_val;
	ap_message_t *msg, *reply;

	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	msg = ap_message_new();
	msg->header.cmd = AP_SORT;
	ap_message_add_string(msg, "seq", seq);
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;
	
	reply = ap_message_receive(fd);
	close(fd);
	
	if ((result = ap_message_find_int32(reply, "ack"))) {
		ret_val = *result;
		ap_message_delete(reply);
		return 1;
	}
	puts("ap_sort() failed for some reason");
	ap_message_delete(reply);
	return 0;
}


/* Convenience function for commands that take no argument */
static int ap_do_command_only(int session, int32_t cmd)
{
	int fd;
	int32_t *result, ret_val;
	
	ap_message_t *msg, *reply;

	fd = ap_connect_session(session);
	if (fd < 0)
		return 0;
	
	msg = ap_message_new();
	msg->header.cmd = cmd;
	ap_message_send(fd, msg);
	ap_message_delete(msg);
	msg = NULL;
	
	reply = ap_message_receive(fd);
	close(fd);	
	
	if ((result = ap_message_find_int32(reply, "ack"))) {
		ret_val = *result;
		ap_message_delete(reply);
		return ret_val;
	}
	ap_message_delete(reply);
	return 0;
}

int ap_play(int session)
{
	return (ap_do_command_only(session, AP_PLAY));
}

int ap_next(int session)
{
	return (ap_do_command_only(session, AP_NEXT));
}

int ap_prev(int session)
{
	return (ap_do_command_only(session, AP_PREV));
}

int ap_stop(int session)
{
	return (ap_do_command_only(session, AP_STOP));
}

int ap_pause(int session)
{
	return (ap_do_command_only(session, AP_PAUSE));
}

int ap_unpause(int session)
{
	return (ap_do_command_only(session, AP_UNPAUSE));
}

int ap_clear_playlist(int session)
{
	return (ap_do_command_only(session, AP_CLEAR_PLAYLIST));
}

int ap_shuffle_playlist(int session)
{
	return (ap_do_command_only(session, AP_SHUFFLE_PLAYLIST));
}

int ap_quit(int session)
{
	return (ap_do_command_only(session, AP_QUIT));
}



#ifdef __cplusplus
} /* extern "C" */
#endif
