/*
 *  wav_engine.c	(C) 1999 by Andy Lo A foe
 *  Based on wav Copyright (c) by Jaroslav Kysela <perex@jcu.cz>
 *  Based on vplay program by Michael Beck
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include "formats.h"
#include "input_plugin.h"

#define APLAY_FRAME_SIZE	4608

#define FORMAT_DEFAULT		-1
#define FORMAT_RAW		0
#define FORMAT_VOC		1
#define FORMAT_WAVE		2
#define FORMAT_AU		3

typedef struct _the_format
{
	int rate;
	int bits;
} the_format;

					
struct wav_local_data
{
	char wav_name[FILENAME_MAX+1];
	int count;
	FILE *wav_fd;
	int header_size;
	the_format format;
};


static void init_wav()
{
	return;
}


/*
 * test, if it's a .WAV file, 0 if ok (and set the speed, stereo etc.)
 *                            < 0 if not
 */
static int test_wavefile(input_object *obj, void *buffer)
{
	WaveHeader *wp = (WaveHeader *)buffer;

	struct wav_local_data *data = (struct wav_local_data *)obj->local_data;	
	if (wp->main_chunk == WAV_RIFF && wp->chunk_type == WAV_WAVE &&
	    wp->sub_chunk == WAV_FMT && 
		(wp->data_chunk == WAV_DATA || wp->data_chunk == WAV_DATA_1)) {
		if (wp->format != WAV_PCM_CODE) {
			fprintf(stderr, "APLAY: cannot play non PCM-coded WAVE-files\n");
			return -1;
		}
		if (wp->modus < 1 || wp->modus > 32) {
			fprintf(stderr, "APLAY: cannot play WAVE-files with %d tracks\n",
				wp->modus);
			return -1;
		}
		switch (wp->bit_p_spl) {
		case 8:
		case 16:
			/* Humm, no nothing :) */
			break;
		default:
			fprintf(stderr, "APLAY: can't play WAVE-files with sample %d bits wide\n",
				wp->bit_p_spl);
		}
		obj->nr_channels = wp->modus;
		data->format.rate = wp->sample_fq;
		data->format.bits = wp->bit_p_spl;
		data->count = wp->data_length;
		return 0;
	} else {
		fprintf(stderr, "APLAY: Cannot identify WAV\n"
						"APLAY: main_chunk = %x -> %x\n"
						"APLAY: chunk_type = %x -> %x\n"
						"APLAY: sub_chunk = %x -> %x\n"
						"APLAY: data_chunk = %x -> %x\n",
							wp->main_chunk,
							WAV_RIFF,
							wp->chunk_type,
							WAV_WAVE,
							wp->sub_chunk,
							WAV_FMT,
							wp->data_chunk,
							WAV_DATA);
		return -1;
	}
	fprintf(stderr, "APLAY: waaah! bug bug\n");
	return -1;
}


static int wav_open(input_object *obj, char *name)
{
	char audiobuf[4096];
	struct wav_local_data *data;
	
	if (!obj)
			return 0;

	obj->local_data = malloc(sizeof(struct wav_local_data));
	if (!obj->local_data) {
			return 0;
	}
	data = (struct wav_local_data *)obj->local_data;
	init_wav();
	if (!name || !strcmp(name, "-")) {
		printf("APLAY: Uhm, we don't support stdin\n");
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	} else {
		if ((data->wav_fd = fopen(name, "r")) == NULL) {
			perror(name);
			free(obj->local_data);
			obj->local_data = NULL;
			return 0;
		}
	}

	/* read the file header */
	if (fread(audiobuf, sizeof(WaveHeader), 1, data->wav_fd) != 1) {
		fprintf(stderr, "APLAY: read error");
		fclose(data->wav_fd);
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	}
	if (test_wavefile(obj, audiobuf) >= 0) {
		/* Extract the filename */
		char *ptr = strrchr(name, '/');
		if (ptr)
			ptr++;
		else
			ptr = name;
		if (strlen(ptr) > FILENAME_MAX) {
			strncpy(data->wav_name, ptr, FILENAME_MAX-1);
			data->wav_name[FILENAME_MAX-1] = 0;
		} else 
			strcpy(data->wav_name, ptr);
		data->header_size = sizeof(WaveHeader);

		obj->flags = P_SEEK;

		return 1;
	}
	if (data->wav_fd != NULL)
		fclose(data->wav_fd);
	free(obj->local_data);
	obj->local_data = NULL;
	return 0;
}


void wav_close(input_object *obj)
{
	struct wav_local_data *data;

	if (!obj)
			return;
	data = (struct wav_local_data *)obj->local_data;
	if (!data) {
			return;
	}		
	if (data->wav_fd != NULL)
			fclose(data->wav_fd);
	data->wav_fd =NULL;
	if (obj->local_data)
			free(obj->local_data);
	obj->local_data = NULL;
}

static int wav_play_frame(input_object *obj, char *buf)
{
	struct wav_local_data *data;		
	char audiobuf[8192];
	char tmpbuf[8192];
	unsigned char *us;
	short *s, *d, c;
	int i;
	
	if (!obj)
			return 0;
	data = (struct wav_local_data *)obj->local_data;
	if (!data) {
			return 0;
	}

	/* Take care of mono streams */
	if (obj->nr_channels == 1) { /* mono, so double  */
			if (data->format.bits == 8) { /* 8 bit */
				if (fread(tmpbuf, APLAY_FRAME_SIZE >> 2, 1, data->wav_fd) != 1) {
					return 0;
				}
#if 1		
				us = (unsigned char *)tmpbuf;
				d = (short *)audiobuf;
				for (i=0; i < APLAY_FRAME_SIZE; i+=4) {
					*d++ = c = ((*us ^ 0x80) << 8) | *us++;
					*d++ = c;
				}
#endif
			} else { /* 16 bit */	
				if (fread(tmpbuf, APLAY_FRAME_SIZE >> 1, 1, data->wav_fd) != 1) {
						return 0;
				}
				s = (short *)tmpbuf;
				d = (short *)audiobuf;
				for (i=0; i < APLAY_FRAME_SIZE; i+=4) {
						*d++ = *s;
						*d++ = *s++; /* Copy twice */
				}
			}
	} else if (obj->nr_channels == 2) {
		if (fread(audiobuf, APLAY_FRAME_SIZE, 1, data->wav_fd) != 1) {
				return 0;
		}
	} else {
			printf("Huh? More than 2 channels?\n");
			exit(3);
	}
	if (buf)
		memcpy(buf, audiobuf, APLAY_FRAME_SIZE);
	return 1;
}


static int wav_frame_seek(input_object *obj, int frame)
{
	struct wav_local_data *data;	
	int result = 0;
	long position;

	if (!obj)
			return result;
	data = (struct wav_local_data *)obj->local_data;
	if (!data) {
			return result;
	}		
	position = (frame * (APLAY_FRAME_SIZE >> (2 - obj->nr_channels)));
	position += data->header_size;
	result = !fseek(data->wav_fd, position, SEEK_SET);
	return result;
}


static int wav_frame_size(input_object *obj)
{
	return APLAY_FRAME_SIZE;
}


static int wav_nr_frames(input_object *obj)
{
	struct wav_local_data *data;

	int result = 0;	
	if (!obj)
			return 0;
	data = (struct wav_local_data *)obj->local_data;
	if (!data) {
			return result;
	}		
	if (data->count)
			 result = (data->count / (APLAY_FRAME_SIZE >> ((2 - obj->nr_channels) + (data->format.bits == 8 ? 1 : 0))));
	return result;
}
		

static int wav_sample_rate(input_object *obj)
{
	struct wav_local_data *data;	
	int result = 0;	

	if (!obj)
			return result;
	data = (struct wav_local_data *)obj->local_data;
	if (!data) {
			return result;
	}		
	result = data->format.rate;
	return result;
}


static int wav_channels(input_object *obj)
{
	return 2; /* Yes, always stereo, we take care of mono -> stereo converion */
}


static long wav_frame_to_sec(input_object *obj, int frame)
{
	struct wav_local_data *data;	
	unsigned long result = 0;
	unsigned long pos;
	
	if (!obj)
			return result;
	data = (struct wav_local_data *)obj->local_data;	
	if (!data) {
			return result;
	}		
	pos = frame * (APLAY_FRAME_SIZE >> (2 - obj->nr_channels));
	result = (pos / (data->format.rate / 100) / obj->nr_channels / 2);
	return result;
}



static float wav_can_handle(const char *name)
{
	WaveHeader wp;
	FILE *fd;
	struct stat st;
	char *ext;

	ext = strrchr (name, '.');
	if (!ext)
		return 0.0;
	ext++;
	if (strcasecmp(ext, "wav"))
		return 0.0;
/*
	if (stat(name, &st)) return 0.0;
	if (!S_ISREG(st.st_mode)) return 0.0;
	if ((fd = fopen(name, "r")) == NULL) {
		return 0.0;
	}	
	if (fread(&wp, sizeof(WaveHeader), 1, fd) != 1) {
			fclose(fd);
			return 0.0;
	}
	fclose(fd);
    if (wp.main_chunk == WAV_RIFF && wp.chunk_type == WAV_WAVE &&
        wp.sub_chunk == WAV_FMT && 
		(wp.data_chunk == WAV_DATA || wp.data_chunk == WAV_DATA_1)) {
		if (wp.format != WAV_PCM_CODE) {
			fprintf(stderr, "APLAY: can't play not PCM-coded WAVE-files\n");
			return 0.0;
		}
	}
*/	
	return 1.0;
}

static int wav_stream_info(input_object *obj, stream_info *info)
{
	 struct wav_local_data *data;	
	if (!obj || !info)
			return 0;
	data = (struct wav_local_data *)obj->local_data;
	if (!data) {
			return 0;
	}		
	sprintf(info->stream_type, "%d-bit %dKhz %s WAV", 16, 
		data->format.rate / 1000, obj->nr_channels == 2 ? "stereo" : "mono");
	info->author[0] = 0;
	info->status[0] = 0;
	strcpy(info->title, data->wav_name);	
	
	return 1;
}

static int wav_init()
{
	return 1;
}

static void wav_shutdown()
{
	return;
}


input_plugin wav_plugin = {
	INPUT_PLUGIN_VERSION,
	0,
	{ "WAV player v1.01" },
	{ "Andy Lo A Foe" },
	NULL,
	wav_init,
	wav_shutdown,
	NULL,
	wav_can_handle,
	wav_open,
	wav_close,
	wav_play_frame,
	wav_frame_seek,
	wav_frame_size,
	wav_nr_frames,
	wav_frame_to_sec,
	wav_sample_rate,
	wav_channels,
	wav_stream_info,
	NULL,
	NULL
};

input_plugin *input_plugin_info(void)
{
	return &wav_plugin;
}

