/*
 * AlsaPlayer MAD plugin.
 *
 * Copyright (C) 2001 Andy Lo A Foe <andy@alsaplayer.org>
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

extern "C" { 	// Make sure MAD symbols are not mangled
							// since we compile them with regular gcc

#include "frame.h"
#include "synth.h"
#include "mad.h"

}

#define BLOCK_SIZE 4096	/* We can use any block size we like */
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
				struct mad_synth  synth; 
				struct mad_stream stream;
				struct mad_frame  frame;
				int mad_init;
				ssize_t offset;
				int samplerate;
				int bitrate;
				int seekable;
				int eof;
};		


/*
 * NAME:	error_str()
 * DESCRIPTION:	return a string describing a MAD error
 */
				static
char const *error_str(enum mad_error error)
{
				static char str[17];

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
								case MAD_ERROR_BADCRC:	 return ("CRC check failed");
								case MAD_ERROR_BADBITALLOC:	 return ("forbidden bit allocation value");
								case MAD_ERROR_BADSCALEFACTOR: return ("bad scalefactor index");
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

				static inline
signed int scale(mad_fixed_t sample)
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
												return data->current_frame;
								}
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

				if (!obj)
								return 0;
				data = (struct mad_local_data *)obj->local_data;
				if (!data)
								return 0;

				if (mad_frame_decode(&data->frame, &data->stream) == -1) {
								if (!MAD_RECOVERABLE(data->stream.error)) {
												/* printf("MAD error: %s\n", error_str(data->stream.error)); */
												mad_frame_mute(&data->frame);
												return 0;
								}	else {
												/* printf("MAD error: %s\n", error_str(data->stream.error)); */
								}
				}
				data->current_frame++;
				if (data->current_frame < (obj->nr_frames + FRAME_RESERVE)
												&& data->seekable) {
								data->frames[data->current_frame] = 
												data->stream.this_frame - data->mad_map;
								if (data->highest_frame < data->current_frame)
												data->highest_frame = data->current_frame;
				}				
				
				mad_synth_frame (&data->synth, &data->frame);
				{
								struct mad_pcm *pcm = &data->synth.pcm;
								mad_fixed_t const *left_ch, *right_ch;
								uint16_t	*output = (uint16_t *)buf;
								int nsamples = pcm->length;
								int nchannels = pcm->channels;
								left_ch = pcm->samples[0];
								right_ch = pcm->samples[1];
								while (nsamples--) {
												*output++ = scale(*left_ch++);
												if (nchannels == 2) 
																*output++ = scale(*right_ch++);
								}
				}

				return 1;
}


static unsigned long mad_frame_to_sec(input_object *obj, int frame)
{
				struct mad_local_data *data;
				int64_t l = 0;
				unsigned long sec = 0;

				if (!obj)
								return 0;
				data = (struct mad_local_data *)obj->local_data;
				if (data)
								sec = data->samplerate ? frame * (obj->frame_size >> 2) /
												(data->samplerate / 100) : 0;
				return sec;
}


static int mad_nr_frames(input_object *obj)
{
				struct mad_local_data *data;
				if (!obj)
								return 0;
				return obj->nr_frames;
}


int mad_stream_info(input_object *obj, stream_info *info)
{
				struct mad_local_data *data;	

				if (!obj || !info)
								return 0;
				data = (struct mad_local_data *)obj->local_data;

				if (data) {
								strcpy(info->title, "Please note: file info is not yet functional");				
								sprintf(info->stream_type, "%d-bit %dKHz %s %d Kbit/s Audio MPEG",
																16, data->frame.header.samplerate / 1000,
																obj->nr_channels == 2 ? "stereo" : "mono",
																data->frame.header.bitrate / 1000);
																
				}				
				return 1;
}


static int mad_samplerate(input_object *obj)
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
												!strcasecmp(ext, "mp2") ||
												strstr(path, "http://")) {
								return 0.9;
				} else {
								return 0.0;
				}
}


static ssize_t find_initial_frame(uint8_t *buf, int size)
{
				uint8_t *data = buf;			

				int pos = 0;
				ssize_t header_size = 0;
				while (pos < (size - 10)) {
								if ((data[pos] == 0x49 && data[pos+1] == 0x44 && data[pos+2] == 0x33)) {
												header_size = (data[pos + 6] << 21) + 
																(data[pos + 7] << 14) +
																(data[pos + 8] << 7) +
																data[pos + 9]; /* syncsafe integer */
												if (data[pos + 5] & 0x10) {
																header_size += 10; /* 10 byte extended header */
												}
												header_size += 10;
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
								} else if (data[pos] == 'T' && data[pos+1] == 'A' && 
																data[pos+2] == 'G') {
												return 128;	/* TAG is fixed 128 bytes */
								}  else {
												pos++;
								}				
				}
				if (data[0] != 0xff) {
								printf("MAD debug: potential problem file, first 4 bytes =  %x %x %x %x\n",
																data[0], data[1], data[2], data[3]);
				}
				return 0;
}


static int mad_open(input_object *obj, char *path)
{
				struct mad_local_data *data;

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
								fprintf(stderr, "mad_open() failed\n");
								return 0;
				}
				if (fstat(data->mad_fd, &data->stat) == -1) {
								perror("fstat");
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
				mad_synth_init  (&data->synth);
				mad_stream_init (&data->stream);
				mad_frame_init  (&data->frame);
				data->mad_init = 1;
				data->offset = find_initial_frame(data->mad_map, STREAM_BUFFER_SIZE);
				data->highest_frame = 0;
				if (data->offset < 0) {
								fprintf(stderr, "mad_open() couldn't find valid MPEG header\n");
								return 0;
				}
				mad_stream_buffer(&data->stream, data->mad_map + data->offset,
												data->stat.st_size - data->offset);
				
				if ((mad_frame_decode(&data->frame, &data->stream) != 0)) {
								switch (data->stream.error) {
												case	MAD_ERROR_BUFLEN:
																printf("MAD_ERROR_BUFLEN...\n");
																break;
												default:
																printf("MAD debug: no valid frame found at start\n");
								}
								return 0;
				} else {
								int mode = (data->frame.header.mode == MAD_MODE_SINGLE_CHANNEL) ?
												1 : 2;
								data->samplerate = data->frame.header.samplerate;
								data->bitrate	= data->frame.header.bitrate;

								mad_synth_frame (&data->synth, &data->frame);

								{
												struct mad_pcm *pcm = &data->synth.pcm;			

												obj->nr_channels = pcm->channels;
								}
				}
				/* Calculate some parameters */

				{
								ssize_t filesize;			
								int64_t time;
								int64_t samples;
								int64_t frames;

								filesize = data->stat.st_size;
								filesize -= data->offset;

								time = (filesize * 8) / (data->bitrate);

								samples = 32 * MAD_NSBSAMPLES(&data->frame.header);

								obj->frame_size = (int) samples << 2; /* Assume 16-bit stereo */

								frames = data->samplerate * time / samples;

								obj->nr_frames = (int) frames;
								obj->nr_tracks = 1;
				}
				/* Allocate frame index */
				if (obj->nr_frames > 500000 ||
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
				return 1;
}


static void mad_close(input_object *obj)
{
				struct mad_local_data *data;

				if (!obj)
								return;
				data = (struct mad_local_data *)obj->local_data;

				if (data) {
								if (data->mad_map)
												munmap(data->mad_map, data->stat.st_size);
								if (data->mad_fd)
												close(data->mad_fd);
								if (data->mad_init) {
												mad_synth_finish (&data->synth);
												mad_frame_finish (&data->frame);
												mad_stream_finish(&data->stream);
								}
								if (data->frames) {
												free(data->frames);
								}				
								free(obj->local_data);
								obj->local_data = NULL;
				}				
}


input_plugin mad_plugin =
{
				INPUT_PLUGIN_VERSION,
				0,
				{ "MAD MPEG audio plugin v0.91" },
				{ "Andy Lo A Foe" },
				NULL,
				mad_init,
				NULL,
				NULL,
				mad_can_handle,
				mad_open,
				mad_close,
				mad_play_frame,
				mad_frame_seek,
				mad_frame_size,
				mad_nr_frames,
				mad_frame_to_sec,
				mad_samplerate,
				mad_channels,
				mad_stream_info,
				NULL,
				NULL
};

extern "C" {

input_plugin *input_plugin_info (void)
{
				return &mad_plugin;
}

}
