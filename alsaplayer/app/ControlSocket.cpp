/*  ControlSocket.cpp
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
#include "AlsaPlayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <signal.h>
#include <pwd.h>
#include "control.h"
#include "message.h"
#include "Playlist.h"
#include "alsaplayer_error.h"

#define MAX_AP_SESSIONS 1024

static pthread_t socket_thread;
static int socket_thread_running = 0;

void socket_looper(void *arg)
{
	int socket_fd = 0;

	Playlist *playlist = (Playlist *)arg;
	CorePlayer *player;
	fd_set set;
	struct timeval tv;
	struct sockaddr_un saddr;
	stream_info info;
	char session_name[32];
	long total_time;
	void *data;
	float *float_val;
	float save_speed = 0.0;
	char *path;
	int *long_val;
	int *int_val;
	socklen_t len;
	int fd;
	int session_id = 0;
	int session_ok = 0;
	int return_value;
	ap_message_t *msg;
	struct passwd *pwd;

	socket_thread_running = 1;

	if (!playlist) {
		alsaplayer_error("No playlist for control socket");
		return;
	}

	pwd = getpwuid(geteuid());

	if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
		saddr.sun_family = AF_UNIX;
		while (session_id < MAX_AP_SESSIONS && !session_ok) {
			if (!ap_ping(session_id)) {
				sprintf(saddr.sun_path, "/tmp/alsaplayer_%s_%d", pwd == NULL ?
						"anonymous" : pwd->pw_name, session_id);
				unlink(saddr.sun_path); // Clean up a bit
			} else {
				// Live session so skip it immediately
				session_id++;
				continue;
			}	
			sprintf(saddr.sun_path, "/tmp/alsaplayer_%s_%d", pwd == NULL ? 
					"anonymous" : pwd->pw_name, session_id);
			if (bind(socket_fd, (struct sockaddr *) &saddr, sizeof (saddr)) != -1) {
				chmod(saddr.sun_path, 00600); // Force permission
				listen(socket_fd, 100);
				session_ok = 1;
			} else {
				session_id++;
			}
		}
		if (!session_ok) {
			alsaplayer_error("Out of alsaplayer sockets (MAX = %d)", MAX_AP_SESSIONS);
			return;
		}	
	} else {
		alsaplayer_error("Error setting up control socket");
		return;
	}
	global_session_id = session_id;

	while (socket_thread_running) {
		FD_ZERO(&set);
		FD_SET(socket_fd, &set);
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		len = sizeof (saddr);
		data = NULL;

		if ((select(socket_fd + 1, &set, NULL, NULL, &tv) <= 0) ||
				((fd = accept(socket_fd, (struct sockaddr *) &saddr, &len)) == -1))
			continue;
		// So we have a connection
		msg = ap_message_receive(fd);

		// Check version
		if (msg->header.version != AP_CONTROL_VERSION) {
			alsaplayer_error("protocl version mismatch (server): %x != %x",
				msg->header.version, AP_CONTROL_VERSION);
			close(fd);
			continue;
		}	
		
		ap_message_t *reply = ap_message_new();
	
		//alsaplayer_error("server: got something (%x)", msg->header.cmd);
	
		player = playlist->GetCorePlayer();
	
		switch(msg->header.cmd) {
			case AP_PING:
				ap_message_add_int32(reply, "pong", 10281975);
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_ADD_PATH:
				if ((path = ap_message_find_string(msg, "path1"))) {
					playlist->Insert(strdup(path), playlist->Length());
				}	
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_PLAY: 
				if (player) {
					if (player->IsPlaying()) {
						if (player->GetSpeed() == 0.0) {
							player->SetSpeed(1.0);
							ap_message_add_int32(reply, "ack", 1);
							break;
						}
						player->Seek(0);
						ap_message_add_int32(reply, "ack", 1);
						break;
					} else {
						ap_message_add_int32(reply, "ack",
							player->Start() ? 1 : 0);
						break;
					}
				}
				ap_message_add_int32(reply, "ack", 0);
				break;
			case AP_NEXT: 
				playlist->Next(1);
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_PREV:
				playlist->Prev(1);
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_STOP:
				if (player)
					player->Stop();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_PAUSE:
				if (player) {
					save_speed = player->GetSpeed();
					player->SetSpeed(0.0);
				}	
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_UNPAUSE:
				if (player) {
					if (save_speed)
						player->SetSpeed(save_speed);
					else	
						player->SetSpeed(1.0);
				}		
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_CLEAR_PLAYLIST:
				playlist->Clear(1);
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_QUIT:
				// Woah, this is very dirty! XXX FIXME XXX
				kill(0, 1);
				break;
			case AP_SET_SPEED:
				if ((float_val=ap_message_find_float(msg, "speed"))) {
					player = playlist->GetCorePlayer();
					if (player) {
						player->SetSpeed(*float_val);
						ap_message_add_int32(reply, "ack", 1);	
					}	
				}
				break;
			case AP_GET_SPEED:
				if (player) {
					ap_message_add_float(reply, "speed", player->GetSpeed());
					ap_message_add_int32(reply, "ack", 1);
				}	
				break;
			case AP_IS_PLAYING:
				if (player) {
					ap_message_add_int32(reply, "int", player->IsPlaying());
					ap_message_add_int32(reply, "ack", 1);
				}	
				break;
			case AP_SET_VOLUME:
				if ((float_val=ap_message_find_float(msg, "volume"))) {
					player = playlist->GetCorePlayer();
					if (player) {
						player->SetVolume((int)*float_val);
						ap_message_add_int32(reply, "ack", 1);
					}
				}
				break;
			case AP_GET_VOLUME:
				if (player) {
					ap_message_add_float(reply, "volume", player->GetVolume());
					ap_message_add_int32(reply, "ack", 1);
				}	
				break;
			case AP_GET_TITLE:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.title);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_ARTIST:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.artist);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_ALBUM:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.album);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_COMMENT:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.comment);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_GENRE:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.genre);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_YEAR:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.year);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_TRACK_NUMBER:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.track);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_STREAM_TYPE:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.stream_type);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_STATUS:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.status);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_SESSION_NAME:
				if (global_session_name) {
					ap_message_add_string(reply, "string", global_session_name);
				} else {
					sprintf(session_name, "alsaplayer-%d", global_session_id);
					ap_message_add_string(reply, "string", session_name);
				}
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_GET_POS_SECOND:	
				if (player) {
					ap_message_add_int32(reply, "int", 
						player->GetCurrentTime() / 100);
					ap_message_add_int32(reply, "ack", 1);
				}	
				break;
			case AP_SET_POS_SECOND_RELATIVE:
				if (player) {
					if ((int_val = ap_message_find_int32(msg, "int"))) {
						*int_val += ( player->GetCurrentTime() / 100);
						*int_val *= player->GetSampleRate();
						*int_val /= player->GetFrameSize();
						*int_val *= player->GetChannels();
						*int_val *= 2; // 16-bit ("2" x 8-bit)a
						if (*int_val < 0)
							*int_val = 0;
						player->Seek(*int_val);
					}
				}
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_SET_POS_SECOND:
				if (player) {
					if ((int_val = ap_message_find_int32(msg, "int"))) {
						*int_val *= player->GetSampleRate();
						*int_val /= player->GetFrameSize();
						*int_val *= player->GetChannels();
						*int_val *= 2; // 16-bit ("2" x 8-bit)
						player->Seek(*int_val);
					}
				}
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_GET_SONG_LENGTH_SECOND:
				if (player) {
					total_time = player->GetCurrentTime(player->GetFrames());
					ap_message_add_int32(reply, "int", (int32_t)(total_time / 100));
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_SONG_LENGTH_FRAME:
				if (player) {
					ap_message_add_int32(reply, "int", player->GetFrames());
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_POS_FRAME:
				if (player) {
					ap_message_add_int32(reply, "int", player->GetPosition());
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_SET_POS_FRAME:
				if (player) {
					if ((int_val = ap_message_find_int32(msg, "int"))) {
						player->Seek(*int_val);
						ap_message_add_int32(reply, "ack", 1);
					}
				}
				break;
			default: alsaplayer_error("CMD unknown or not implemented= %x",
					msg->header.cmd);
				 break;
		}
		//alsaplayer_error("Sending reply");
		ap_message_send(fd, reply);
		ap_message_delete(reply);
		ap_message_delete(msg);
		close(fd);	
	}
	unlink(saddr.sun_path);
}


void control_socket_start(Playlist *playlist)
{
	pthread_create(&socket_thread, NULL, (void * (*)(void *))socket_looper, playlist);
}


void control_socket_stop()
{
	socket_thread_running = 0;
	pthread_cancel(socket_thread);
	pthread_join(socket_thread, NULL);
}


