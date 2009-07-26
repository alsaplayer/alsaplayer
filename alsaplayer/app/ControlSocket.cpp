/*  ControlSocket.cpp
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
#include "AlsaPlayer.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <csignal>
#include <pwd.h>
#include <limits.h>
#include "control.h"
#include "message.h"
#include "Playlist.h"
#include "interface_plugin.h"
#include "alsaplayer_error.h"

#define MAX_AP_SESSIONS 1024

static pthread_t socket_thread;
static int socket_thread_running = 0;


struct socket_params
{
	Playlist *playlist;
	interface_plugin *ui;
};

static void socket_looper(void *arg)
{
	int socket_fd = 0;
	struct socket_params *sp = (struct socket_params *)arg;
	Playlist *playlist = sp->playlist;
	PlayItem *item;
	interface_plugin *ui = sp->ui;
	CorePlayer *player;
	fd_set set;
	struct sockaddr_un saddr;
	stream_info info;
	char session_name[32];
	char strnum[64];
	char tmp[512];
	long total_time;
	void *data;
	float *float_val;
	char *path;
	int32_t *int_val;
	socklen_t len;
	int fd;
	int fsize = 0;
	int session_id = 0;
	int session_ok = 0;
	int nr_requests = 0;
	int playlist_length = 0;
	int pc = 0;
	ap_message_t *msg;
	struct passwd *pwd;

	socket_thread_running = 1;

	if (!playlist) {
		alsaplayer_error("No playlist for control socket");
		return;
	}

	pwd = getpwuid(geteuid());

	if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		alsaplayer_error("Error setting up control socket");
		return;
	}
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
	global_session_id = session_id;

	while (socket_thread_running) {
		FD_ZERO(&set);
		FD_SET(socket_fd, &set);
		len = sizeof (saddr);
		data = NULL;

		if ((select(socket_fd + 1, &set, NULL, NULL, NULL) <= 0) ||
				((fd = accept(socket_fd, (struct sockaddr *) &saddr, &len)) == -1))
			continue;
		// So we have a connection
		msg = ap_message_receive(fd);

		// Check version
		if (msg->header.version != AP_CONTROL_VERSION) {
			alsaplayer_error("protocol version mismatch (server): %x != %x",
				msg->header.version, AP_CONTROL_VERSION);
			close(fd);
			continue;
		}	

		ap_message_t *reply = ap_message_new();

		//alsaplayer_error("server: got something (%x)", msg->header.cmd);
	
		player = playlist->GetCorePlayer();

		nr_requests++;

		switch(msg->header.cmd) {
			case AP_PING:
				//alsaplayer_error("ping received");
				ap_message_add_int32(reply, "pong", 10281975);
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_ADD_AND_PLAY:
				if ((path = ap_message_find_string(msg, "path1"))) {
					playlist->AddAndPlay(strdup(path));
				}
				playlist->UnPause();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_SORT:
				if ((path = ap_message_find_string(msg, "seq"))) {
					playlist->Sort(path);
				}
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_ADD_PATH:
				if ((path = ap_message_find_string(msg, "path1"))) {
					playlist->Insert(path, playlist->Length());
				}	
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_ADD_PLAYLIST:
				if ((path = ap_message_find_string(msg, "path1"))) {
					playlist->Load(path, playlist->Length(), 0);
				}	
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_PLAY: 
				if (player) {
					playlist->UnPause();
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
				playlist->Next();
				playlist->UnPause();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_PREV:
				playlist->Prev();
				playlist->UnPause();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_JUMP_TO:
				if ((int_val = ap_message_find_int32(msg, "int"))) {
					playlist->Play(*int_val);
					playlist->UnPause();
					ap_message_add_int32(reply, "ack", 1);
				} else {
					ap_message_add_int32(reply, "ack", 0);
				}	
				break;
			case AP_SHUFFLE_PLAYLIST:
				playlist->Shuffle();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_STOP:
				playlist->Stop();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_PAUSE:
				playlist->Pause();
				if (player)
					player->Pause();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_UNPAUSE:
				playlist->UnPause();
				if (player) 
					player->UnPause();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_CLEAR_PLAYLIST:
				playlist->Clear();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_SAVE_PLAYLIST:
				if (getenv("HOME") == NULL) {
					ap_message_add_int32(reply, "ack", 0);
				} else {
					char save_path[PATH_MAX];
					snprintf(save_path, sizeof(save_path)-1, "%s/.alsaplayer/latest.m3u", getenv("HOME"));
					playlist->Save(save_path, PL_FORMAT_M3U);
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_QUIT:
				// Woah, this is very dirty! XXX FIXME XXX
				if (ui)
					ui->stop();
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
			case AP_IS_PAUSED:
                                ap_message_add_int32(reply, "int", playlist->IsPaused());
                                ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_IS_PLAYING:
				if (player) {
					ap_message_add_int32(reply, "int", player->IsPlaying());
					ap_message_add_int32(reply, "ack", 1);
				}	
				break;
			case AP_IS_LOOPING:
				ap_message_add_int32(reply, "int", playlist->LoopingSong());
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_SET_LOOPING:
				if ((int_val = ap_message_find_int32(msg, "int"))) {
					if (*int_val) {
						playlist->LoopSong();
					} else {
						playlist->UnLoopSong();
					}
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_GET_PLAYLIST:
				playlist->Lock();
				playlist_length = playlist->Length();
				ap_message_add_int32(reply, "items", playlist->Length());
				for (pc=1; pc <= playlist_length; pc++) {
					sprintf(strnum, "%d", pc);
					item = playlist->GetItem(pc);
					if (item->title.size()) {
						sprintf(tmp, "%s %s", item->title.c_str(), 
			                        item->artist.size() ? (std::string("- ") + 
						item->artist).c_str() : "");
						ap_message_add_string(reply, strnum, tmp);
					} else {
						sprintf(tmp, "%s", item->filename.c_str());
						path = strrchr(tmp, '/');
						if (path) {
							path++;
						} else {
							path = tmp;
						}	
						ap_message_add_string(reply, strnum, path);
					}	
				}
				playlist->Unlock();
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_IS_PLAYLIST_LOOPING:
				ap_message_add_int32(reply, "int", playlist->LoopingPlaylist());
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_SET_PLAYLIST_LOOPING:
				if ((int_val = ap_message_find_int32(msg, "int"))) {
					if (*int_val) {
						playlist->LoopPlaylist();
					} else {
						playlist->UnLoopPlaylist();
					}
					ap_message_add_int32(reply, "ack", 1);
				}
				break;
			case AP_SET_VOLUME:
				if (player) {
					if ((float_val = ap_message_find_float(msg, "float"))) {
						player->SetVolume (*float_val);
						ap_message_add_int32(reply, "ack", 1);
					}
				}
				break;
			case AP_GET_VOLUME:
				if (player) {
					ap_message_add_float(reply, "float",  player->GetVolume());
					ap_message_add_int32(reply, "ack", 1);
				}	
				break;
			case AP_SET_PAN:
				if (player) {
					if ((float_val = ap_message_find_float(msg, "float"))) {
						player->SetPan (*float_val);
						ap_message_add_int32(reply, "ack", 1);
					}
				}
				break;
			case AP_GET_PAN:
				if (player) {
					ap_message_add_float(reply, "float",  player->GetPan());
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
			case AP_GET_FILE_PATH:
				if (player) {
					player->GetStreamInfo(&info);
					ap_message_add_string(reply, "string", info.path);
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
			case AP_GET_TRACKS:
				if (player) {
					ap_message_add_int32(reply, "int", player->GetTracks());
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
						fsize = player->GetFrameSize();
						if (fsize) {
							*int_val += ( player->GetCurrentTime() / 100);
							*int_val *= player->GetSampleRate();
							*int_val /= fsize;
							*int_val *= player->GetChannels();
							*int_val *= 2; // 16-bit ("2" x 8-bit)a
							if (*int_val < 0)
								*int_val = 0;
							player->Seek(*int_val);
						}	
					}
				}
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_SET_POS_SECOND:
				if (player) {
					if ((int_val = ap_message_find_int32(msg, "int"))) {
						fsize = player->GetFrameSize();
						if (fsize) {
							*int_val *= player->GetSampleRate();
							*int_val /= fsize;
							*int_val *= player->GetChannels();
							*int_val *= 2; // 16-bit ("2" x 8-bit)
							player->Seek(*int_val);
						}	
					}
				}
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_GET_PLAYLIST_LENGTH:
				ap_message_add_int32(reply, "int", (int32_t)playlist->Length());
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
			case AP_GET_PLAYLIST_POSITION:
				ap_message_add_int32(reply, "int", (int32_t)playlist->GetCurrent());
				ap_message_add_int32(reply, "ack", 1);
				break;
			case AP_GET_FILE_PATH_FOR_TRACK:
				if((int_val = ap_message_find_int32(msg, "int"))) {
					if(0 < *int_val && *int_val <= playlist->Length()) {
						ap_message_add_string(reply, "string", playlist->GetQueue()[(*int_val)-1].filename.c_str());
						ap_message_add_int32(reply, "ack", 1);
					}
					else {
						ap_message_add_int32(reply, "ack", 0);
					}
				}
				break;
			case AP_INSERT:
				if((int_val = ap_message_find_int32(msg, "int"))) {
					if(-1 < *int_val && *int_val <= playlist->Length()) {
						path = ap_message_find_string(msg, "string");
						playlist->Insert(path, *int_val);
						ap_message_add_int32(reply, "ack", 1);
					}
					else {
						ap_message_add_int32(reply, "ack", 0);
					}
				}
				break;
			case AP_REMOVE:
				if((int_val = ap_message_find_int32(msg, "int"))) {
					if(0 < *int_val && *int_val <= playlist->Length()) {
						playlist->Remove(*int_val, *int_val);
						ap_message_add_int32(reply, "ack", 1);
					}
					else {
						ap_message_add_int32(reply, "ack", 0);
					}
				}
				break;
			case AP_SET_CURRENT:
				if((int_val = ap_message_find_int32(msg, "int"))) {
					if(0 < *int_val && *int_val <= playlist->Length()) {
						playlist->SetCurrent(*int_val);
						ap_message_add_int32(reply, "ack", 1);
					}
					else {
						ap_message_add_int32(reply, "ack", 0);
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
	if (global_verbose) {
		alsaplayer_error("control: received %ld requests", nr_requests);
	}		
	unlink(saddr.sun_path);
}


void control_socket_start(Playlist *playlist, interface_plugin *ui)
{
	static struct socket_params sp;

	sp.playlist = playlist;
	sp.ui = ui;
	pthread_create(&socket_thread, NULL, (void * (*)(void *))socket_looper, &sp);
}


void control_socket_stop()
{
	socket_thread_running = 0;
	pthread_cancel(socket_thread);
	pthread_join(socket_thread, NULL);
}


