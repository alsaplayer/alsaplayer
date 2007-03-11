#include <stdio.h>
#include "ttalib.h"
#include "id3genre.h"

/***********************************************************************
 * ID3 tags manipulation routines
 *
 * Provides read access to ID3v1 tags v1.1, ID3v2 tags v2.3.x and above
 * Supported ID3v2 frames: Title, Artist, Album, Track, Year,
 *                         Genre, Comment.
 *
 **********************************************************************/

static unsigned int unpack_sint28 (const char *ptr) {
	unsigned int value = 0;

	if (ptr[0] & 0x80) return 0;

	value =  value       | (ptr[0] & 0x7f);
	value = (value << 7) | (ptr[1] & 0x7f);
	value = (value << 7) | (ptr[2] & 0x7f);
	value = (value << 7) | (ptr[3] & 0x7f);

	return value;
}

static void pack_sint32 (unsigned int value, char *ptr) {
	ptr[0] = (value >> 24) & 0xff;
	ptr[1] = (value >> 16) & 0xff;
	ptr[2] = (value >>  8) & 0xff;
	ptr[3] = (value & 0xff);
}

static unsigned int unpack_sint32 (const char *ptr) {
	unsigned int value = 0;

	if (ptr[0] & 0x80) return 0;

	value = (value << 8) | ptr[0];
	value = (value << 8) | ptr[1];
	value = (value << 8) | ptr[2];
	value = (value << 8) | ptr[3];

	return value;
}

static int get_frame_id (const char *id) {
	if (!memcmp(id, "TIT2", 4)) return TIT2;	// Title
	if (!memcmp(id, "TPE1", 4)) return TPE1;	// Artist
	if (!memcmp(id, "TALB", 4)) return TALB;	// Album
	if (!memcmp(id, "TRCK", 4)) return TRCK;	// Track
	if (!memcmp(id, "TYER", 4)) return TYER;	// Year
	if (!memcmp(id, "TCON", 4)) return TCON;	// Genre
	if (!memcmp(id, "COMM", 4)) return COMM;	// Comment
	return 0;
}

void get_id3v1_tag (tta_info *ttainfo) {
	id3v1_tag id3v1;

	long  pos = ftell(ttainfo->HANDLE);

	memset (&ttainfo->id3v1, 0, sizeof (ttainfo->id3v1));

	fseek (ttainfo->HANDLE, -(int) sizeof(id3v1_tag), SEEK_END);
	if ((fread (&id3v1, 1, sizeof(id3v1_tag), ttainfo->HANDLE) != 0) &&
		!memcmp(id3v1.id, "TAG", 3)) {
		memcpy(ttainfo->id3v1.title, id3v1.title, 30);
		memcpy(ttainfo->id3v1.artist, id3v1.artist, 30);
		memcpy(ttainfo->id3v1.album, id3v1.album, 30);
		memcpy(ttainfo->id3v1.year, id3v1.year, 4);
		memcpy(ttainfo->id3v1.comment, id3v1.comment, 28);
		ttainfo->id3v1.track = id3v1.track;
		ttainfo->id3v1.genre = id3v1.genre;
		ttainfo->id3v1.id3has = 1;
	}
	fseek(ttainfo->HANDLE, pos, SEEK_SET);
}

int get_id3v2_tag (tta_info *ttainfo) {
	id3v2_tag id3v2;
	id3v2_frame frame_header;
	int id3v2_size;
	unsigned char *buffer, *ptr;

	if (!fread(&id3v2, 1, sizeof (id3v2_tag), ttainfo->HANDLE) || 
	    memcmp(id3v2.id, "ID3", 3))
	 {
		fseek (ttainfo->HANDLE, 0, SEEK_SET);
		return 0;
	}

	id3v2_size = unpack_sint28(id3v2.size) + 10;

	if (!(buffer = (unsigned char *)malloc (id3v2_size)))
	    return -1;

	if ((id3v2.flags & ID3_UNSYNCHRONISATION_FLAG) ||
		(id3v2.flags & ID3_EXPERIMENTALTAG_FLAG) ||
		(id3v2.version < 3)) goto done;

	fseek (ttainfo->HANDLE, 0, SEEK_SET);

	if (!fread(buffer, 1, id3v2_size, ttainfo->HANDLE))
	 {
		fseek (ttainfo->HANDLE, 0, SEEK_SET);
		free (buffer);
		return -1;
	}

	ptr = buffer + 10;

	// skip extended header if present
	if (id3v2.flags & ID3_EXTENDEDHEADER_FLAG) {
		int offset = (int) unpack_sint32(ptr);
		ptr += offset;
	}

	// read id3v2 frames
	while (ptr - buffer < id3v2_size) {
		int data_size, frame_id;
		int size, comments = 0;
		unsigned char *data;

		// get frame header
		memcpy(&frame_header, ptr, sizeof(id3v2_frame));
		ptr += sizeof(id3v2_frame);
		data_size = unpack_sint32(frame_header.size);

		if (!*frame_header.id) break;

		if (!(frame_id = get_frame_id(frame_header.id))) {
			ptr += data_size;
			continue;
		}

		// skip unsupported frames
		if (frame_header.flags & FRAME_COMPRESSION_FLAG ||
			frame_header.flags & FRAME_ENCRYPTION_FLAG ||
			frame_header.flags & FRAME_UNSYNCHRONISATION_FLAG ||
			*ptr != FIELD_TEXT_ISO_8859_1) {
			ptr += data_size;
			continue;
		}

		ptr++; data_size--;

		switch (frame_id) {
		case TIT2:	data = ttainfo->id3v2.title;
					size = sizeof(ttainfo->id3v2.title) - 1; break;
		case TPE1:	data = ttainfo->id3v2.artist;
					size = sizeof(ttainfo->id3v2.artist) - 1; break;
		case TALB:	data = ttainfo->id3v2.album;
					size = sizeof(ttainfo->id3v2.album) - 1; break;
		case TRCK:	data = ttainfo->id3v2.track;
					size = sizeof(ttainfo->id3v2.track) - 1; break;
		case TYER:	data = ttainfo->id3v2.year;
					size = sizeof(ttainfo->id3v2.year) - 1; break;
		case TCON:	data = ttainfo->id3v2.genre;
					size = sizeof(ttainfo->id3v2.genre) - 1; break;
		case COMM:	if (comments++) goto next;
					data = ttainfo->id3v2.comment;
					size = sizeof(ttainfo->id3v2.comment) - 1;
					data_size -= 3; ptr += 3;
					// skip zero short description
					if (*ptr == 0) { data_size--; ptr++; }
					break;
		}
next:
		memcpy(data, ptr, (data_size <= size)? data_size:size);
		ptr += data_size;
	}

done:
	free (buffer);

	if (id3v2.flags & ID3_FOOTERPRESENT_FLAG) id3v2_size += 10;

	fseek (ttainfo->HANDLE, id3v2_size, SEEK_SET);
	ttainfo->id3v2.size = id3v2_size;

	ttainfo->id3v2.id3has = 1;

	return id3v2_size;
}

/* eof */

