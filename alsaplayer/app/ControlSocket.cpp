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
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <pwd.h>
#include "control.h"
#include "packet.h"
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
	void *data;
	float float_val;
	char *char_val;
	char session_name[32];
	int long_val;
	int int_val;
	socklen_t len;
	int fd;
	int session_id = 0;
	int session_ok = 0;
	ap_pkt_t pkt;
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
			float_val = 0.0;
			if ((int_val = ap_get_float(session_id, AP_GET_FLOAT_PING, &float_val)) == -1) {
				//alsaplayer_error("found stale session %d", session_id);
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
		read(fd, &pkt, sizeof(ap_pkt_t));

		// Check version
		if (pkt.version != AP_CONTROL_VERSION) {
			alsaplayer_error("protocl version mismatch (server): %x != %x",
				pkt.version, AP_CONTROL_VERSION);
			close(fd);
			continue;
		}	
		// Read the data blurb 
		if (pkt.pld_length && ((data = malloc(pkt.pld_length+1)) != NULL)) {
			read(fd, data, pkt.pld_length);
		} else {
			data = NULL;
		}
		switch(pkt.cmd) {
			case AP_DO_PLAY: 
				player = playlist->GetCorePlayer();
				if (player && !player->IsPlaying()) {
					player->Start();
				}	
				break;
			case AP_DO_NEXT: 
				playlist->Next(1);
				break;
			case AP_DO_PREV:
				playlist->Prev(1);
				break;
			case AP_DO_STOP:
				player = playlist->GetCorePlayer();
				if (player)
					player->Stop();
				break;
			case AP_DO_PAUSE:
				player = playlist->GetCorePlayer();
				if (player)
					player->SetSpeed(0.0);
				break;
			case AP_DO_UNPAUSE:
				player = playlist->GetCorePlayer();
				if (player)
					player->SetSpeed(1.0);
				break;	
			case AP_DO_CLEAR_PLAYLIST:
				playlist->Clear(1);
				break;
			case AP_DO_QUIT:
				// Woah, this is very dirty! XXX FIXME XXX
				exit(0); 
				break;
			case AP_GET_FLOAT_PING:
				// Perhaps return the running time here
				float_val = 1.0;
				pkt.pld_length = sizeof(float);
				write (fd, &pkt, sizeof(ap_pkt_t));
				write(fd, &float_val, pkt.pld_length);
				break;
			case AP_SET_FLOAT_SPEED:
				player = playlist->GetCorePlayer();
				if (player)
					player->SetSpeed(*(float *)data);
				break;
			case AP_GET_FLOAT_SPEED:
				player = playlist->GetCorePlayer();
				if (player) {
					float_val = player->GetSpeed();
					pkt.pld_length = sizeof(float);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write(fd, &float_val, pkt.pld_length);
				}
				break;
			case AP_SET_FLOAT_VOLUME:
				player = playlist->GetCorePlayer();
				if (player)
					player->SetVolume((int)(*(float *)data));
				break;
			case AP_GET_FLOAT_VOLUME:
				player = playlist->GetCorePlayer();
				if (player) {
					float_val = player->GetVolume();
					pkt.pld_length = sizeof(float);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write(fd, &float_val, pkt.pld_length);
				}
				break;
			case AP_GET_STRING_SONG_NAME:
				player = playlist->GetCorePlayer();
				if (player) {
					player->GetStreamInfo(&info);
					pkt.pld_length = strlen(info.title);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write(fd, info.title, sizeof(info.title));
				}	
				break;
			case AP_GET_STRING_SESSION_NAME:
				if (global_session_name) {
					pkt.pld_length = strlen(global_session_name);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write (fd, global_session_name, pkt.pld_length);
				} else {
					sprintf(session_name, "alsaplayer-%d", global_session_id);
					pkt.pld_length = strlen(session_name);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write (fd, session_name, pkt.pld_length);
				}	
				break;
			case AP_SET_STRING_ADD_FILE:
				if (pkt.pld_length && data) {
					char_val = (char *)data;
					char_val[pkt.pld_length] = 0; // Null terminate string
					if (strlen(char_val)) {
						playlist->Insert(char_val, playlist->Length());
					}
					playlist->UnPause();
				}	
				break;
			case AP_GET_INT_POS_SECOND:
				player = playlist->GetCorePlayer();
				if (player) {
					int_val = player->GetCurrentTime() / 100;
					pkt.pld_length = sizeof(int);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write (fd, &int_val, sizeof(int));
				}
				break;
			case AP_SET_INT_POS_SECOND:
				player = playlist->GetCorePlayer();
				if (player) {
					if (pkt.pld_length && data) {
						int_val = *(int *)data;
						int_val *= player->GetSampleRate();
						int_val /= player->GetFrameSize();
						int_val *= player->GetChannels();
						int_val *= 2; // 16-bit ("2" x 8-bit)
						player->Seek(int_val);
					}
				}
				break;
			case AP_GET_INT_SONG_LENGTH_SECOND:
				player = playlist->GetCorePlayer();
				if (player) {
					long_val = player->GetCurrentTime(
							player->GetFrames());
					// we get centiseconds
					int_val = (int)(long_val / 100);
					pkt.pld_length = sizeof(int);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write (fd, &int_val, sizeof(int));
				}
				break;
			case AP_GET_INT_SONG_LENGTH_FRAME:
				player = playlist->GetCorePlayer();
				if (player) {
					int_val = player->GetFrames();
					pkt.pld_length = sizeof(int);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write (fd, &int_val, sizeof(int));
				}
				break;
			case AP_GET_INT_POS_FRAME:
				player = playlist->GetCorePlayer();
				if (player) {
					int_val = player->GetPosition();
					pkt.pld_length = sizeof(int);
					write (fd, &pkt, sizeof(ap_pkt_t));
					write (fd, &int_val, sizeof(int));
				}
				break;
			case AP_SET_INT_POS_FRAME:
				player = playlist->GetCorePlayer();
				if (player) {
					if (pkt.pld_length && data) {
						int_val = *(int *)data;
						player->Seek(int_val);
					}
				}
				break;
			default: alsaplayer_error("CMD unknown or not implemented= %x",
					pkt.cmd);
				 break;
		}
		if (pkt.pld_length)
			free(data);
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


