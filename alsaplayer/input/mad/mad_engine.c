/*
 * AlsaPlayer MAD plugin.
 *
 * Copyright (C) 2001-2002 Andy Lo A Foe <andy@alsaplayer.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 *  $Id$
 */ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "input_plugin.h"
#include "alsaplayer_error.h"

#ifdef __cplusplus
extern "C" { 	/* Make sure MAD symbols are not mangled
		 * since we compile them with regular gcc */
#endif

#ifndef HAVE_LIBMAD /* Needed if we use our own (outdated) copy */
#include "frame.h"
#include "synth.h"
#endif	
#include "mad.h"

#ifdef __cplusplus
}
#endif

#ifdef HAVE_ID3TAG_H
#ifndef HAVE_LIBID3TAG
#error "libid3tag.h was present but not libid3tag.so :-("
#endif
#include "id3tag.h"
#include <assert.h>
#endif

#define BLOCK_SIZE 4096
#define MAX_NUM_SAMPLES 8192
#define STREAM_BUFFER_SIZE	16384
#define FRAME_RESERVE	2000

# if !defined(O_BINARY)
#  define O_BINARY  0
# endif

struct mad_local_data {
	int mad_fd;
	uint8_t *mad_map;
	struct stat stat;
	ssize_t	*frames;
	int highest_frame;
	int current_frame;
	char path[FILENAME_MAX+1];
	char filename[FILENAME_MAX+1];
	struct mad_synth  synth; 
	struct mad_stream stream;
	struct mad_frame  frame;
	stream_info sinfo;
	int mad_init;
	ssize_t offset;
	ssize_t filesize;
	int samplerate;
	int bitrate;
	int seekable;
	int seeking;
	int eof;
	int parsed_id3;
	char str[17];
};		


/*
 * NAME:	error_str()
 * DESCRIPTION:	return a string describing a MAD error
 */
	static
char const *error_str(enum mad_error error, char *str)
{

	switch (error) {
		case MAD_ERROR_BUFLEN:
		case MAD_ERROR_BUFPTR:
			/* these errors are handled specially and/or should not occur */
			break;

		case MAD_ERROR_NOMEM:		 return ("not enough memory");
		case MAD_ERROR_LOSTSYNC:	 return ("lost synchronization");
		case MAD_ERROR_BADLAYER:	 return ("reserved header layer value");
		case MAD_ERROR_BADBITRATE:	 return ("forbidden bitrate value");
		case MAD_ERROR_BADSAMPLERATE:	 return ("reserved sample frequency value");
		case MAD_ERROR_BADEMPHASIS:	 return ("reserved emphasis value");
		case MAD_ERROR_BADCRC:		 return ("CRC check failed");
		case MAD_ERROR_BADBITALLOC:	 return ("forbidden bit allocation value");
		case MAD_ERROR_BADSCALEFACTOR:	 return ("bad scalefactor index");
		case MAD_ERROR_BADFRAMELEN:	 return ("bad frame length");
		case MAD_ERROR_BADBIGVALUES:	 return ("bad big_values count");
		case MAD_ERROR_BADBLOCKTYPE:	 return ("reserved block_type");
		case MAD_ERROR_BADSCFSI:	 return ("bad scalefactor selection info");
		case MAD_ERROR_BADDATAPTR:	 return ("bad main_data_begin pointer");
		case MAD_ERROR_BADPART3LEN:	 return ("bad audio data length");
		case MAD_ERROR_BADHUFFTABLE:	 return ("bad Huffman table select");
		case MAD_ERROR_BADHUFFDATA:	 return ("Huffman data overrun");
		case MAD_ERROR_BADSTEREO:	 return ("incompatible block_type for JS");
	}

	sprintf(str, "error 0x%04x", error);
	return str;
}


/* utility to scale and round samples to 16 bits */

static inline signed int my_scale(mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - 16));

	/* clip */
	if (sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if (sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}



static int mad_frame_seek(input_object *obj, int frame)
{
	struct mad_local_data *data;
	struct mad_header header;
	int64_t pos;
	int bytes_read;
	int advance;
	int num_bytes;
	int skip;
	ssize_t byte_offset;

	if (!obj)
		return 0;
	data = (struct mad_local_data *)obj->local_data;


	if (data) {
		ssize_t current_pos;

		if (!data->seekable)
			return 0;

		mad_header_init(&header);

		if (frame <= data->highest_frame) {
			skip = 0;
			if (frame > 4) {
				skip = 3;
			}
			byte_offset = data->frames[frame-skip];
			mad_stream_buffer(&data->stream, data->mad_map + 
					byte_offset,
					data->stat.st_size - byte_offset);
			skip++;
			while (skip != 0) {
				skip--;

				mad_frame_decode(&data->frame, &data->stream);
				if (skip == 0)
					mad_synth_frame (&data->synth, &data->frame);
			}				
			data->current_frame = frame;
			data->seeking = 0;
			return data->current_frame;
		}

		data->seeking = 1;

		mad_stream_buffer(&data->stream, data->mad_map +
				data->frames[data->highest_frame],
				data->stat.st_size - data->offset -
				data->frames[data->highest_frame]);
		while (data->highest_frame < frame) {
			if (mad_header_decode(&header, &data->stream) == -1) {
				if (!MAD_RECOVERABLE(data->stream.error)) {
					printf("MAD debug: error seeking to %d, going to %d\n",
							data->current_frame, data->highest_frame);
					mad_stream_buffer(&data->stream,
							data->mad_map + data->offset +
							data->frames[data->highest_frame],
							data->stat.st_size - data->offset -
							data->frames[data->highest_frame]);
					data->seeking = 0;	
					return 0;
				}				
			}
			data->frames[++data->highest_frame] =
				data->stream.this_frame - data->mad_map;
		}
		data->current_frame = data->highest_frame;
		if (data->current_frame > 4) {
			skip = 3;
			byte_offset = data->frames[data->current_frame-skip];
			mad_stream_buffer(&data->stream, data->mad_map +
					byte_offset, data->stat.st_size - byte_offset);
			skip++;
			while (skip != 0) { 
				skip--;
				mad_frame_decode(&data->frame, &data->stream);
				if (skip == 0) 
					mad_synth_frame (&data->synth, &data->frame);
			}
		}
		data->seeking = 0;
		return data->current_frame;
	}
	return 0;
}

static int mad_frame_size(input_object *obj)
{
	if (!obj || !obj->frame_size) {
		printf("No frame size!!!!\n");
		return 0;
	}
	return obj->frame_size;
}


/* #define MAD_DEBUG */

static int mad_play_frame(input_object *obj, char *buf)
{
	int ret;
	int bytes_read;
	struct mad_local_data *data;
	struct mad_pcm *pcm;
	mad_fixed_t const *left_ch;
	mad_fixed_t const *right_ch;
	int16_t	*output;
	int nsamples;
	int nchannels;

	if (!obj)
		return 0;
	data = (struct mad_local_data *)obj->local_data;
	if (!data)
		return 0;
	if (mad_frame_decode(&data->frame, &data->stream) == -1) {
		if (!MAD_RECOVERABLE(data->stream.error)) {
			/* alsaplayer_error("MAD error: %s", error_str(data->stream.error, data->str)); */
			mad_frame_mute(&data->frame);
			return 0;
		}	else {
			/* alsaplayer_error("MAD error: %s", error_str(data->stream.error, data->str)); */
		}
	}
	data->current_frame++;
	if (data->current_frame < (obj->nr_frames + FRAME_RESERVE)
			&& data->seekable) {
		data->frames[data->current_frame] = 
			data->stream.this_frame - data->mad_map;
		if (data->current_frame > 3 && 
				(data->frames[data->current_frame] -
				 data->frames[data->current_frame-3]) < 6) {
			/* alsaplayer_error("EOF reached"); */
			return 0;
		}		
		if (data->highest_frame < data->current_frame)
			data->highest_frame = data->current_frame;
	}				

	mad_synth_frame (&data->synth, &data->frame);
	{
		pcm = &data->synth.pcm;
		output = (int16_t *)buf;
		nsamples = pcm->length;
		nchannels = pcm->channels;
		left_ch = pcm->samples[0];
		right_ch = pcm->samples[1];
		while (nsamples--) {
			*output++ = my_scale(*(left_ch++));
			if (nchannels == 1) 
				*output++ = my_scale(*(left_ch-1));
			else /* nchannels == 2 */
				*output++ = my_scale(*(right_ch++));

		}
	}

	return 1;
}


static  long mad_frame_to_sec(input_object *obj, int frame)
{
	struct mad_local_data *data;
	int64_t l = 0;
	unsigned long sec = 0;

	if (!obj)
		return 0;
	data = (struct mad_local_data *)obj->local_data;
	if (data) {
		if (data->seeking)
			return -1;
		sec = data->samplerate ? frame * (obj->frame_size >> 2) /
			(data->samplerate / 100) : 0;
	}			
	return sec;
}


static int mad_nr_frames(input_object *obj)
{
	struct mad_local_data *data;
	if (!obj)
		return 0;
	return obj->nr_frames;
}

#ifdef HAVE_LIBID3TAG

#define _(text) text
#define N_(text) text
#define gettext(text) text
/*
 * NAME:	show_id3()
 * DESCRIPTION:	display an ID3 tag
 * This code was lifted from the player.c file in the libmad distribution
 */
	static
void parse_id3(struct id3_tag const *tag, stream_info *sinfo)
{
	unsigned int i;
	struct id3_frame const *frame;
	id3_ucs4_t const *ucs4;
	id3_latin1_t *latin1;
	char const spaces[] = "          ";

	struct {
		char const *id;
		char const *name;
	} const info[] = {
		{ ID3_FRAME_TITLE,  N_("Title")     },
		{ "TIT3",           0               },  /* Subtitle */
		{ "TCOP",           0,              },  /* Copyright */
		{ "TPRO",           0,              },  /* Produced */
		{ "TCOM",           N_("Composer")  },
		{ ID3_FRAME_ARTIST, N_("Artist")    },
		{ "TPE2",           N_("Orchestra") },
		{ "TPE3",           N_("Conductor") },
		{ "TEXT",           N_("Lyricist")  },
		{ ID3_FRAME_ALBUM,  N_("Album")     },
		{ ID3_FRAME_YEAR,   N_("Year")      },
		{ ID3_FRAME_TRACK,  N_("Track")     },
		{ "TPUB",           N_("Publisher") },
		{ ID3_FRAME_GENRE,  N_("Genre")     },
		{ "TRSN",           N_("Station")   },
		{ "TENC",           N_("Encoder")   }
	};

	/* text information */

	for (i = 0; i < sizeof(info) / sizeof(info[0]); ++i) {
		union id3_field const *field;
		unsigned int nstrings, namelen, j;
		char const *name;

		frame = id3_tag_findframe(tag, info[i].id, 0);
		if (frame == 0)
			continue;

		field    = &frame->fields[1];
		nstrings = id3_field_getnstrings(field);

		name = info[i].name;
		if (name)
			name = gettext(name);

		namelen = name ? strlen(name) : 0;
		assert(namelen < sizeof(spaces));

		for (j = 0; j < nstrings; ++j) {
			ucs4 = id3_field_getstrings(field, j);
			assert(ucs4);

			if (strcmp(info[i].id, ID3_FRAME_GENRE) == 0)
				ucs4 = id3_genre_name(ucs4);

			latin1 = id3_ucs4_latin1duplicate(ucs4);
			if (latin1 == 0)
				goto fail;

			if (j == 0 && name) {
				if (strcmp(name, "Title") == 0) {
					strcpy(sinfo->title, latin1);
				} 
				if (strcmp(name, "Artist") == 0)
					strcpy(sinfo->author, latin1);
				//alsaplayer_error("%s%s: %s", &spaces[namelen], name, latin1);
			} else {
				if (strcmp(info[i].id, "TCOP") == 0 ||
						strcmp(info[i].id, "TPRO") == 0) {
					//alsaplayer_error("%s  %s %s\n", spaces, (info[i].id[1] == 'C') ?
					//		_("Copyright (C)") : _("Produced (P)"), latin1);
				}
				else
					;//alsaplayer_error("%s  %s\n", spaces, latin1);
			}

			free(latin1);
		}
	}

	/* comments */

	i = 0;
	while ((frame = id3_tag_findframe(tag, ID3_FRAME_COMMENT, i++))) {
		id3_latin1_t *ptr, *newline;
		int first = 1;

		ucs4 = id3_field_getstring(&frame->fields[2]);
		assert(ucs4);

		if (*ucs4)
			continue;

		ucs4 = id3_field_getfullstring(&frame->fields[3]);
		assert(ucs4);

		latin1 = id3_ucs4_latin1duplicate(ucs4);
		if (latin1 == 0)
			goto fail;

		ptr = latin1;
		while (*ptr) {
			newline = strchr(ptr, '\n');
			if (newline)
				*newline = 0;

			if (strlen(ptr) > 66) {
				id3_latin1_t *linebreak;

				linebreak = ptr + 66;

				while (linebreak > ptr && *linebreak != ' ')
					--linebreak;

				if (*linebreak == ' ') {
					if (newline)
						*newline = '\n';

					newline = linebreak;
					*newline = 0;
				}
			}

			if (first) {
				char const *name;
				unsigned int namelen;

				name    = _("Comment");
				namelen = strlen(name);
				assert(namelen < sizeof(spaces));

				//alsaplayer_error("%s%s: %s\n", &spaces[namelen], name, ptr);
				first = 0;
			}
			else 
				;//alsaplayer_error("%s  %s\n", spaces, ptr);

			ptr += strlen(ptr) + (newline ? 1 : 0);
		}

		free(latin1);
		break;
	}

	if (0) {
fail:
		alsaplayer_error(_("not enough memory to display tag"));
	}
}
#endif

static int mad_stream_info(input_object *obj, stream_info *info)
{
	struct mad_local_data *data;	
#ifdef HAVE_LIBID3TAG
	struct id3_file *id3;
	struct id3_tag *tag;
	struct id3_frame *frame;
	union id3_field const *field;
	id3_ucs4_t const *ucs4;
	id3_latin1_t *latin1;
#endif				

	if (!obj || !info)
		return 0;
	data = (struct mad_local_data *)obj->local_data;

	if (data) {
#ifdef HAVE_LIBID3TAG								
		if (!data->parsed_id3) { /* Parse only one time */
			id3 = id3_file_open(data->path, ID3_FILE_MODE_READONLY);
			if (id3) {
				tag = id3_file_tag(id3);
				if (tag) {
					parse_id3(tag, &data->sinfo);
				}
				id3_file_close(id3);
			}
			data->parsed_id3 = 1;
		}
		if (strlen(data->sinfo.title))
			sprintf(info->title, "%s", data->sinfo.title);
		else
			sprintf(info->title, "%s", data->filename);
		if (strlen(data->sinfo.author))
			sprintf(info->author, "%s", data->sinfo.author);
#else										
		sprintf(info->title, "Unparsed: %s", data->filename);				
#endif
		sprintf(info->stream_type, "%dKHz %dkbit %s audio mpeg",
				data->frame.header.samplerate / 1000,
				data->frame.header.bitrate / 1000,
				obj->nr_channels == 2 ? "stereo" : "mono");
		if (data->seeking)
			sprintf(info->status, "Seeking...");
		else
			sprintf(info->status, "");
	}				
	return 1;
}


static int mad_sample_rate(input_object *obj)
{
	struct mad_local_data *data;

	if (!obj)
		return 44100;
	data = (struct mad_local_data *)obj->local_data;
	if (data)	
		return data->samplerate;
	return 44100;
}

static int mad_init() 
{
	return 1;
}		


static int mad_channels(input_object *obj)
{
	struct mad_local_data *data;

	if (!obj)
		return 2; /* Default to 2, this is flaky at best! */
	data = (struct mad_local_data *)obj->local_data;

	return obj->nr_channels;
}


static float mad_can_handle(const char *path)
{
	char *ext;      
	ext = strrchr(path, '.');

	if (!ext)
		return 0.0;
	ext++;
	if (!strcasecmp(ext, "mp3") ||
			!strcasecmp(ext, "mp2")) {
		return 0.9;
	} else {
		return 0.0;
	}
}


static ssize_t find_initial_frame(uint8_t *buf, int size)
{
	uint8_t *data = buf;			
	int ext_header = 0;
	int pos = 0;
	ssize_t header_size = 0;
	while (pos < (size - 10)) {
		if (pos == 0 && (data[pos] == 'I' && data[pos+1] == 'D' && data[pos+2] == '3')) {
			header_size = (data[pos + 6] << 21) + 
				(data[pos + 7] << 14) +
				(data[pos + 8] << 7) +
				data[pos + 9]; /* syncsafe integer */
			if (data[pos + 5] & 0x10) {
				ext_header = 1;
				header_size += 10; /* 10 byte extended header */
			}
			//printf("ID3v2.%c detected with header size %d (at pos %d)\n",  0x30 + data[pos + 3], header_size, pos);
			if (ext_header) {
				//printf("Extended header detected\n");
			}
			header_size += 10;
			//printf("MP3 should start at %d\n", header_size);
			if (data[header_size] != 0xff) {
				alsaplayer_error("broken MP3 or unkown TAG! Searching for next 0xFF");
				while (header_size < size) {
					if (data[++header_size] == 0xff &&
							data[header_size+1] == 0xfb) {
						//alsaplayer_error("Found 0xff 0xfb at %d",  header_size);
						return header_size;
					}
				}
				alsaplayer_error("Not found in first 16K, bad :(");
			}	
			return header_size;
		} else if (data[pos] == 'R' && data[pos+1] == 'I' &&
				data[pos+2] == 'F' && data[pos+3] == 'F') {
			pos+=4;
			while (pos < size) {
				if (data[pos] == 'd' && data[pos+1] == 'a' &&
						data[pos+2] == 't' && data[pos+3] == 'a') {
					pos += 8; /* skip 'data' and ignore size */
					return pos;
				} else
					pos++;
			}
			printf("MAD debug: invalid header\n");
			return -1;
		} else if (pos == 0 && data[pos] == 'T' && data[pos+1] == 'A' && 
				data[pos+2] == 'G') {
			return 128;	/* TAG is fixed 128 bytes */
		}  else {
			pos++;
		}				
	}
	if (data[0] != 0xff) {
		header_size = 0;
		while (header_size < size) {
			if (data[++header_size] == 0xff && data[header_size+1] == 0xfb) {
				//printf("Found ff fb at %d\n", header_size);
				return header_size;
			}
		}	
		printf("MAD debug: potential problem file or unhandled info block, next 4 bytes =  %x %x %x %x\n",
				data[header_size], data[header_size+1],
				data[header_size+2], data[header_size+3]);
	}
	return 0;
}


static int mad_open(input_object *obj, char *path)
{
	struct mad_local_data *data;
	char *p;
	int mode;

	if (!obj)
		return 0;

	obj->local_data = malloc(sizeof(struct mad_local_data));
	if (!obj->local_data) {
		printf("failed to allocate local data\n");		
		return 0;
	}
	data = (struct mad_local_data *)obj->local_data;
	memset(data, 0, sizeof(struct mad_local_data));

	if ((data->mad_fd = open(path, O_RDONLY | O_BINARY)) < 0) {
		//fprintf(stderr, "mad_open() failed\n");
		return 0;
	}
	if (fstat(data->mad_fd, &data->stat) == -1) {
		perror("fstat");
		return 0;
	}
	if (!data->stat.st_size) {
		fprintf(stderr, "empty file\n");
		return 0;
	}	
	if (!S_ISREG(data->stat.st_mode)) {
		fprintf(stderr, "%s: Not a regular file\n", path);
		return 0;
	}
	data->mad_map = (uint8_t *)mmap(0, data->stat.st_size, PROT_READ, MAP_SHARED, data->mad_fd, 0);

	if (data->mad_map == MAP_FAILED) {
		perror("mmap");
		return 0;
	}
	/* Use madvise to tell kernel we will do mostly sequential reading */
#ifdef HAVE_MADVISE				
	if (madvise(data->mad_map, data->stat.st_size, MADV_SEQUENTIAL) < 0)
		printf("MAD warning: madvise() call failed\n");
#endif				
	mad_synth_init  (&data->synth);
	mad_stream_init (&data->stream);
	mad_frame_init  (&data->frame);
	data->mad_init = 1;
	data->offset = find_initial_frame(data->mad_map, 
			data->stat.st_size < STREAM_BUFFER_SIZE ? data->stat.st_size :
			STREAM_BUFFER_SIZE);
	data->highest_frame = 0;
	if (data->offset < 0) {
		fprintf(stderr, "mad_open() couldn't find valid MPEG header\n");
		return 0;
	}
	mad_stream_buffer(&data->stream, data->mad_map + data->offset,
			data->stat.st_size - data->offset);

	if ((mad_frame_decode(&data->frame, &data->stream) != 0)) {
		alsaplayer_error("MAD error: %s", error_str(data->stream.error, data->str));
		switch (data->stream.error) {
			case	MAD_ERROR_BUFLEN:
				return 0;
			case MAD_ERROR_LOSTSYNC:
				return 0;
			case MAD_ERROR_BADBITALLOC:
				return 0;
			case 0x232:				
			case 0x235:
				break;
			default:
				printf("No valid frame found at start (pos: %d, error: 0x%x)\n", data->offset, data->stream.error);
				return 0;
		}
	}
	mode = (data->frame.header.mode == MAD_MODE_SINGLE_CHANNEL) ?
		1 : 2;
	data->samplerate = data->frame.header.samplerate;
	data->bitrate	= data->frame.header.bitrate;

	mad_synth_frame (&data->synth, &data->frame);
	{
		struct mad_pcm *pcm = &data->synth.pcm;			

		obj->nr_channels = pcm->channels;
	}
	/* Calculate some values */

	{
		int64_t time;
		int64_t samples;
		int64_t frames;

		data->filesize = data->stat.st_size;
		data->filesize -= data->offset;

		time = (data->filesize * 8) / (data->bitrate);

		samples = 32 * MAD_NSBSAMPLES(&data->frame.header);

		obj->frame_size = (int) samples << 2; /* Assume 16-bit stereo */
		frames = data->samplerate * (time+1) / samples;

		obj->nr_frames = (int) frames;
		obj->nr_tracks = 1;
	}
	/* Allocate frame index */
	if (obj->nr_frames > 1000000 ||
			(data->frames = (ssize_t *)malloc((obj->nr_frames + FRAME_RESERVE) * sizeof(ssize_t))) == NULL) {
		data->seekable = 0;
	}	else {
		data->seekable = 1;
	}				
	data->frames[0] = data->offset;

	/* Reset decoder */
	data->mad_init = 0;
	mad_synth_finish (&data->synth);
	mad_frame_finish (&data->frame);
	mad_stream_finish(&data->stream);

	mad_synth_init  (&data->synth);
	mad_stream_init (&data->stream);
	mad_frame_init  (&data->frame);
	data->mad_init = 1;

	mad_stream_buffer (&data->stream, data->mad_map + data->offset, data->stat.st_size - data->offset);

	p = strrchr(path, '/');
	if (p) {
		strcpy(data->filename, ++p);
	} else {
		strcpy(data->filename, path);
	}
	strcpy(data->path, path);

	obj->flags = P_SEEK;

	return 1;
}


static void mad_close(input_object *obj)
{
	struct mad_local_data *data;
	if (!obj)
		return;
	data = (struct mad_local_data *)obj->local_data;

	if (data) {
		if (data->mad_fd)
			close(data->mad_fd);
		if (data->mad_map) {
			if (munmap(data->mad_map, data->stat.st_size) == -1)
				alsaplayer_error("failed to unmap memory...");
		}				
		if (data->mad_init) {
			mad_synth_finish (&data->synth);
			mad_frame_finish (&data->frame);
			mad_stream_finish(&data->stream);
			data->mad_init = 0;
		}
		if (data->frames) {
			free(data->frames);
		}				
		free(obj->local_data);
		obj->local_data = NULL;
	}				
}


static void mad_shutdown()
{
	return;
}


static input_plugin mad_plugin;

#ifdef __cplusplus
extern "C" {
#endif

input_plugin *input_plugin_info (void)
{
	memset(&mad_plugin, 0, sizeof(input_plugin));
	mad_plugin.version = INPUT_PLUGIN_VERSION;
	mad_plugin.name = "MAD MPEG audio plugin v0.91";
	mad_plugin.author = "Andy Lo A Foe";
	mad_plugin.init = mad_init;
	mad_plugin.shutdown = mad_shutdown;
	mad_plugin.can_handle = mad_can_handle;
	mad_plugin.open = mad_open;
	mad_plugin.close = mad_close;
	mad_plugin.play_frame = mad_play_frame;
	mad_plugin.frame_seek = mad_frame_seek;
	mad_plugin.frame_size = mad_frame_size;
	mad_plugin.nr_frames = mad_nr_frames;
	mad_plugin.frame_to_sec = mad_frame_to_sec;
	mad_plugin.sample_rate = mad_sample_rate;
	mad_plugin.channels = mad_channels;
	mad_plugin.stream_info = mad_stream_info;
	return &mad_plugin;
}

#ifdef __cplusplus
}
#endif
