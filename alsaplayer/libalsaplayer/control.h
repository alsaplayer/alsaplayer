/*  control.h
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
*/ 
#ifndef __CONTROL_H__
#define __CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#define AP_CONTROL_BASE_VERSION		0x1000
#define AP_CONTROL_VERSION				(AP_CONTROL_BASE_VERSION + 1)
#define MAX_AP_SESSIONS						32	/* Convenience, will be removed */
	
typedef enum  {
	AP_DO_PLAY = 0x1,
	AP_DO_STOP,
	AP_DO_NEXT,
	AP_DO_PREV,
	AP_DO_PAUSE,
	AP_DO_CLEAR_PLAYLIST,
	AP_GET_FLOAT_PING,
	AP_SET_FLOAT_VOLUME,
	AP_GET_FLOAT_VOLUME,
	AP_SET_FLOAT_SPEED,
	AP_GET_FLOAT_SPEED,
	AP_GET_STRING_SESSION_NAME,
	AP_GET_STRING_SONG_NAME,
	AP_SET_STRING_ADD_FILE,
	AP_GET_INT_SONG_LENGTH,
	AP_GET_INT_CURRENT_TIME,
	AP_SET_INT_POS_SECOND,
	AP_GET_INT_POS_SECOND,
	AP_SET_INT_POS_FRAME,
	AP_GET_INT_POS_FRAME
} ap_cmd_t;	

int ap_connect_session(int session);
int ap_do(int session, ap_cmd_t cmd);
int ap_get_int(int session, ap_cmd_t cmd, int *val);
int ap_set_int(int session ,ap_cmd_t cmd, int val);
int ap_get_float(int session, ap_cmd_t cmd, float *val);
int ap_set_float(int session, ap_cmd_t cmd, float val);
int ap_get_string(int session, ap_cmd_t cmd, char *val);
int ap_set_string(int session, ap_cmd_t cmd, char *val);
int ap_find_session(char *session_name);
int ap_session_running(int session);

#ifdef __cplusplus
}
#endif

#endif
