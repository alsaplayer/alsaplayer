/*  SampleBuffer.h
 *  Copyright (C) 1998-2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#ifndef __SampleBuffer_h__
#define __SampleBuffer_h__
#include <pthread.h>

enum {
	SAMPLE_SIZE_1,
	SAMPLE_SIZE_2,
	SAMPLE_SIZE_4
};


enum {
	DIR_FORWARD,
	DIR_BACK
};

#define SAMPLE_MONO	SAMPLE_SIZE_1
#define SAMPLE_STEREO	SAMPLE_SIZE_2
#define SAMPLE_AC3	SAMPLE_SIZE_6


class SampleBuffer {

 private:
        int sample_size;
        int buffer_size;
        int read_direction;
        int read_index;
        int write_index;
        char *buffer_data;
	pthread_mutex_t	lock;	
 public:
	
	SampleBuffer(int mode, int size);
	~SampleBuffer();
	
	int WriteSamples(void *data, int length);
        int ReadSamples(void *data, int length);
	int Seek(int index);
	void SetReadDirection(int rd);
	void ResetRead();
	int GetBufferSizeBytes(int frame_size = -1);
	int GetBufferSize();
	int GetSamplesInBuffer();
	int GetReadIndex();
	int GetReadDirection();
	void* GetSamples();
	void SetSamples(int);
	int GetFreeSamples();
	int GetAvailableSamples();
	int GetSampleSize() { return sample_size; }
	void Clear(); 
};

#endif
