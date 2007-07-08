/*  control.h
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
#ifndef __CONTROL_H__
#define __CONTROL_H__

#define AP_SESSION_MAX		256
#define AP_TITLE_MAX		256
#define AP_ARTIST_MAX		256
#define AP_ALBUM_MAX		256
#define AP_GENRE_MAX		256
#define AP_STREAM_TYPE_MAX	256
#define AP_STATUS_MAX		256
#define AP_COMMENT_MAX		256
#define AP_TRACK_NUMBER_MAX	10
#define AP_YEAR_MAX		10
#define AP_FILE_PATH_MAX	1024

#ifdef __cplusplus
extern "C" {
#endif

int ap_find_session(char *session_name, int *session);
int ap_session_running(int session);
int ap_version(void);

int ap_play(int session);
int ap_stop(int session);
int ap_pause(int session);
int ap_unpause(int session);
int ap_next(int session);
int ap_prev(int session);
int ap_ping(int session);
int ap_quit(int session);
int ap_clear_playlist(int session);
int ap_add_path(int session, const char *path);
int ap_add_and_play(int session, const char *path);
int ap_add_playlist(int session, const char *playlistfile);
int ap_shuffle_playlist(int session);
int ap_save_playlist(int session);
int ap_get_playlist_length(int session, int *length);

int ap_set_speed(int session, float speed);
int ap_get_speed(int session, float *val);
int ap_set_volume(int session, float volume);
int ap_get_volume(int session, float *volume);
int ap_set_pan(int session, float pan);
int ap_get_pan(int session, float *pan);

int ap_set_looping(int session, int val);
int ap_is_looping(int session, int *val);
int ap_set_playlist_looping(int session, int val);
int ap_is_playlist_looping(int session, int *val);

int ap_get_tracks(int session, int *nr_tracks);

int ap_get_session_name(int session, char *str);

int ap_get_title(int session, char *str);
int ap_get_artist(int session, char *str);
int ap_get_album(int session, char *str);
int ap_get_genre(int session, char *str);
int ap_get_year(int session, char *str);
int ap_get_track_number(int session, char *str);
int ap_get_comment(int session, char *str);
int ap_get_file_path (int session, char *str);

int ap_set_position(int session, int pos);
int ap_get_position(int session, int *val);
int ap_set_position_relative(int session, int pos);
int ap_get_length(int session, int *length);
int ap_set_frame(int session, int frame);
int ap_get_frame(int session, int *val);
int ap_get_frames(int session, int *val);

int ap_get_stream_type(int session, char *str);
int ap_get_status(int session, char *str);

int ap_is_playing(int session, int *val);

int ap_sort (int session, char *seq);
int ap_jump_to(int session, int pos);

int ap_get_playlist_position(int session, int *pos);
int ap_get_file_path_for_track(int session, char* path, int pos);
int ap_insert(int session, const char*, int pos);
int ap_remove(int session, int pos);
int ap_set_current(int session, int pos);

int ap_get_playlist(int session, int *argc, char ***the_list);

#ifdef __cplusplus
}
#endif

#endif
