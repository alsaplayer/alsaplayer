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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "control.h"
#include "packet.h"
#include "Playlist.h"
#include "error.h"

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
		socklen_t len;
		int fd;
		ap_pkt_t pkt;
		
		socket_thread_running = 1;
	
		if (!playlist) {
			alsaplayer_error("No playlist for control socket\n");
			return;
		}
		unlink("/tmp/alsaplayer_0");
		if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
			saddr.sun_family = AF_UNIX;
			sprintf(saddr.sun_path, "/tmp/alsaplayer_%d", 0);
			if (bind(socket_fd, (struct sockaddr *) &saddr, sizeof (saddr)) != -1) {
				listen(socket_fd, 100);
			} else {
				alsaplayer_error("Error listening on control socket\n");
				return;
			}
		} else {
			alsaplayer_error("Error setting up control socket\n");
			return;
		}	
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
			// Read the data blurb 
			if (pkt.pld_length && ((data = malloc(pkt.pld_length+1)) != NULL)) {
				read(fd, data, pkt.pld_length);
			} else {
				data = NULL;
			}	
			switch(pkt.cmd) {
				case AP_DO_PLAY: 
					player = playlist->GetCorePlayer();
					if (player)
						player->Start();
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
				case AP_DO_CLEAR_PLAYLIST:
					playlist->Clear();
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
						player->SetVolume(*(float *)data);
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
					// Hackery
					pkt.pld_length = strlen("AlsaPlayer");
					write (fd, &pkt, pkt.pld_length);
					write (fd, "AlsaPayer", pkt.pld_length);
					break;
				case AP_SET_STRING_ADD_FILE:
					if (pkt.pld_length && data) {
						char_val = (char *)data;
						char_val[pkt.pld_length] = 0; // Null terminate string
						playlist->Insert(char_val, playlist->Length());
					}	
					break;
				default: alsaplayer_error("CMD = %x\n", pkt.cmd);
					break;
			}
			if (pkt.pld_length)
				free(data);
			close(fd);	
		}
		unlink("/tmp/alsaplayer_0");
}


void control_socket_start(Playlist *playlist)
{
	pthread_create(&socket_thread, NULL, (void * (*)(void *))socket_looper, playlist);
}


void control_socket_stop()
{
	socket_thread_running = 0;
	pthread_join(socket_thread, NULL);
}


