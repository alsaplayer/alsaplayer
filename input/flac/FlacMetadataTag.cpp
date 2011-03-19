//---------------------------------------------------------------------------
//  Provide read-only access to flac stream tags.  Supports flac
//  VORBIS_COMMENT metadata blocks via libFLAC.
//
//  Copyright (c) 2003 by Tim Evans <t.evans@paradise.net.nz>
//
//  Based on FlacId3v1Tag.cpp by Drew Hess <dhess@bothan.net>.
//
//  Changes:
//  June 05, 2005 (Hubert Chan <hubert@uhoreg.ca>)
//    - don't use fixed-length buffers
//    - use an array instead of a long if-else chain
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


#include "FlacMetadataTag.h"

#include <cstring>
#include "reader.h"

extern "C" {
#include <FLAC/metadata.h>
}


static bool
find_vorbis_comment_metadata (const char *filename,
			      FLAC__StreamMetadata **metadata)
{
    FLAC__Metadata_SimpleIterator *flac = FLAC__metadata_simple_iterator_new ();
    if (!flac)
	return false;
    if (!FLAC__metadata_simple_iterator_init (flac, filename, true, false)) {
	FLAC__metadata_simple_iterator_delete (flac);
	return false;
    }

    bool status = false;
    FLAC__MetadataType type;
    do {
	type = FLAC__metadata_simple_iterator_get_block_type (flac);
	if (type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
	    status = true;
	    if (metadata)
		*metadata = FLAC__metadata_simple_iterator_get_block (flac);
	    break;
	}
    }
    while (FLAC__metadata_simple_iterator_next (flac));

    FLAC__metadata_simple_iterator_delete (flac);
    return status;
}


// Split the comment into key and value.  Key and value are newly allocated
// strings and should be deleted after they are no longer needed.
// Returns true if the split was successful; false otherwise.
static bool
parse_comment (const FLAC__StreamMetadata_VorbisComment_Entry *comment,
	       char **key, char **value)
{
    FLAC__byte *sep;
    size_t key_len, value_len;

    if ((sep = (FLAC__byte *) memchr(comment->entry, '=', comment->length))
	== NULL)
      return false;

    key_len = sep - comment->entry;
    value_len = comment->length - key_len - 1;

    (*key) = new char[key_len+1];
    memcpy (*key, comment->entry, key_len);
    (*key)[key_len] = '\0';

    (*value) = new char[value_len+1];
    memcpy (*value, sep + 1, value_len);
    (*value)[value_len] = '\0';

    return true;
}


namespace Flac
{

const FlacMetadataTag::field_mapping_t FlacMetadataTag::field_mappings[] =
{
    { "TITLE", &FlacMetadataTag::_title },
    { "ARTIST", &FlacMetadataTag::_artist },
    { "TRACKNUMBER", &FlacMetadataTag::_track },
    { "ALBUM", &FlacMetadataTag::_album },
    { "YEAR", &FlacMetadataTag::_year },
    { "DESCRIPTION", &FlacMetadataTag::_comment },
    { NULL, NULL }
};


// static
bool
FlacMetadataTag::hasMetadata (const std::string & name)
{
    return find_vorbis_comment_metadata (name.c_str (), NULL);

} // FlacMetadataTag::hasMetadata


FlacMetadataTag::FlacMetadataTag (const std::string & name)
    : FlacTag (name)
{
    FLAC__StreamMetadata *metadata;
    FLAC__StreamMetadata_VorbisComment_Entry *comment;
    unsigned int i,j;
    char *key, *value;

    if (!find_vorbis_comment_metadata (name.c_str (), &metadata))
	return;

    for (i=0; i<metadata->data.vorbis_comment.num_comments; i++) {
	comment = &metadata->data.vorbis_comment.comments[i];
	if (parse_comment(comment, &key, &value)) {
	    // look through the field mappings to see which field we have, and
	    // put it in the appropriate member
	    for (j=0; field_mappings[j].name != NULL; j++) {
	        if (strcmp (field_mappings[j].name, key) == 0)
		{
		    (this->*(field_mappings[j].value)) = value;
		}
	    }
	    delete key;
	    delete value;
	}
    }

    FLAC__metadata_object_delete (metadata);

} // FlacMetadataTag::FlacMetadataTag

} // namespace Flac
