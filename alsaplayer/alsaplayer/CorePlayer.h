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
#include <set>
#include "SampleBuffer.h"
#include "AlsaNode.h"
#include "AlsaSubscriber.h"
#include "input_plugin.h"

// Tunable parameters

#define BUF_SIZE (10240)	// Size of a single ringbuffer partition
#define NR_BUF 16		// Number of partitions in ringbuffer
#define NR_CBUF 8		// Number of partitions to read ahead
				// (equals 2.6 seconds). NOTE: NR_CBUF
				// should NEVER exceed (NR_BUF/2), it doesn't
				// make sense otherwise

#define MAX_INPUT_PLUGINS 16

typedef void(*volume_changed_type)(void *, float new_vol);
typedef void(*speed_changed_type)(void *, float new_speed);
typedef void(*pan_changed_type)(void *, float new_pan);
typedef void(*position_notify_type)(void *, int pos);
typedef void(*stop_notify_type)(void *);
typedef void(*start_notify_type)(void *);		

typedef struct _coreplayer_notifier
{
	void *data;
	volume_changed_type volume_changed;
	speed_changed_type speed_changed;
	pan_changed_type pan_changed;
	position_notify_type position_notify;
	start_notify_type start_notify;
	stop_notify_type stop_notify;
} coreplayer_notifier;


typedef struct _sample_buf
{
	int start;
	_sample_buf *next, *prev;
	SampleBuffer *buf;
} sample_buf;


class CorePlayer // Much more abstraction to come, well maybe not
{
 private:
	int total_frames;
	int write_buf_changed;
	int read_direction;
	int frames_in_buffer;
	int jump_point;
	int last_read;
	bool streaming;
	int repitched;
	int new_frame_number;
	float pitch_point;
	float pitch;
	float pitch_multi;
	bool jumped;
	bool producing;
	float volume;
	float pan;
	int output_rate;
	int input_rate;
	AlsaNode *node;
	AlsaSubscriber *sub;
	float save_speed;

	std::set<coreplayer_notifier *> notifiers;
	
	// INPUT plugin stuff
	input_object *the_object;
	input_plugin *plugin; // Pointer to the current plugin
	
	pthread_t producer_thread;
	pthread_mutex_t player_mutex;
	pthread_mutex_t counter_mutex;
	pthread_cond_t producer_ready;
	pthread_mutex_t thread_mutex;
	pthread_mutex_t notifier_mutex;
#if !defined(EMBEDDED)
	// this buffer is used to apply volume/pan and mixing channels
	char *input_buffer;
#endif
	sample_buf *buffer;
	sample_buf *read_buf, *write_buf, *new_write_buf;
	int FrameSeek(int);
	int FilledBuffers();
	void ResetBuffer();
	void SetSpeedMulti(float multi) { pitch_multi = multi; }
	void update_pitch();
	void kill_producer();
	static void producer_func(void *data);
	static bool streamer_func(void *, void *, int);
	int pcm_worker(sample_buf *dest, int start);
	int Read32(void *, int);
	int SetDirection(int dir);
	int GetDirection() { return read_direction; }
	void load_input_addons();
	void unregister_plugins();
	void UnregisterPlugins();
	void Lock();
	void Unlock();
	void LockNotifiers();
	void UnlockNotifiers();
	int RegisterPlugin(input_plugin *the_plugin);

 public:
	// Static members
	static int plugins_loaded;
	static int plugin_count;
	static pthread_mutex_t plugins_mutex;
	static input_plugin plugins[MAX_INPUT_PLUGINS];
	
	CorePlayer(AlsaNode *node=(AlsaNode *)NULL);
	~CorePlayer();

	void RegisterNotifier(coreplayer_notifier *, void *data);
	void UnRegisterNotifier(coreplayer_notifier *);

	AlsaNode *GetNode() { return node; }
	int GetPosition();	// Current position in frames
	void PositionUpdate();	// Notify the interfaces about the position
	int SetSpeed(float val);	// Set the playback speed: 1.0 = 100%
	float GetSpeed();	// Get speed
	float GetVolume() { return volume; }	// Get Volume level
	void SetVolume(float vol);	// Set volume level
	float GetPan() { return pan; }	// Get Pan level
	
	void SetPan(float p);	// Set Pan level: 
					// 0.0	= center
					// -1.0 = right channel muted
					// 1.0  = left channel muted
	
	int GetCurrentTime(int frame=-1);
					// Returns the time position of frame in
					// hundreths of seconds
	int GetStreamInfo(stream_info *info); // Return stream info
	int GetFrames();	// Total number of frames
	int GetTracks();	// Total number of tracks
	int GetSampleRate();	// Samplerat of this player
	int GetChannels();	// Number of channels
	int GetFrameSize();	// Frame size in bytes
	input_plugin * GetPlayer(const char *); // This one is temporary
	int GetLatency() { if (node) return node->GetLatency(); else return 0; }

	bool Open(const char *path = (const char *)NULL);
	void Close();
	bool Start();
	void Stop();
	int Seek(int pos);
	bool CanSeek();

	void Pause ();
	void UnPause ();
	bool IsPaused ();
	
	int IsActive() { return streaming; }	
	int IsPlaying() { return producing; }
};


#endif
