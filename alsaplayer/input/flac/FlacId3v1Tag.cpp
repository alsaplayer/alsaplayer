//---------------------------------------------------------------------------
//  Provide read-only access to flac file tags.  Supports id3v1 tags
//  without any additional libraries.
//
//  Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
//
//  code theft from Josh Coalson's flac xmms plugin (C) 2001 by Josh Coalson
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

#include <stdio.h>
#include <string.h>

using namespace std;

static bool
findId3Tag (FILE * f, char * tag)
{
    if (fseek (f, -128, SEEK_END) != 0)
	return false;
    if (fread (tag, 1, 128, f) != 128)
	return false;
    return strncmp (tag, "TAG", 3) == 0 ? true : false;

} // findId3Tag


namespace Flac
{

typedef struct {
    char raw[128];
    char title[31];
    char artist[31];
    char album[31];
    char comment[31];
    char year[5];
    char track[4];
    char genre[4];
} id3v1_struct;

// static
bool
FlacId3Tag::hasId3 (const string & path)
{
    static char tag[128];

    FILE * f = fopen (path.c_str (), "rb");
    if (!f)
	return false;

    bool status = findId3Tag (f, tag);
    fclose (f);
    return status;

} // FlacId3Tag::hasId3


FlacId3Tag::FlacId3Tag (const string & path)
    : FlacTag (path)
{
    FILE * f = fopen (path.c_str (), "rb");
    if (!f)
	return;

    id3v1_struct tag;
    memset ((void *) & tag, 0, sizeof (id3v1_struct));
    if (!findId3Tag (f, tag.raw))
	return;

    memcpy(tag.title, tag.raw+3, 30);
    memcpy(tag.artist, tag.raw+33, 30);
    memcpy(tag.album, tag.raw+63, 30);
    memcpy(tag.year, tag.raw+93, 4);
    memcpy(tag.comment, tag.raw+97, 30);
    sprintf (tag.genre, "%u", (unsigned char) tag.raw[127]);
    sprintf (tag.track, "%u", (unsigned char) tag.raw[126]);

    _artist  = tag.artist;
    _title   = tag.title;
    _track   = tag.track;
    _album   = tag.album;
    _year    = tag.year;
    _comment = tag.comment;
    _genre   = tag.genre;

} // FlacId3Tag::FlacId3Tag

} // namespace Flac
