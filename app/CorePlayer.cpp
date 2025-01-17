/*  CorePlayer.cpp - Core player object, most of the hacking is done here!
 *	Copyright (C) 1998-2003 Andy Lo A Foe <andy@loafoe.com>
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


/*
	Issues:
		- Things need to be cleaned up!

*/

#include "AlsaPlayer.h"
#include "CorePlayer.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sched.h>
#include <pthread.h>
#include <cmath>
#include "Effects.h"
#include "utilities.h"
#include "alsaplayer_error.h"
#include "ap_string.h"

//#include "MetadataReader.h" //TODO

// some arch don't define PATH_MAX
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

extern void exit_sighandler(int);
static char addon_dir[PATH_MAX];

int CorePlayer::plugin_count = 0;
int CorePlayer::plugins_loaded = 0;
pthread_mutex_t CorePlayer::plugins_mutex = PTHREAD_MUTEX_INITIALIZER;
input_plugin CorePlayer::plugins[MAX_INPUT_PLUGINS];

void CorePlayer::Lock()
{
	if (pthread_mutex_lock(&player_mutex))
		alsaplayer_error("FIRE!!!!!");
}


void CorePlayer::Unlock()
{
	pthread_mutex_unlock(&player_mutex);
}


void CorePlayer::LockNotifiers()
{
	pthread_mutex_lock(&notifier_mutex);
}


void CorePlayer::UnlockNotifiers()
{
	 pthread_mutex_unlock(&notifier_mutex);
}


void CorePlayer::RegisterNotifier(coreplayer_notifier *core_notif, void *data)
{
	if (!core_notif) {
		alsaplayer_error("Notifier is NULL");
		return;
	}
	if (data)
		core_notif->data = data;
	if (core_notif->speed_changed)
		core_notif->speed_changed(core_notif->data, GetSpeed());
	if (core_notif->pan_changed)
		core_notif->pan_changed(core_notif->data, GetPan());
	if (core_notif->volume_changed)
		core_notif->volume_changed(core_notif->data, GetVolume());
	if (core_notif->position_notify)
		core_notif->position_notify(core_notif->data, GetPosition());
	LockNotifiers();
	notifiers.insert(core_notif);
	UnlockNotifiers();
}


void CorePlayer::UnRegisterNotifier(coreplayer_notifier *core_notif)
{
	if (!core_notif)
		return;
	LockNotifiers();
	notifiers.erase(notifiers.find(core_notif));
	UnlockNotifiers();
}

void CorePlayer::load_input_addons()
{
	char path[PATH_MAX];

	DIR *dir;
	struct stat buf;
	dirent *entry;

	input_plugin_info_type input_plugin_info;

	snprintf(path, sizeof (path), "%s/input", addon_dir);

	memset(plugins, 0, sizeof(plugins));

	if ((dir = opendir(path)) == NULL)
		return;

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0) {
				continue;
		}
		snprintf(path, sizeof (path), "%s/input/%s", addon_dir, entry->d_name);
		if (stat(path, &buf)) continue;
		if (!S_ISREG(buf.st_mode)) continue;

		void *handle;

		char *ext = strrchr(path, '.');
		if (!ext)
			continue;

		ext++;

		if (strcasecmp(ext, "so"))
			continue;

		if ((handle = dlopen(path, RTLD_NOW |RTLD_GLOBAL)) == NULL) {
			alsaplayer_error("%s", dlerror());
			continue;
		}

		input_plugin_info = (input_plugin_info_type)
				dlsym(handle, "input_plugin_info");

		if (!input_plugin_info) {
			alsaplayer_error("Could not find input_plugin_info symbol in shared object `%s'", path);
			dlclose(handle);
			continue;
		}

		//alsaplayer_error("Loading input plugin: %s", path);
		input_plugin *the_plugin = input_plugin_info();
		if (the_plugin) {
			the_plugin->handle = handle;
		}
		if (!RegisterPlugin(the_plugin)) {
			alsaplayer_error("Error loading %s", path);
			dlclose(handle);
			continue;
		}
	}
	closedir(dir);
}


CorePlayer::CorePlayer(AlsaNode *the_node)
{
	int i;

	/* Init mutexes */
	pthread_mutex_init(&counter_mutex, NULL);
	pthread_mutex_init(&player_mutex, NULL);
	pthread_mutex_init(&notifier_mutex, NULL);
	pthread_cond_init(&producer_ready, NULL);

	producer_thread = 0;
	total_blocks = 0;
	streaming = false;
	producing = false;
	jumped = false;
	//plugin_count = 0;
	plugin = NULL;
	jump_point = repitched = write_buf_changed = 0;
	new_block_number = 0;
	read_direction = DIR_FORWARD;
	node = the_node;
	pitch = 1.0;
	sub = NULL;
	last_read = -1;
	input_rate = output_rate = 44100;
	SetSpeedMulti(1.0);
	SetSpeed(1.0);
	SetVolume(1.0);
	SetPan(0.0);
	buffer = NULL;
	the_object = NULL;

	save_speed = 0.0;

	// allocate ringbuffer partition pointer table
	if ((buffer = new sample_buf[NR_BUF]) == NULL) {
		alsaplayer_error("Out of memory in CorePlayer::CorePlayer");
		exit(1);
	}

	for (i=0; i < NR_BUF; i++) {
		buffer[i].buf = new SampleBuffer(SAMPLE_STEREO, BUF_SIZE);
		if (buffer[i].buf == NULL) {
			alsaplayer_error("Out of memory in CorePlayer::CorePlayer");
			exit(1);
		}
		// Set up next/prev pointers
		if (i > 0) {
			buffer[i].prev = &buffer[i-1];
			buffer[i-1].next = &buffer[i];
		}
		buffer[i].start = -1;
	}
	// Connect head and tail
	buffer[0].prev = &buffer[NR_BUF-1];
	buffer[NR_BUF-1].next = &buffer[0];

	//memset(plugins, 0, sizeof(plugins));

	read_buf = write_buf = buffer;

	the_object = new input_object;
	the_object->ready = 0;

	pthread_mutex_init(&the_object->object_mutex, NULL);
	ap_strlcpy(addon_dir, ADDON_DIR, sizeof (addon_dir));

	// Load the input addons
	pthread_mutex_lock(&plugins_mutex);
	if (!plugins_loaded) {
		plugins_loaded = 1;
		load_input_addons();
	}
#if !defined(EMBEDDED)
	if ((input_buffer = new char[128 * 1024]) == NULL) {
		alsaplayer_error("cannot allocate mix buffer");
		exit(1);
	}
#endif
	pthread_mutex_unlock(&plugins_mutex);
}



void CorePlayer::ResetBuffer()
{
	for (int i=0; i < NR_BUF; i++) {
		buffer[i].start = -1;
		buffer[i].buf->Clear();
	}
}


CorePlayer::~CorePlayer()
{
	int i;

	UnregisterPlugins();

	for (i=0; i < NR_BUF; i++) {
		delete buffer[i].buf;
	}
	pthread_mutex_destroy(&counter_mutex);
	pthread_mutex_destroy(&player_mutex);
	pthread_mutex_destroy(&the_object->object_mutex);
	delete []buffer;
	delete the_object;
#if !defined(EMBEDDED)
	delete []input_buffer;
#endif
}


void CorePlayer::UnregisterPlugins()
{
	input_plugin *tmp;

	Stop();
	Close();

	Lock();
	for (int i = 0; i < plugin_count; i++) {
		tmp = &plugins[i];
		//alsaplayer_error("Unloading Input plugin: %s", tmp->name);
		if (tmp->handle) {
			//tmp->shutdown(); // Shutdown plugin
			dlclose(tmp->handle);
			tmp->handle = NULL;
			tmp = NULL;
		}
	}
	Unlock();
}

int CorePlayer::RegisterPlugin(input_plugin *the_plugin)
{
	int version;
	int error_count = 0;
	input_plugin *tmp;

	if (the_plugin->name == NULL) {
		alsaplayer_error("No name");
		error_count++;
	}
	if (the_plugin->author == NULL) {
		alsaplayer_error("No author");
		error_count++;
	}
	if (the_plugin->init == NULL) {
		alsaplayer_error("No init function");
		error_count++;
	}
	if (the_plugin->can_handle == NULL) {
		alsaplayer_error("No can_handle function");
		error_count++;
	}
	if (the_plugin->open == NULL) {
		alsaplayer_error("No open function");
		error_count++;
	}
	if (the_plugin->close == NULL) {
		alsaplayer_error("No close function");
		error_count++;
	}
	if (the_plugin->play_block == NULL) {
		alsaplayer_error("No play_block function");
		error_count++;
	}
	if (the_plugin->block_seek == NULL) {
		alsaplayer_error("No block_seek function");
		error_count++;
	}

	if (the_plugin->nr_blocks == NULL) {
		alsaplayer_error("No nr_blocks function");
		error_count++;
	}
	if (the_plugin->frame_count == NULL) {
		alsaplayer_error("No frame_count function");
		error_count++;
	}
	if (the_plugin->block_size == NULL) {
		alsaplayer_error("No block_size function");
		error_count++;
	}
	if (the_plugin->channels == NULL) {
		alsaplayer_error("No channels function");
		error_count++;
	}
	if (the_plugin->stream_info == NULL) {
		alsaplayer_error("No stream_info function");
//		error_count++; // we don't require this function since generic metadata parsing
	}
	if (the_plugin->shutdown == NULL) {
		alsaplayer_error("No shutdown function");
		error_count++;
	}
	if (error_count) {
	    	alsaplayer_error("At least %d error(s) were detected", error_count);
		return 0;
	}

	tmp = &plugins[plugin_count];
	tmp->version = the_plugin->version;
	if (tmp->version) {
		if ((version = tmp->version) != INPUT_PLUGIN_VERSION) {
			alsaplayer_error("Wrong version number on plugin (v%d, wanted v%d)",
					version - INPUT_PLUGIN_BASE_VERSION,
					INPUT_PLUGIN_VERSION - INPUT_PLUGIN_BASE_VERSION);
			// this is a minor error actually
			// no need to stop there
//			return 0;
		}
	}

	tmp->name = the_plugin->name;
	tmp->author = the_plugin->author;
	tmp->init = the_plugin->init;
	tmp->can_handle = the_plugin->can_handle;
	tmp->open = the_plugin->open;
	tmp->close = the_plugin->close;
	tmp->play_block = the_plugin->play_block;
	tmp->block_seek = the_plugin->block_seek;
	tmp->block_size = the_plugin->block_size;
	tmp->nr_blocks = the_plugin->nr_blocks;
	tmp->block_to_sec = the_plugin->block_to_sec;
	tmp->sample_rate = the_plugin->sample_rate;
	tmp->channels = the_plugin->channels;
	tmp->stream_info = the_plugin->stream_info;
	tmp->shutdown = the_plugin->shutdown;
	tmp->nr_tracks = the_plugin->nr_tracks;
	tmp->track_seek = the_plugin->track_seek;

	if (!tmp->init()) {
		alsaplayer_error("Plugin failed to initialize (\"%s\")", tmp->name);
		return 0;
	}
	plugin_count++;

	if (global_verbose)
		alsaplayer_error("Loading Input plugin: %s", tmp->name);

	return 1;
}


int CorePlayer::GetCurrentTime(int block)
{
	 long result = 0;

	Lock();
	if (plugin && the_object && the_object->ready) {
		result = plugin->block_to_sec(the_object, block < 0 ? GetPosition()
			: block);
	}
	Unlock();
	if (result < 0) {
		alsaplayer_error("Huh, negative????");
	}
	return (int)result;
}


int CorePlayer::GetPosition()
{
	int result;

	if (jumped)	// HACK!
		return jump_point;
	if (read_buf && plugin && the_object && the_object->ready) {
		if (read_buf->start < 0)
			return 0;

		int block_size = plugin->block_size(the_object);
		if (block_size)  {
			result = (read_buf->start + (read_buf->buf->read_index * 4) / block_size);
			if (result < 0) {
				alsaplayer_error("Found the error!");
			}
			return result;
		} else
			return 0;
	} else {
		return 0;
	}
}


int CorePlayer::GetBlocks()
{
	int result = 0;

	Lock();
	if (plugin && the_object && the_object->ready)
		result = plugin->nr_blocks(the_object);
	Unlock();

	return result;
}


int CorePlayer::GetTracks()
{
	int result = 0;
	Lock();
	if (plugin && the_object && the_object->ready) {
		if (plugin->nr_tracks) {
			result = plugin->nr_tracks(the_object);
		} else {
			result = 1;
		}
	}
	Unlock();

	return result;
}


int CorePlayer::GetChannels()
{
	return 2;	// Hardcoded for now
}

int CorePlayer::GetBlockSize()
{
	int result = 0;

	Lock();
	if (plugin && the_object && the_object->ready)
		result = plugin->block_size(the_object);
	Unlock();

	return result;
}


int CorePlayer::GetSampleRate()
{
	int result = 0;

	Lock();
	if (plugin && the_object && the_object->ready)
		result = plugin->sample_rate(the_object);
	Unlock();

	return result;
}


int CorePlayer::GetStreamInfo(stream_info *info)
{
	int result = 0;

	memset(info, 0, sizeof(stream_info));

	Lock();
	if (plugin && plugin->stream_info && info && the_object) {
		result = plugin->stream_info(the_object, info);
	}
	if (plugin && info && the_object) {
		if (plugin->stream_info) { // plugin provides its own metadata parser
			result = plugin->stream_info(the_object, info);
		}
		else{ // we'll use generic metadata parser
//TODO			result = get_stream_info (the_object->path, info);
		}
	}
	Unlock();

	return result;
}


// This is the first function to get documented. Believe it or not
// it was the most difficult to get right! We absolutely NEED to have
// a known state of all the threads of our object!

bool CorePlayer::Start()
{
	float result;
	int tries;

	if (IsPlaying())
		return false;

	if (!node) {
		alsaplayer_error("CorePlayer does not have a node");
		return false;
	}
	Lock();
	// First check if we have a filename to play
	if (!the_object->ready) {
		Unlock();
		return false;
	}
	producing = true; // So Read32() doesn't generate an error
	streaming = true;
	jumped = false;

	ResetBuffer();

	last_read = -1;
	output_rate = node->SamplingRate();
	input_rate = plugin->sample_rate(the_object);

	if (input_rate == output_rate)
		SetSpeedMulti(1.0);
	else
		SetSpeedMulti((float) input_rate / (float) output_rate);
	update_pitch();
	write_buf_changed = 0;
	read_buf = write_buf;
	result = GetSpeed();

	if (result < 0.0) {
		int start_block = plugin->nr_blocks(the_object) - 16;
		write_buf->start = start_block > 0 ? start_block : 0;
		SetDirection(DIR_BACK);
	} else {
		write_buf->start = 0;
		SetDirection(DIR_FORWARD);
	}
	pthread_create(&producer_thread, NULL,
		(void * (*)(void *))producer_func, this);

	// allow producer to actually start producing
	dosleep(20000);
	//alsaplayer_error("Prebuffering...");
	// wait up to 4 seconds to get all (really: half) the PCM buffers filled
	tries = 100;
	while (--tries && (FilledBuffers() < NR_CBUF / 2) && producing) {
		//alsaplayer_error("Waiting for buffers...");
		dosleep(40000);
	}
	sub = new AlsaSubscriber();
	if (!sub) {
		alsaplayer_error("Subscriber creation failed :-(\n");
		Unlock();
		return false;
	}
	sub->Subscribe(node);
	//alsaplayer_error("Starting streamer thread...");
	sub->EnterStream(streamer_func, this);
	Unlock();
	// Notify
	std::set<coreplayer_notifier *>::const_iterator i;
	LockNotifiers();
	for (i = notifiers.begin(); i != notifiers.end(); i++) {
		if ((*i)->start_notify) {
			(*i)->start_notify((*i)->data);
		}
	}
	UnlockNotifiers();
	return true;
}


void CorePlayer::Stop()
{
	Lock();

	if (sub) {
        	delete sub;
        	sub = NULL;
	}
	producing = false;
	streaming = false;
	//alsaplayer_error("Signalling 6...");
	pthread_cond_signal(&producer_ready);
#ifdef DEBUG
	alsaplayer_error("Waiting for producer_thread to finish...");
#endif
	if (producer_thread) {
	    if (pthread_join(producer_thread, NULL)) {
		    alsaplayer_error("producer_thread not running...");
	    }
	}
	pthread_mutex_destroy(&counter_mutex);

	pthread_mutex_init(&counter_mutex, NULL);
	producer_thread = 0;

	//alsaplayer_error("producer_func is gone now...");

	ResetBuffer();
	Unlock();

	// Notify
	std::set<coreplayer_notifier *>::const_iterator i;
	LockNotifiers();
	for (i = notifiers.begin(); i != notifiers.end(); i++) {
		if ((*i)->stop_notify) {
			(*i)->stop_notify((*i)->data);
		}
	}
	UnlockNotifiers();
}


void CorePlayer::SetVolume(float val)
{
	if (val > 2.0) {
		alsaplayer_error("ERROR: volume out of bound (%.2f)", val);
		return;
	}
	volume = val;

	std::set<coreplayer_notifier *>::const_iterator i;
	LockNotifiers();
	for (i = notifiers.begin(); i != notifiers.end(); i++) {
		if ((*i)->volume_changed)
			(*i)->volume_changed((*i)->data, volume);
	}
	UnlockNotifiers();
}

void CorePlayer::SetPan(float val)
{
	pan = val;

	std::set<coreplayer_notifier *>::const_iterator i;
	LockNotifiers();
	for (i = notifiers.begin(); i != notifiers.end(); i++) {
		if ((*i)->pan_changed)
			(*i)->pan_changed((*i)->data, pan);
	}
	UnlockNotifiers();
}


void CorePlayer::PositionUpdate()
{
	std::set<coreplayer_notifier *>::const_iterator i;
	LockNotifiers();
	for (i = notifiers.begin(); i != notifiers.end(); i++) {
		if ((*i)->position_notify)
			(*i)->position_notify((*i)->data, GetPosition());
	}
	UnlockNotifiers();
}


int CorePlayer::SetSpeed(float val)
{
	if (val < 0.0 && !CanSeek())
		val = 0.0; // Do not allow reverse play if we can't seek
	//alsaplayer_error("new speed = %.2f", val);
	pitch_point = val;
	repitched = 1;

	std::set<coreplayer_notifier *>::const_iterator i;

	LockNotifiers();
	for (i = notifiers.begin(); i != notifiers.end(); i++) {
		if ((*i)->speed_changed)
			(*i)->speed_changed((*i)->data, pitch_point);
	}
	UnlockNotifiers();

	return 0;
}

float CorePlayer::GetSpeed()
{
	if (repitched) { // Pitch was changed so return this instead
		return pitch_point;
	}
	float ret_pitch = pitch / pitch_multi;
	return (read_direction == DIR_FORWARD ? ret_pitch : -ret_pitch);
}


bool CorePlayer::CanSeek()
{
	if (plugin && the_object && the_object->ready) {
		if (the_object->flags & P_SEEK)
			return true;
	}
	return false;
}

int CorePlayer::Seek(int index)
{
	jump_point = index;
	jumped = true;
	return 0;
}


int CorePlayer::BlockSeek(int index)
{
	if (plugin && the_object && the_object->ready)
		index = plugin->block_seek(the_object, index);
	else
		index = -1;
	return index;
}


void CorePlayer::Close()
{
	Lock();
	if (plugin && the_object && the_object->ready) {
		free (the_object->path);
		the_object->ready = 0;
		plugin->close(the_object);
	}
	Unlock();
}


// Find best plugin to play a file
input_plugin *
CorePlayer::GetPlayer(const char *path)
{
	// Check we've got a path
	if (!*path) {
		return NULL;
	}

	float best_support = 0.0;
	input_plugin *best_plugin = NULL;

	// Go through plugins, asking them
	for (int i = 0; i < plugin_count; i++) {
		input_plugin *p = plugins + i;
		float sl = p->can_handle(path);
		if (sl > best_support) {
			best_support = sl;
			best_plugin = p;
			if (sl == 1.0) break;
		}
	}

	// Return best plugin, if there is one
	if(best_support <= 0.0) return NULL;
	return best_plugin;
}


bool CorePlayer::Open(const char *path)
{
	input_plugin *best_plugin;
	int block_size = 0;

	Close();

	if (path == NULL || strlen(path) == 0) {
		alsaplayer_error("No path supplied");
		return false;
	}
	if ((best_plugin = GetPlayer(path)) == NULL) {
		alsaplayer_error("No suitable plugin found for \"%s\"", path);
		return false;
	}
	Lock();

	the_object->local_data = NULL;

	if (best_plugin->open(the_object, path)) {
		if ((block_size = best_plugin->block_size(the_object)) > BUF_SIZE) {
			alsaplayer_error("CRITICAL ERROR: this plugin advertised a buffer size\n"
					 "larger than %d bytes. This is not supported by AlsaPlayer!\n"
					 "Contact the author to fix this problem.\n"
					 "TECHNICAL: A possible solution is to divide the block size\n"
					 "in 2 and double the number of effective blocks. This can be\n"
					 "handled relatively easily in the plugin (hopefully).\n\n"
					 "We will retreat, as chaos and despair await us on\n"
					 "this chosen path........................................", BUF_SIZE);
			best_plugin->close(the_object);
			Unlock();
			return false;
		} else {
			if (!block_size) {
					alsaplayer_error("No blocksize");
					best_plugin->close(the_object);
					Unlock();
					return false;
			}
			// result = true
		}
	} else {
		best_plugin->close(the_object);
		Unlock();
		return false;
	}

	// Copy path into input_object structure for future use
	the_object->path = strdup (path); // including trailing \0

	the_object->ready = 1;
	plugin = best_plugin;

	blocks_in_buffer = read_buf->buf->GetBufferSizeBytes(plugin->block_size(the_object)) / plugin->block_size(the_object);

	Unlock();

	return true;
}

#ifdef DEBUG
void print_buf(sample_buf *start)
{
	sample_buf *c = start;
	for (int i = 0; i < NR_BUF; i++, c = c->next) {
		alsaplayer_error("%d ", c->start);
	}
}
#endif

int CorePlayer::SetDirection(int direction)
{
	sample_buf *tmp_buf;
	int buffers = 0;

	if (read_direction != direction) {
		tmp_buf = read_buf;
		switch(direction) {
		 case DIR_FORWARD:
			while (buffers < NR_BUF-1 && tmp_buf->next->start ==
			       (tmp_buf->start + blocks_in_buffer)) {
				buffers++;
				tmp_buf = tmp_buf->next;
			}
			break;
	 	  case DIR_BACK:
			while (buffers < NR_BUF-1 && tmp_buf->prev->start ==
			       (tmp_buf->start - blocks_in_buffer)) {
				buffers++;
				tmp_buf = tmp_buf->prev;
			}
		}
		new_write_buf = tmp_buf;
		new_block_number = new_write_buf->start;
		write_buf_changed = 1;
		read_direction = direction;
		//alsaplayer_error("Signalling...1");
		pthread_cond_signal(&producer_ready);
	}
	return 0;

}


// return number of buffers already filled with (decoded) PCM data
//
int CorePlayer::FilledBuffers()
{
	int result = 0;
	sample_buf *tmp = read_buf;

	if (read_buf == write_buf) {
		return 0;
	}
	switch (read_direction) {
	 case DIR_FORWARD:
		while (tmp->next != write_buf) {
			tmp = tmp->next;
			result++;
		}
		break;
	 case DIR_BACK:
		while (tmp->prev != write_buf) {
			tmp = tmp->prev;
			result++;
		}
		break;
	}
	return result; // We can't use a buffer that is potentially being written to
			 // so a value of 1 should still return no buffers
}


void CorePlayer::update_pitch()
{
	if (pitch_point < 0) {
		SetDirection(DIR_BACK);
		pitch = -pitch_point;
	} else {
		SetDirection(DIR_FORWARD);
		pitch = pitch_point;
	}
	pitch *= pitch_multi;

	repitched = 0;
}


/*
	FIXME: some documentation needed

	returns the number of samples put into output buffer
	negative return values indicate errors
*/
int CorePlayer::Read32(void *data, int samples)
{
	if (repitched) {
		update_pitch();
	}

	if (pitch == 0.0 || (read_buf == write_buf)) {
		if (write_buf->next->start == -2 || !producing) {
			return -2;
		}
		//alsaplayer_error("Nothing to play (spitting out silence)");
		memset(data, 0, samples * 4);
		return samples;
	}
	int use_read_direction = read_direction;
	int *out_buf = (int *)data;
	int *in_buf = (int *)read_buf->buf->buffer_data;
	in_buf += read_buf->buf->read_index;
	int buf_index = 0;
	int tmp_index = 0;
	int check_index = read_buf->buf->read_index;
	int base_corr = 0;
	float use_pitch = pitch;

	if (jumped) {
		int i;
		jumped = false;
		new_write_buf = read_buf;
		// ---- Testing area ----
		sample_buf *sb;
		for (i=0, sb = buffer; i < NR_BUF; i++, sb = sb->next) {
			sb->start = -1;
			sb->buf->Clear();
		}
		// ---- Testing area ----
		new_write_buf->start = jump_point;
		new_block_number = jump_point;
		write_buf_changed = 1;
		//alsaplayer_error("Signalling 2...");
		pthread_cond_signal(&producer_ready);
	}
#ifdef EMBEDDED
	// provide a fast path implementation for the common case (no pitch)
	if ((use_read_direction == DIR_FORWARD) && (use_pitch == 1.0)) {

		// calculate amount of samples available in current read buffer
		int samplesInBuffer = read_buf->buf->write_index - check_index;

		if (samplesInBuffer >= samples)
		{
			// copy the whole block (usually 4KB) at once
			memcpy(out_buf, in_buf, samples * 4);
			tmp_index = samples - 1;
		}
		else
		{
			if (samplesInBuffer <= 0) {
				memset(data, 0, samples * 4);
				return samples;
			}
			// use rest of current block
			if (samplesInBuffer)
				memcpy(out_buf, in_buf, samplesInBuffer * 4);

			// get next block
			read_buf = read_buf->next;
			//alsaplayer_error("Signalling 3...");
			pthread_cond_signal(&producer_ready);
			read_buf->buf->SetReadDirection(DIR_FORWARD);
			read_buf->buf->ResetRead();
			in_buf = (int *)read_buf->buf->buffer_data;
			check_index = samples - samplesInBuffer;
			memcpy(out_buf + samplesInBuffer, in_buf, (samples - samplesInBuffer) * 4);
			tmp_index = -1;
		}
	}
	else
#endif
	if (use_read_direction == DIR_FORWARD) {
		while (buf_index < samples) {
			tmp_index = (int) ((float)use_pitch*(float)(buf_index-base_corr));
			if ((check_index + tmp_index) > (read_buf->buf->write_index)-1) {
				if (read_buf->next->start < 0 || read_buf->next == write_buf) {
					if (read_buf->next->start == -2) {
						//alsaplayer_error("Next in queue (2)");
						return -2;
					}
					memset(data, 0, samples * 4);
					if (!producing) {
						//alsaplayer_error("blah 1");
						return -1;
					}
					return samples;
				} else if (read_buf->next->start !=
					read_buf->start + blocks_in_buffer) {
					alsaplayer_error("------ WTF!!? %d - %d",
					read_buf->next->start,
					read_buf->start);
				}
				read_buf = read_buf->next;
				//alsaplayer_error("Signalling 4...");
				pthread_cond_signal(&producer_ready);
				read_buf->buf->SetReadDirection(DIR_FORWARD);
				read_buf->buf->ResetRead();
				in_buf = (int *)read_buf->buf->buffer_data;
				base_corr = buf_index;
				check_index = 0;
				// Recalc
				tmp_index = (int)((float)use_pitch*(float)(buf_index-base_corr));
			}
			out_buf[buf_index++] =  *(in_buf + tmp_index);
		}
	} else { // DIR_BACK
		while (buf_index < samples) {
			tmp_index = (int)((float)use_pitch*(float)(buf_index-base_corr));
			if ((check_index - tmp_index) < 0) {
				if (read_buf->prev->start < 0 ||
					read_buf->prev == write_buf) {
				//	printf("Back (%d %d) ", FilledBuffers(), read_buf->prev->start);
				//	print_buf(read_buf->prev);
					if (!producing) {
						//printf("blah 2\n");
						return -1;
					}
					memset(data, 0, samples * 4);
					return samples;
				}
				read_buf = read_buf->prev;
				//alsaplayer_error("Signalling 5...");
				pthread_cond_signal(&producer_ready);
				read_buf->buf->SetReadDirection(DIR_BACK);
				read_buf->buf->ResetRead();
				int buf_size = read_buf->buf->write_index;
				in_buf = (int *)read_buf->buf->buffer_data;
				in_buf += (buf_size + (check_index - tmp_index));
				base_corr = buf_index;
				check_index = buf_size-1;
				// Recalc
				tmp_index = (int)((float)use_pitch*(float)(buf_index-base_corr));
			}
			out_buf[buf_index++] = *(in_buf - tmp_index);
		}
	}

	read_buf->buf->Seek(check_index + ((use_read_direction == DIR_FORWARD) ?
		 tmp_index+1 : -((tmp_index+1) > check_index ? check_index : tmp_index+1)));
	if (!samples) alsaplayer_error("Humm, I copied nothing?");

	return samples;
}


int CorePlayer::pcm_worker(sample_buf *dest, int start)
{
	int result = 0;
	int blocks_read = 0;
	int samples_written = 0;
	short *sample_buffer;

	if (!dest || !the_object || !the_object->ready) {
		return -1;
	}

	int count = dest->buf->GetBufferSizeBytes(plugin->block_size(the_object));
	dest->buf->Clear();
	if (last_read != start) {
		BlockSeek(start);
	}
	if (start < 0) {
		return -1;
	}
	sample_buffer = dest->buf->buffer_data;
	while (count > 0 && producing) {
		if (!plugin->play_block(the_object, sample_buffer + samples_written)) {
			dest->start = start;
#ifdef DEBUG
			alsaplayer_error("blocks read = %d", blocks_read);
#endif
                        count = 0;
		} else {
			samples_written += plugin->block_size(the_object); // OPTIMIZE!
			blocks_read++;
		}
		count -= plugin->block_size(the_object);
	}
	dest->start = start;
	last_read = dest->start + blocks_read;
	dest->buf->SetSamples(samples_written >> 1);

	// Check if rates are still the same
	if ((result = plugin->sample_rate(the_object)) != input_rate) {
		alsaplayer_error("WARNING: Sample rate changed in midstream (new = %d)", result);
		input_rate = result;
		SetSpeedMulti((float) input_rate / (float) output_rate);
		update_pitch();
	}
	return blocks_read;
}


// fill PCM buffers with (decoded) data
//
void CorePlayer::producer_func(void *data)
{
	CorePlayer *obj = (CorePlayer *)data;
	int blocks_read;

#ifdef DEBUG
	alsaplayer_error("Starting new producer_func...");
#endif
	while (obj->producing) {
		if (obj->write_buf_changed) {
			obj->write_buf_changed = 0;
			obj->write_buf = obj->new_write_buf;
			obj->write_buf->start = obj->new_block_number;
		}
		// still at least one buffer left to fill?
		if (obj->FilledBuffers() < (NR_CBUF-1)) {
			//alsaplayer_error("producer: filling buffer");
			switch (obj->read_direction) {
			 case DIR_FORWARD:
				blocks_read = obj->pcm_worker(obj->write_buf, obj->write_buf->start);
				if (blocks_read != obj->blocks_in_buffer) {
					obj->producing = false;
					obj->write_buf->next->start = -2;
				}
				obj->write_buf = obj->write_buf->next;
				obj->write_buf->start = obj->write_buf->prev->start + obj->blocks_in_buffer;
				break;
			 case DIR_BACK:
				blocks_read = obj->pcm_worker(obj->write_buf, obj->write_buf->start);
				if (blocks_read != obj->blocks_in_buffer) {
					if (obj->write_buf->start >= 0)
						alsaplayer_error("an ERROR occurred or EOF (%d)",
							obj->write_buf->start);
					obj->write_buf->prev->start = -2;
					obj->producing = false;
				}
				obj->write_buf = obj->write_buf->prev;
				obj->write_buf->start = obj->write_buf->next->start -
					obj->blocks_in_buffer;
				break;
			}
		} else {
			//alsaplayer_error("producer: waiting for free buffer");
			pthread_mutex_lock(&obj->counter_mutex);
			//alsaplayer_error("Waiting for a signal...");
			pthread_cond_wait(&obj->producer_ready, &obj->counter_mutex);
			//alsaplayer_error("Got signalled...");
			pthread_mutex_unlock(&obj->counter_mutex);
			//alsaplayer_error("producer: unblocked");
		}
	}
	//alsaplayer_error("Exiting producer_func (producing = %d)", obj->producing);
	pthread_exit(NULL);
}

extern int global_bal;
extern int global_vol_left;
extern int global_vol_right;
extern int global_vol;
extern int global_pitch;
extern int global_reverb_on;
extern int global_reverb_delay;
extern int global_reverb_feedback;

#ifdef EMBEDDED
// the simple streamer only handles a single stream with no software
// volume/pan calculation; intended for slow systems/embedded system use
bool CorePlayer::streamer_func(void *arg, void *data, int size)
{
	CorePlayer *obj = (CorePlayer *)arg;

	if (obj->Read32(data, size / sizeof(short)) >= 0)
		return true;

	obj->streaming = false;
	return false;
}

#else

bool CorePlayer::streamer_func(void *arg, void *data, int size)
{
	CorePlayer *obj = (CorePlayer *)arg;
	int count;

	if ((count = obj->Read32(obj->input_buffer, size / sizeof(short))) >= 0) {
		float v, p, left, right;
		p = obj->pan;
		v = obj->volume;
		if (v != 1.0 || p != 0.0) {
			if (p == 0.0) {
				left = right = 1.0;
			} else 	if (p > 0) {
				right = 1.0;
				left = 1.0 - p;
			} else {
				left = 1.0;
				right = 1.0 + p;
			}
			if (v != 1.0) {
				left *= v;
				right *= v;
			}
			//printf("v = %d, p = %d, left = %d, right = %d\n",
			//	p, v, left, right);
			volume_effect32(obj->input_buffer, count, left, right);
		}
		// Now add the data to the current stream
		int16_t *in_buffer;
		int16_t *out_buffer;
		int32_t level;
		in_buffer = (int16_t *)obj->input_buffer;
		out_buffer = (int16_t *)data;
		for (int i=0; i < size ; i++) {
				level = *(in_buffer++) + *out_buffer;
				if (level > 32767) level = 32767;
				else if (level < -32768) level = -32768;
				*(out_buffer++) = (int16_t) level;
		}
		return true;
	} else {
		//alsaplayer_error("Exiting from streamer_func...");
		obj->streaming = false;
		return false;
	}
}
#endif


bool CorePlayer::IsPaused()
{
	return ((GetSpeed() == 0.0) ? true : false);
}


void CorePlayer::Pause()
{
	save_speed = GetSpeed ();
	SetSpeed (0.0);
}


void CorePlayer::UnPause()
{
	if (save_speed)
	    SetSpeed (save_speed);
	else
	SetSpeed (1.0);
}
