/*  amessage.h
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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
*/ 
#ifndef __AMESSAGE_H__
#define __AMESSAGE_H__

#include <pthread.h>
#include <sys/types.h>

#define AP_CONTROL_BASE_VERSION 0x1000
#define AP_CONTROL_VERSION      (AP_CONTROL_BASE_VERSION + 6)

#define KEYID_LEN	32

#define KTYPE_STRING	0xaa01
#define KTYPE_FLOAT	0xaa02
#define KTYPE_INT32	0xaa03

#define AP_PLAY                 0xcc00
#define AP_STOP                 0xcc01
#define AP_NEXT                 0xcc02
#define AP_PREV                 0xcc03
#define AP_PAUSE                0xcc04
#define AP_UNPAUSE              0xcc05
#define AP_CLEAR_PLAYLIST       0xcc06
#define AP_QUIT                 0xcc07
#define AP_PING                 0xcc08
#define AP_SET_VOLUME           0xcc09
#define AP_GET_VOLUME           0xcc0a
#define AP_SET_SPEED            0xcc0b
#define AP_GET_SPEED            0xcc0c
#define AP_GET_SESSION_NAME     0xcc0d
#define AP_GET_SONG_NAME        0xcc0e
#define AP_IS_PLAYING		0xcc0f
#define AP_GET_ARTIST           0xcc10
#define AP_GET_ALBUM            0xcc11
#define AP_GET_GENRE            0xcc12
#define AP_GET_TRACK_NUMBER     0xcc13
#define AP_GET_YEAR             0xcc14
#define AP_ADD_PATH             0xcc15
#define AP_GET_SONG_LENGTH_SECOND	0xcc16
#define AP_GET_SONG_LENGTH_FRAME	0xcc17
#define AP_SET_POS_SECOND               0xcc18
#define AP_GET_POS_SECOND               0xcc19
#define AP_SET_POS_FRAME                0xcc1a
#define AP_GET_POS_FRAME                0xcc1b
#define AP_GET_STREAM_TYPE              0xcc1c
#define AP_GET_STATUS			0xcc1d
#define AP_SET_POS_SECOND_RELATIVE	0xcc1e
#define AP_GET_TITLE			0xcc1f
#define AP_GET_COMMENT			0xcc20
#define AP_ADD_AND_PLAY		0xcc21
#define AP_SET_PAN		0xcc22
#define AP_GET_PAN		0xcc23
#define AP_SORT			0xcc24
#define AP_GET_FILE_PATH	0xcc25
#define AP_ADD_PLAYLIST		0xcc26
#define AP_JUMP_TO		0xcc27
#define AP_SHUFFLE_PLAYLIST	0xcc28
#define AP_SAVE_PLAYLIST	0xcc29
#define AP_GET_PLAYLIST_LENGTH	0xcc2a
#define AP_GET_TRACKS		0xcc2b
#define AP_SET_LOOPING		0xcc2c
#define AP_IS_LOOPING		0xcc2d
#define AP_SET_PLAYLIST_LOOPING	0xcc2e
#define AP_IS_PLAYLIST_LOOPING	0xcc2f
#define AP_IS_PAUSED		0xcc30

#define AP_GET_PLAYLIST_POSITION 0xcc31
#define AP_GET_FILE_PATH_FOR_TRACK 0xcc32
#define AP_INSERT                 0xcc33
#define AP_REMOVE               0xcc34
#define AP_SET_CURRENT          0xcc35

#define AP_GET_PLAYLIST		0xcc36
/* Add new commands here */

#ifdef __cplusplus
extern "C" {
#endif	

struct _ap_hdr {
	int32_t version;
	int32_t cmd;
	int32_t result;
	int32_t nr_keys;
};	

struct _ap_key {
	int32_t length;
	int32_t key_type;
	char key_id[KEYID_LEN];
	void *data;
	struct _ap_key *next;
};	

typedef struct _ap_hdr ap_hdr_t;
typedef struct _ap_key ap_key_t;

struct _ap_message {
	pthread_mutex_t mutex;
	ap_hdr_t header;
	ap_key_t keys;
	struct _ap_key *root;
	struct _ap_key *tail;
	struct _ap_key *current;
};

typedef struct _ap_message ap_message_t;


ap_message_t *ap_message_new(void);
ap_key_t *ap_key_new(const char *keyid);
int ap_message_add_float(ap_message_t *, const char *, float);
int ap_message_add_int32(ap_message_t *, const char *, int32_t);
int ap_message_add_string(ap_message_t *, const char *, const char *);
void ap_message_delete(ap_message_t *);

int ap_message_send(int fd, ap_message_t *);
ap_message_t *ap_message_receive(int fd);

float *ap_message_find_float(ap_message_t *, const char *);
int32_t *ap_message_find_int32(ap_message_t *, const char *);
char *ap_message_find_string(ap_message_t *, const char *);

#ifdef __cplusplus
}
#endif	



#endif
