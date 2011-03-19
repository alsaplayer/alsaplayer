/*  control.cc
 *  Copyright (C) 2006 Austin Bingham <austin.bingham@gmail.com>
 *
 *  This file is part of the python bindings for AlsaPlayer.
 *
 *  The python bindings for AlsaPlayer is free software;
 *  you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software
 *  Foundation; either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  The python bindings for AlsaPlayer is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/ 


#include <alsaplayer/control.h>
#include <boost/bind.hpp>
#include <boost/python.hpp>
#include <sstream>
#include <string>

using namespace boost::python;

namespace
{

  // This is how most of the functions communicate that they
  // have experienced an error. Instead of returning an
  // error code, this is thrown.
  struct APException
  {
    APException(int e,
		std::string msg = std::string()) : 
      err(e),
      message(msg)
    {}

    int err;
    std::string message;
  };
  
  void translator(APException const& e) {
    std::ostringstream oss;

    oss << "ALSAPlayerException: ";
    if (!e.message.empty()) 
      oss << e.message << ", ";
    oss << "id=" << e.err;

    PyErr_SetString(PyExc_UserWarning, oss.str().c_str());
  }
  
  /*
    The rest of these functions serve to put the alsaplayer interface into
    a more pythonic format. For instance, get-functions return values
    rather than modify paramters, they throw exceptions on error, they return
    bools rather than ints where appropriate, and so forth. Otherwise,
    these are basically a thin layer of the alsaplayer C API
  */

  int find_session(const std::string& name)
  {
    int rval;
    int err = ap_find_session(const_cast<char*>(name.c_str()), &rval);
    if (err == 0) throw APException(err);
    return rval;
  }

  bool session_running(int sid)
  {
    return ap_session_running(sid);
  }
  
  template <class T>
  T get(int(*f)(int, T*), int sid)
  {
    T rval;
    int err = f(sid, &rval);
    if (0 == err) throw APException(err);
    return rval;
  }

  template <int MaxSize>
  std::string get_string(int(*f)(int, char*), int sid)
  {
    char result[MaxSize + 1];
    int err = f(sid, result);
    if (0 == err) throw APException(err);
    return std::string(result);
  }

  // TODO: Consider using Boost.PP to simplify this repetition
  int get_playlist_length(int sid) 
  { return get<int>(&ap_get_playlist_length, sid); }

  float get_speed(int sid) 
  { return get<float>(&ap_get_speed, sid); }

  float get_volume(int sid) 
  { return get<float>(&ap_get_volume, sid); }

  float get_pan(int sid) 
  { return get<float>(&ap_get_pan, sid); }

  bool is_looping(int sid) 
  { return get<int>(&ap_is_looping, sid); }

  bool is_playlist_looping(int sid) 
  { return get<int>(&ap_is_playlist_looping, sid); }

  int get_tracks(int sid)
  { return get<int>(&ap_get_tracks, sid); }

  std::string get_session_name(int sid) 
  { return get_string<AP_SESSION_MAX>(ap_get_session_name, sid); }

  std::string get_title(int sid)
  { return get_string<AP_TITLE_MAX>(ap_get_title, sid); }

  std::string get_artist(int sid)
  { return get_string<AP_ARTIST_MAX>(ap_get_artist, sid); }

  std::string get_album(int sid)
  { return get_string<AP_ALBUM_MAX>(ap_get_album, sid); }

  std::string get_genre(int sid)
  { return get_string<AP_GENRE_MAX>(ap_get_genre, sid); }

  std::string get_year(int sid)
  { return get_string<AP_YEAR_MAX>(ap_get_year, sid); }

  std::string get_track_number(int sid)
  { return get_string<AP_TRACK_NUMBER_MAX>(ap_get_track_number, sid); }

  std::string get_comment(int sid)
  { return get_string<AP_COMMENT_MAX>(ap_get_comment, sid); }

  std::string get_file_path(int sid)
  { return get_string<AP_FILE_PATH_MAX>(ap_get_file_path, sid); }

  int get_position(int sid)
  { return get<int>(ap_get_position, sid); }

  int get_length(int sid)
  { return get<int>(ap_get_length, sid); }

  int get_frame(int sid)
  { return get<int>(ap_get_frame, sid); }

  int get_frames(int sid)
  { return get<int>(ap_get_frames, sid); }

  std::string get_stream_type(int sid)
  { return get_string<AP_STREAM_TYPE_MAX>(ap_get_stream_type, sid); }

  std::string get_status(int sid)
  { return get_string<AP_STATUS_MAX>(ap_get_status, sid); }

  bool is_playing(int sid)
  { return get<int>(ap_is_playing, sid); }

  void sort(int sid, const std::string& sz)
  { 
    int err = ap_sort(sid, const_cast<char*>(sz.c_str()));
    if (0 == err) throw APException(err);
  }

  int get_playlist_position(int sid)
  { return get<int>(ap_get_playlist_position, sid); }

  std::string get_file_path_for_track(int sid, int pos)
  {
    char path[AP_FILE_PATH_MAX + 1];
    int err = ap_get_file_path_for_track(sid, path, pos);
    if (0 == err) throw APException(err);
    return std::string(path);
  }

  void insert(int sid, const std::string& sz, int pos)
  {
    int err = ap_insert(sid, const_cast<char*>(sz.c_str()), pos);
    if (0 == err) throw APException(err);
  }

  list get_playlist(int sid)
  {
    int length;
    char** playlist;

    int err = ap_get_playlist(sid, &length, &playlist);
    if (0 == err) throw APException(err);

    list rval;
    for (int idx = 0; idx < length; ++idx)
      {
	rval.append(str(std::string(playlist[idx])));
	free(playlist[idx]);
      }
    free(playlist);
    return rval;
  }
  
}

/* This defines the actual alsaplayer module */
BOOST_PYTHON_MODULE(alsaplayer)
{

  register_exception_translator< ::APException >(::translator);
    
  def("find_session",            
      ::find_session);
  def("session_running",
      ::session_running);
  def("version",
      ap_version);
  def("play",
      ap_play);
  def("stop",
      ap_stop);
  def("pause",                   
      ap_pause);
  def("unpause",
      ap_unpause);
  def("next",
      ap_next);
  def("prev",
      ap_prev);
  def("ping",
      ap_ping);
  def("quit",
      ap_quit);
  def("clear_playlist",
      ap_clear_playlist);
  def("add_path",
      ap_add_path);
  def("add_and_play",
      ap_add_and_play);
  def("add_playlist",
      ap_add_playlist);
  def("shuffle_playlist",
      ap_shuffle_playlist);
  def("save_playlist",
      ap_save_playlist);
  def("get_playlist_length",
      ::get_playlist_length);
  def("set_speed",
      ap_set_speed);
  def("get_speed",
      ::get_speed);
  def("set_volume",
      ap_set_volume);
  def("get_volume",
      ::get_volume);
  def("set_pan",
      ap_set_pan);
  def("get_pan",
      ::get_pan);
  def("set_looping",
      ap_set_looping);
  def("is_looping",
      ::is_looping);
  def("set_playlist_looping",
      ap_set_playlist_looping);
  def("is_playlist_looping",
      ::is_playlist_looping);
  def("get_tracks",
      ::get_tracks);
  def("get_session_name",
      ::get_session_name);
  def("get_title",
      ::get_title);
  def("get_artist",
      ::get_artist);
  def("get_album",
      ::get_album);
  def("get_genre", 
      ::get_genre);
  def("get_year",
      ::get_year);
  def("get_track_number",
      ::get_track_number);
  def("get_comment",
      ::get_comment);
  def("get_file_path", 
      ::get_file_path);
  def("set_position",
      ap_set_position);
  def("get_position",
      ::get_position);
  def("set_position_relative",
      ap_set_position_relative);
  def("get_length",
      ::get_length);
  def("set_frame",
      ap_set_frame);
  def("get_frame",
      ::get_frame);
  def("get_frames",
      ::get_frames);
  def("get_stream_type",
      ::get_stream_type);
  def("get_status",
      ::get_status);
  def("is_playing",
      ::is_playing);
  def("sort",
      ::sort);
  def("jump_to",
      ap_jump_to);
  def("get_playlist_position",
      ::get_playlist_position);
  def("get_file_path_for_track",
      ::get_file_path_for_track);
  def("insert",
      ::insert);
  def("remove",
      ap_remove);
  def("set_current",
      ap_set_current);
  def("get_playlist",
      ::get_playlist);
}

