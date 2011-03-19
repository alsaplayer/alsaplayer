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
//--------------------------------------------------------------------------

#ifndef _FLAC_TAG_H_
#define _FLAC_TAG_H_

#include <string>

namespace Flac
{

class FlacTag
{
 public:

    //-----------------------------------------------------------
    // Does the stream whose name is given have a recognized tag?
    //-----------------------------------------------------------

    static bool      hasTag (const std::string & name);


    //-----------------------------------------------------------
    // Factory method to construct the proper tag implementation.
    // Returns a pointer to the new tag, or 0 if no tag could
    // be created.
    //-----------------------------------------------------------

    static FlacTag * newTag (const std::string & name);


    //-------------------------------------------------------------
    // Factory method similar to above, but this returns an object.
    // Throws an exception if no tag could be created.
    //-------------------------------------------------------------

    static FlacTag   tag (const std::string & name);


    //-----------------------------------------------------------------
    // Create a flac tag object from the flac stream whose name is
    // given.
    //-----------------------------------------------------------------

    FlacTag (const std::string & name);

    virtual ~FlacTag ();


    //--------------------------------------------------------------------
    // Access methods for tag fields.
    //
    // Any tags which are not present in the stream are returned as empty
    // strings.
    //--------------------------------------------------------------------

    const std::string artist ();
    const std::string title ();
    const std::string track ();
    const std::string album ();
    const std::string year ();
    const std::string comment ();
    const std::string genre ();

    
 protected:

    const std::string _name;
    std::string       _artist;
    std::string       _title;
    std::string       _track;
    std::string       _album;
    std::string       _year;
    std::string       _comment;
    std::string       _genre;

}; // class FlacTag


//----------------
// Inline methods.
//----------------

inline
FlacTag::~FlacTag ()
{
}

inline const std::string
FlacTag::artist ()
{
    return _artist;
}

inline const std::string
FlacTag::title ()
{
    return _title;
}

inline const std::string
FlacTag::track ()
{
    return _track;
}

inline const std::string
FlacTag::album ()
{
    return _album;
}

inline const std::string
FlacTag::year ()
{
    return _year;
}

inline const std::string
FlacTag::comment ()
{
    return _comment;
}

inline const std::string
FlacTag::genre ()
{
    return _genre;
}

} // namespace Flac

#endif // _FLAC_TAG_H_

