//-----------------------------------------------------------------------------
//  Provide read-only access to flac stream tags.  This is the public interface
//  class for tags.  It's the default tag implementation for flac streams with
//  no tag info.  Other tag implementations (e.g. id3, ogg) are provided by 
//  subclasses of this class.
//
//  Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
//
/*  This file is part of AlsaPlayer.
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
 */
//----------------------------------------------------------------------------

#include "FlacTag.h"
#include "FlacId3Tag.h"
#include "FlacMetadataTag.h"


using namespace std;

namespace Flac
{

// static
bool
FlacTag::hasTag (const std::string & path)
{
    return FlacId3Tag::hasId3 (path) || FlacMetadataTag::hasMetadata (path);

} // FlacTag::hasTag


// static
FlacTag *
FlacTag::newTag (const std::string & name)
{
    try
    {
	if (FlacId3Tag::hasId3 (name))
	    return new FlacId3Tag (name);
	else if (FlacMetadataTag::hasMetadata (name))
	    return new FlacMetadataTag (name);

	return new FlacTag (name);
    }
    catch (...)
    {
	return 0;
    }

} // FlacTag::newTag


// static
FlacTag
FlacTag::tag (const std::string & name)
{
    if (FlacId3Tag::hasId3 (name))
	return FlacId3Tag (name);
    if (FlacMetadataTag::hasMetadata (name))
	return FlacMetadataTag (name);

    return FlacTag (name);

} // FlacTag::tag


FlacTag::FlacTag (const std::string & name)
    : _name (name)
{
} // FlacTag::FlacTag

} // namespace Flac
