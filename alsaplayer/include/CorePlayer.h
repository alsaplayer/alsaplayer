/*  CorePlayer.h
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
 *
 *  $Id$
 *  
*/ 

#ifndef __CorePlayer_h__
#define __CorePlayer_h__
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include "SampleBuffer.h"
#include "AlsaNode.h"
#include "AlsaSubscriber.h"
#include "input_plugin.h"

// Tunable parameters

#define BUF_SIZE (10240) // Size of a single ringbuffer partition
#define NR_BUF 64	// Number of partitions in ringbuffer
#define NR_CBUF 16	// Number of partition to read ahead

#define MAX_PLUGINS 16

typedef struct _sample_buf
{
	int start;
	_sample_buf *next, *prev;
	SampleBuffer *buf;
} sample_buf;


class CorePlayer // Much more abstraction to come, well maybe not
{
 private:
	char file_path[1024]; 
	int total_frames;
	int write_buf_changed;
	int read_direction;
	int frames_in_buffer;
	int jumped;
	int jump_point;
	int last_read;
	bool streaming;
	int repitched;
	int new_frame_number;
	float pitch_point;
	float pitch;
	float pitch_multi;
	bool producing;
	int volume;
	int pan;
	AlsaNode *node;
	AlsaSubscriber *sub;

	// INPUT plugin stuff
	input_object *the_object;
	input_plugin *plugin; // Pointer to the current plugin
	
	pthread_t producer_thread;
	pthread_mutex_t player_mutex;
	pthread_mutex_t counter_mutex;
	pthread_mutex_t thread_mutex;
	sample_buf *buffer;
	sample_buf *read_buf, *write_buf, *new_write_buf;
	virtual int FrameSeek(int);
	virtual int AvailableBuffers();
	virtual void ResetBuffer();
	virtual void SetSpeedMulti(float multi) { pitch_multi = multi; }
	virtual void update_pitch();
	virtual bool Open();
	virtual void Close();
	static void producer_func(void *data);
	static bool streamer_func(void *, void *, int);
	virtual int pcm_worker(sample_buf *dest, int start, int lin=0);
	virtual int Read32(void *, int);
	virtual int SetDirection(int dir);
	virtual int GetDirection() { return read_direction; }
	void load_input_addons();
	void UnregisterPlugins();
	int RegisterPlugin(input_plugin *the_plugin);
	int plugin_count; // Number of registered plugins
	input_plugin plugins[MAX_PLUGINS]; // Be very optimistic
 public:
	CorePlayer(AlsaNode *node=(AlsaNode *)NULL);
	~CorePlayer();
	AlsaNode *GetNode() { return node; }
	virtual int GetPosition();				// Current position in frames
	virtual int SetSpeed(float val);	// Set the playback speed: 1.0 = 100%
	virtual float GetSpeed();					// Get speed
	virtual int GetVolume() { return volume; }				// Get Volume level
	virtual void SetVolume(int vol) { volume = vol; }	// Set volume level
	virtual int GetPan() { return pan; }	// Get Pan level
	
	virtual void SetPan(int p) { pan = p; }	// Set Pan level: 
																					// 0		= center
																					// -100	= right channel muted
																					// 100  = left channel muted
	
	virtual void SetFile(const char *path);	// Set path to file
	virtual unsigned long GetCurrentTime(int frame=-1);
																		// Returns the time position of frame in
																		// hundreths of seconds
	virtual int GetStreamInfo(stream_info *info); // Return stream info
	virtual int GetFrames();					// Total number of frames
	virtual int GetSampleRate();			// Samplerat of this player
	virtual int GetChannels();				// Number of channels
	virtual int GetFrameSize();				// Frame size in bytes
	input_plugin * GetPlayer(const char *);
	// This one is temporary
	virtual int GetLatency() { if (node) return node->GetLatency(); else return 0; }

	virtual bool PlayFile(const char *);
	virtual bool Start(int reset=1);
	virtual void Stop(int streamer=1);
	virtual int Seek(int pos);
	
	virtual int IsActive() { return streaming; }	
	virtual int IsPlaying() { return producing; }
};


#endif
