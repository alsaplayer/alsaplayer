//---------------------------------------------------------------------------
//  Provide read-only access to flac file tags.  Supports id3v1 and the
//  id3v1 field subset of id3v2 via the id3tag library.
//
//  Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
//
//  based on mad_engine.c (C) 2002 by Andy Lo A Foe
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//--------------------------------------------------------------------------


#include "FlacId3Tag.h"

#include <id3tag.h>

using namespace std;


static void
toStr (struct id3_tag * tag, const string & frameName, string & str)
{

    // You have gotta be kidding me.  The id3tag library just might
    // have the worst interface I've ever seen.  Apologies to anyone
    // who ever taught me how to write code.  I'd much rather use id3lib,
    // but a) alsaplayer already uses id3tag and b) id3lib 3.7 on
    // my Debian system is broken. :<

    struct id3_frame const * frame = id3_tag_findframe (tag, frameName.c_str (), 0);
    if (!frame)
	return;
    
    id3_ucs4_t const * ucs4 = 0;
    if (frameName == ID3_FRAME_COMMENT)
    {
	ucs4 = id3_field_getstring (&frame->fields[2]);
	if (!ucs4)
	    return;
	ucs4 = id3_field_getfullstring (&frame->fields[3]);
	if (!ucs4)
	    return;
    }
    else if (frameName == ID3_FRAME_GENRE)
    {
	ucs4 = id3_field_getstrings (&frame->fields[1], 0);
	if (!ucs4)
	    return;
	ucs4 = id3_genre_name (ucs4);
	if (!ucs4)
	    return;
    }
    else
    {
	ucs4 = id3_field_getstrings (&frame->fields[1], 0);
	if (!ucs4)
	    return;
    }

    str = (char *) id3_ucs4_latin1duplicate (ucs4);
    return;

} // toStr


namespace Flac
{

// static
bool
FlacId3Tag::hasId3 (const string & path)
{
    struct id3_file * id3;

    id3 = id3_file_open (path.c_str (), ID3_FILE_MODE_READONLY);
    if (!id3)
	return false;
    
    bool status;
    struct id3_tag * tag = id3_file_tag (id3);
    if (!tag)
	status = false;
    else
	bool status = tag->nframes;

    id3_file_close (id3);
    
    return status;

} // FlacId3Tag::hasId3


FlacId3Tag::FlacId3Tag (const string & path)
    : FlacTag (path)
{
  struct id3_file * id3= id3_file_open (path.c_str (), ID3_FILE_MODE_READONLY);
  if (!id3)
      return;

  struct id3_tag * tag = id3_file_tag (id3);
  if (!tag || tag->nframes == 0)
  {
      id3_file_close (id3);
      return;
  }
  
  toStr (tag, ID3_FRAME_ALBUM,   _album);
  toStr (tag, ID3_FRAME_TITLE,   _title);
  toStr (tag, ID3_FRAME_ARTIST,  _artist);
  toStr (tag, ID3_FRAME_TRACK,   _track);
  toStr (tag, ID3_FRAME_YEAR,    _year);
  toStr (tag, ID3_FRAME_COMMENT, _comment);
  toStr (tag, ID3_FRAME_GENRE,   _genre);

  id3_file_close (id3);

} // FlacId3Tag::FlacId3Tag

} // namespace Flac
