/*  ControlSocket.h
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

#define SOCKET_CONTROL_BASE_VERSION		0x1000
#define SOCKET_CONTROL_VERSION				(SOCKET_CONTROL_BASE_VERSION + 1)

typedef enum {
	TYPE_INT8 = 0x1,
	TYPE_INT16,
	TYPE_INT32,
	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_NONE
} PayloadType;

typedef enum  {
	CMD_PLAY = 0x1,
	CMD_STOP,
	CMD_NEXT,
	CMD_PREV,
	CMD_PAUSE,
	CMD_PING,
	CMD_GET_NAME,
	CMD_GET_SONGNAME,
	CMD_GET_SONGLENGTH,
	CMD_GET_CURRENTTIME,
	CMD_SET_VOLUME,
	CMD_SET_POS,
	CMD_SET_SPEED,
	CMD_GET_VOLUME,
	CMD_GET_POS,
	CMD_GET_SPEED
} CommandType;	

struct _ap_msg {
	int version;			
	CommandType cmd_type;
	PayloadType pld_type;
	int length;
};	
	
typedef struct _ap_msg ap_msg_t;

int  AP_Connect(int session);
int  AP_SendMessage(int session_id, ap_msg_t *msg, ap_msg_t *reply);

