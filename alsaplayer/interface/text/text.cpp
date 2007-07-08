/*
 *  text.cpp - Command Line Interface 
 *  Copyright (C) 2001-2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cassert>
#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <cmath>

#include "config.h"

#include "SampleBuffer.h"
#include "CorePlayer.h"
#include "Playlist.h"
#include "utilities.h"
#include "interface_plugin.h"

#define NR_BLOCKS	30
#define SLEEPTIME	1000000

extern int global_quiet;

static char addon_dir[1024];
static bool going = false;
static pthread_mutex_t finish_mutex;
static coreplayer_notifier notifier;

static float vol = 0.0;
static float speed = 0.0;

void stop_notify(void *)
{
	fprintf(stdout, "\n\n");
}


void speed_changed(void *, float new_speed)
{
	speed = new_speed;
}


void volume_changed(void *, float new_vol)
{
	vol = new_vol;
}


void position_notify(void *, int frame)
{
	fprintf(stdout, "Frame: %6d  Vol: %3d   Speed: %.0f    \r", 
		frame, (int)(vol * 100), speed * 100.0);
	fflush(stdout);	
}


int interface_text_init(void)
{
	pthread_mutex_init(&finish_mutex, NULL);
	strcpy(addon_dir, ADDON_DIR);
	return 1;
}


int interface_text_running(void)
{
	if (pthread_mutex_trylock(&finish_mutex) != 0)
		return 1;
	pthread_mutex_unlock(&finish_mutex);
	return 0;
}


int interface_text_stop(void)
{
	going = false;
	pthread_mutex_lock(&finish_mutex);
	pthread_mutex_unlock(&finish_mutex);
	return 1;
}

void interface_text_close(void)
{
	pthread_mutex_destroy(&finish_mutex);
	return;
}


int interface_text_start(Playlist *playlist, int /* argc */, char ** /* argv */)
{
	CorePlayer *coreplayer;
	stream_info info;
	stream_info old_info;
	bool streamInfoRequested = false;
	int nr_frames, pos = -1, old_pos = -1, spaces, c;
	char out_text[81];
		
	memset(&info, 0, sizeof(stream_info));
	memset(&old_info, 0, sizeof(stream_info));

	going = true;

	memset(&notifier, 0, sizeof(notifier));
	notifier.speed_changed = speed_changed;
	notifier.volume_changed = volume_changed;
	notifier.position_notify = position_notify;
	notifier.stop_notify = stop_notify;

	//fprintf(stdout, "\n");
	//playlist->RegisterNotifier(&notifier, NULL);

	// Fall through console player
	pthread_mutex_lock(&finish_mutex);

	// playlist loop
	if (playlist->Length() == 0) {
		if (!global_quiet)
			fprintf(stdout, "Nothing to play.\n");
		pthread_mutex_unlock(&finish_mutex);
		return 0;
	}

	playlist->Play(1);
	playlist->UnPause();
	
	while(going && !playlist->Eof()) {
		unsigned long secs, t_min, t_sec, c_min, c_sec;
		t_min = t_sec = c_min = c_sec = 0;
		streamInfoRequested = false;

		
		// single title loop
		coreplayer = playlist->GetCorePlayer();
		
		while (going && (coreplayer->IsActive() || coreplayer->IsPlaying())) {
			int cur_val, block_val, i;
			
			old_pos = pos;	

			playlist->UnPause();
			
			pos = playlist->GetCurrent();
			if (pos != old_pos) {
				fprintf(stdout, "\n");
				streamInfoRequested = false;
			}
			coreplayer->GetStreamInfo(&info);

			if (global_quiet) {
				dosleep(SLEEPTIME);
				continue;
			}

			nr_frames = coreplayer->GetFrames();
			if (nr_frames >= 0) {
				block_val = secs = coreplayer->GetCurrentTime(nr_frames);
			} else {
				block_val = secs = 0;
			}
			
			t_min = secs / 6000;
			t_sec = (secs % 6000) / 100;
			cur_val = secs = coreplayer->GetCurrentTime();
			if (secs == 0) {
				dosleep(SLEEPTIME);
				continue;
			}	
			c_min = secs / 6000;
			c_sec = (secs % 6000) / 100;
			fprintf(stdout, "\rPlaying (%d/%d): %ld:%02ld ", 
					 playlist->GetCurrent(),
					 playlist->Length(),
					 c_min, c_sec);
			i = 50;
			if (nr_frames >= 0)  {
				fprintf(stdout, "(%ld:%02ld) ", t_min, t_sec);
				i -= 8;		
			} else {
				fprintf(stdout, "(streaming) ");
				i -= 8;
			}	
			if (*info.artist)
				snprintf(out_text, i, "%s - %s", info.artist, info.title);
			else if (*info.title)
				snprintf(out_text, i, "%s", info.title);
			else 
				snprintf(out_text, i, "(no title information available)");
			spaces = i - strlen(out_text);
			fprintf(stdout, "%s", out_text);
			for (c=0; c < spaces; c++) {
				fprintf(stdout, " ");
			}
			fprintf(stdout, "\r");

#ifdef FANCY_INDICATOR			
			// Draw nice indicator
			block_val /= NR_BLOCKS; 

			if (!block_val)
				cur_val = 0;
			else
				cur_val /= block_val;
			
			if (nr_frames >= 0) {
				fprintf(stdout, "[");
				for (i = 0; i < NR_BLOCKS; i++) {
					fputc(cur_val >= i ? '*':' ', stdout);
				}
				fprintf(stdout,"] ");
			}
#endif			
			fflush(stdout);
			dosleep(SLEEPTIME);
		}
	}
	fprintf(stdout, "\n...done playing\n");
	pthread_mutex_unlock(&finish_mutex);

	//playlist->UnRegisterNotifier(&notifier);
	return 0;
}


interface_plugin default_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	"TEXT interface v1.1",
	"Andy Lo A Foe",
	NULL,
	interface_text_init,
	interface_text_start,
	interface_text_running,
	interface_text_stop,
	interface_text_close
};

extern "C" {

	interface_plugin *interface_plugin_info()
	{
		return &default_plugin;
	}

}
