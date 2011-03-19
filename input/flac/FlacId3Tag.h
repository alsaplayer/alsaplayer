//---------------------------------------------------------------------------
//  Provide read-only access to flac stream tags.  Supports id3v1 tags
//  without any additional libraries.  Supports the id3v1 subset of
//  fields for id3v2 tags if libid3tag is available.
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


#ifndef _FLAC_ID3_TAG_H_
#define _FLAC_ID3_TAG_H_

#include "FlacTag.h"

namespace Flac
{

class FlacId3Tag : public FlacTag
{
 public:

    FlacId3Tag (const std::string & name);

    virtual ~FlacId3Tag ();

 protected:

    //--------------------------------------------------------------
    // Return true if the flac stream whose name is given has an id3
    // tag.
    //--------------------------------------------------------------

    static bool hasId3 (const std::string & name);

    friend class FlacTag;

}; // class FlacId3Tag


//----------------
// Inline methods.
//----------------

inline
FlacId3Tag::~FlacId3Tag ()
{
}

} // namespace Flac

#endif // _FLAC_ID3_TAG_H_

