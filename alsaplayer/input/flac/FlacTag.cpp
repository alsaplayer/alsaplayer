//---------------------------------------------------------------------------
//  Provide read-only access to flac file tags.  This is the public interface
//  class for tags.  It's the default tag implementation for flac files with
//  no tag info.  Other tag implementations (e.g. id3, ogg) are provided by 
//  subclasses of this class.
//
//  Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
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

#include "FlacTag.h"
#include "FlacId3Tag.h"


using namespace std;

namespace Flac
{

// static
bool
FlacTag::hasTag (const string & path)
{
    if (FlacId3Tag::hasId3 (path))
	return true;

    return false;

} // FlacTag::hasTag


// static
FlacTag *
FlacTag::newTag (const string & path)
{
    try
    {
	if (FlacId3Tag::hasId3 (path))
	    return new FlacId3Tag (path);

	return new FlacTag (path);
    }
    catch (...)
    {
	return 0;
    }

} // FlacTag::newTag


// static
FlacTag
FlacTag::tag (const string & path)
{
    if (FlacId3Tag::hasId3 (path))
	return FlacId3Tag (path);

    return FlacTag (path);

} // FlacTag::tag


FlacTag::FlacTag (const string & path)
    : _path (path)
{
} // FlacTag::FlacTag

} // namespace Flac
