/*  SampleBuffer.cpp - SampleBuffer object, used in the ringbuffer
 *  Copyright (C) 1998-2002 Andy Lo A Foe <andy@alsaplayer.org>
 *
 *  This file is part of AlsaPlayer.
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
 *
*/

#include "SampleBuffer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include "alsaplayer_error.h"

// NOTE: THIS IS FOR STEREO 16-BIT SAMPLES ONLY

SampleBuffer::SampleBuffer(int mode, int size)
{
	// Find out the sample size
	switch(mode) {
	 case SAMPLE_STEREO:
		sample_size = 4;
		break;
	 default:
		alsaplayer_error("Unsupported SAMPLE size");
		exit(1);
	}
	
	// Allocate buffer data
	if ((buffer_data = new char[size]) == NULL) {
		alsaplayer_error("Out of memory in SampleBuffer::SampleBuffer()");
		exit(1);
	}

	// Init other stuff
	SetReadDirection(DIR_FORWARD);
	read_index = write_index = 0;
	buffer_size = size / sample_size;
}


SampleBuffer::~SampleBuffer()
{
	delete []buffer_data;
}


void SampleBuffer::Clear()
{
	read_index = write_index = 0;
	//memset(buffer_data, 0, buffer_size * sample_size);
}

int SampleBuffer::Seek(int index)
{
	if (index < 0 || index > write_index) {
	  /* alsaplayer_error("index out of range (%d)\n", index); */
	  return -1;
	}
	read_index = index;
	return read_index;
}


#if 0	// these methods are completely unused; FB
int SampleBuffer::WriteSamples(void *data, int nr)
{
	int add = 0;
	int *dest = (int *)buffer_data + write_index;
	int *src = (int *)data;

	if (nr < GetFreeSamples()) { // Simply copy everything
		//memcpy(dest, data, nr * sample_size);
		for (int c=0; c < nr; c++) {
			*(dest++) = *(src++);
		}
		write_index += nr;
		return nr;
	} else {
		add = GetFreeSamples();
		//memcpy(dest, data, add * sample_size);
		for (int c=0; c < add; c++) {
			*(dest++) = *(src++);
		}
		write_index += add;
		// Paranoia check
		//assert (write_index == GetBufferSize());
		//if (GetReadDirection() == DIR_BACK)  {
		//	read_index = write_index; // Hmmm!!
		//}
		return add;
	}
}


int SampleBuffer::ReadSamples(void *data, int nr)
{
	int *src = (int *)buffer_data + read_index;
	int *dest = (int *)data;

	switch (GetReadDirection()) {
	 case DIR_FORWARD:
		if (nr <= (GetBufferSize() - read_index)) {
			//memcpy(data, src, (nr * sample_size));
			for (int c=0; c < nr; c++) {
				*(dest++) = *(src++);
			}	
			read_index += nr;
			return nr;
		} else {
			int count = GetBufferSize() - read_index;
			//memcpy(data, src, count * sample_size);
			for (int c=0; c < count; c++) {
				*(dest++) = *(src++);
			}
			read_index += count;
			// Paranoia check
			assert(read_index == GetBufferSize());
			return count;
		}
		break;
	 case DIR_BACK:
		//alsaplayer_error("Backwards....");
		int *s, *d;
		//if (read_index > GetFreeSamples())
		//  read_index = GetFreeSamples();
		if (read_index < nr)
		        nr = read_index;
		s = (int *)(buffer_data) + (read_index * sample_size);
		d = (int *)data;
		//alsaplayer_error("From %d to %d", read_index, read_index-nr);
		for (int i=0; i < nr; i++) {
			*(d++) = *(s--);
		}
		read_index -= nr;
		return nr;
	}
	return -1;
}
#endif


void SampleBuffer::SetSamples(int count)
{
	write_index = count;
}

void *SampleBuffer::GetSamples()
{
	return buffer_data;
}

void SampleBuffer::SetReadDirection(int rd)
{
	read_direction = rd;
}

int SampleBuffer::GetSamplesInBuffer()
{
	return write_index;
}

int SampleBuffer::GetBufferSize()
{
	return buffer_size;
}


int SampleBuffer::GetBufferSizeBytes(int frame_size)
{
	if (frame_size < 0) 
		return buffer_size * sample_size;
	else {
		int byte_count = buffer_size * sample_size;
		int frame_mul = frame_size * (byte_count / frame_size);
		return frame_mul;
	}
}


int SampleBuffer::GetReadIndex()
{
	return read_index;
}


int SampleBuffer::GetReadDirection()
{
	return read_direction;
}


void SampleBuffer::ResetRead()
{
#if 1 
	switch (GetReadDirection()) {
		case DIR_FORWARD:
			read_index = 0;
			break;
		case DIR_BACK:
			read_index = write_index;
			break;
	}
#else	
	read_index = 0;
#endif
	return;
}

int SampleBuffer::GetAvailableSamples()
{
	//alsaplayer_error("read = %d, write = %d", read_index, write_index);
	switch(GetReadDirection()) {
	 case DIR_FORWARD:
		if (write_index < read_index) {
			return 0;
		} else {	 
			return (write_index - read_index);
		}
		break;
	 case DIR_BACK:
		return read_index;
	}
	return -1;
}

int SampleBuffer::GetFreeSamples()
{
	return (buffer_size - write_index);
}
