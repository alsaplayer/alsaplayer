//---------------------------------------------------------------------------
//  Provide read-only access to flac stream tags.  Supports flac
//  VORBIS_COMMENT metadata blocks via libFLAC.  Based on FlacId3Tag.h.
//
//  Copyright (c) 2003 by Tim Evans <t.evans@paradise.net.nz>
//
//  Based on FlacId3Tag.h by Drew Hess <dhess@bothan.net>.
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


#ifndef _FLAC_METADATA_TAG_H_
#define _FLAC_METADATA_TAG_H_

#include "FlacTag.h"

namespace Flac
{

class FlacMetadataTag : public FlacTag
{

    // Map metadata names to the variable that they refer to
    typedef struct
    {
        const char *name;
        std::string FlacMetadataTag::* value;
    }
    field_mapping_t;

    static const field_mapping_t field_mappings[];

 public:

    FlacMetadataTag (const std::string & name);

    virtual ~FlacMetadataTag ();

 protected:

    //--------------------------------------------------------------
    // Return true if the flac stream whose name is given has a
    // metadata tag.
    //--------------------------------------------------------------

    static bool hasMetadata (const std::string & name);

    friend class FlacTag;

}; // class FlacMeatadataTag


//----------------
// Inline methods.
//----------------

inline
FlacMetadataTag::~FlacMetadataTag ()
{
}

} // namespace Flac

#endif // _FLAC_METADATA_TAG_H_

