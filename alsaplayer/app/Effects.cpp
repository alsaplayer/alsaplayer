/*  Effects.cpp - Various effects implementations, needs a cleanup
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
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
 *
*/ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Effects.h"

static char buf[DELAY_BUF_SIZE];	
static char cont_buf[MAX_CHUNK];
static int head = 0;

void init_effects()
{
	memset(buf, 0, sizeof(buf));
}

extern "C" {

void clear_buffer(void)
{
	memset(buf, 0, sizeof(buf));
}

}

void buffer_effect(void *buffer, int size)
{
	int tmp;
	char *input = (char *)buffer;

	if ((head + size) > DELAY_BUF_SIZE) {
                memcpy(buf + head, input, DELAY_BUF_SIZE - head);
                tmp = (DELAY_BUF_SIZE - head);
                memcpy(buf, (input + tmp), size - tmp);
                head = size - tmp;
        } else {
                memcpy(buf + head, input, size);
                head += size;
        }
}

void echo_effect32(void *buffer, int size, int delay, int vol)
{
	int tail;
	int tmp;
	int gap = ((44100 * 2 * 2) / 1000) * delay;

	if (size % 4) {
		printf("Warning, size is not a multiple of 4\n");
	}

	tail = (head - gap) < 0 ? 
		DELAY_BUF_SIZE + (head - gap) : head - gap;


	short *s = (short *)(buf + tail);
	short *d = (short *)buffer;
	int i, v;
	if ((tail + size) > DELAY_BUF_SIZE) {
		tmp = DELAY_BUF_SIZE - tail;
		for (i = 0; i < tmp / 2; i++) {
			v = ((*(s++) * vol)/100) + *d;
			*(d++) = (v>32767) ? 32767 : ((v<-32768) ? -32768 : v);
		}
		tmp = size - (DELAY_BUF_SIZE - tail);
		s = (short *)buf;
		for (i = 0; i < tmp / 2; i++) {
			v = ((*(s++) * vol)/100) + *d;
                               *(d++) = (v>32767) ? 32767 : ((v<-32768) ? -32768 : v);
		}
	} else {
		for (i = 0; i < size / 2; i++) {
			v = ((*(s++) * vol)/100) + *d;
			*(d++) = (v>32767) ? 32767 : ((v<-32768) ? -32768 : v);
		}
	}
}


void volume_effect32(void *buffer, int length, int left, int right)
{
	short *data = (short *)buffer;

	if (right == -1) right = left;

	for (int i=0; i < length << 1; i+=2) {
                int v=(int) ((*(data) * left) / 100);
                *(data++)=(v>32767) ? 32767 : ((v<-32768) ? -32768 : v);
        	v=(int) ((*(data) * right) / 100);
                *(data++)=(v>32767) ? 32767 : ((v<-32768) ? -32768 : v);
	}	
}

char *delay_feed(int delay_bytes, int max_size)
{
	int copy;	
	int copied = 0;
	int use_head = head;
	//memset(cont_buf, 0xf, max_size);
#if 1
	if ((copy = use_head - delay_bytes) < 0) { // Wraparound to end of buffer
		copy = abs(copy);
		if (copy <= max_size) {
			memcpy(cont_buf, buf + DELAY_BUF_SIZE - copy, copy);
			copied+=copy;
			memcpy(&cont_buf[copy], buf, max_size - copy);
			copied+=(max_size - copy);
		} else {
			memcpy(cont_buf, buf + DELAY_BUF_SIZE - copy, max_size);
			copied+=max_size;
		}
		//printf("copied = %d\n", copied);
		return cont_buf;
	}
#else
	if ((copy = (use_head - delay_bytes + max_size)) > DELAY_BUF_SIZE) {
		//printf("%d\n", DELAY_BUF_SIZE - (use_head - delay_bytes));

		memcpy(cont_buf, &buf[use_head - delay_bytes], DELAY_BUF_SIZE - (use_head - delay_bytes));
		memcpy(cont_buf + DELAY_BUF_SIZE - copy, buf, max_size - (DELAY_BUF_SIZE - copy));
		return cont_buf;
	}
#endif
	//memcpy(cont_buf, buf + use_head - delay_bytes, max_size);
	return (buf + use_head - delay_bytes);
	//return cont_buf;
}

