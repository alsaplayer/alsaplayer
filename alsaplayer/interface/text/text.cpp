/*
 *  text.cpp - Command Line Interface 
 *  Copyright (C) 2001-2002 Andy Lo A Foe <andy@alsaplayer.org>
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
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <math.h>

#include "config.h"

#include "SampleBuffer.h"
#include "CorePlayer.h"
#include "Playlist.h"
#include "utilities.h"
#include "interface_plugin.h"

#define NR_BLOCKS	30
#define SLEEPTIME	500000

extern int global_quiet;

static char addon_dir[1024];
static bool going = false;
static pthread_mutex_t finish_mutex;
static coreplayer_notifier notifier;

static int vol = 0;
static float speed = 0;

void stop_notify(void *data)
{
	fprintf(stdout, "\n\n");
}


void speed_changed(void *data, float new_speed)
{
	speed = new_speed;
}


void volume_changed(void *data, int new_vol)
{
	vol = new_vol;
}


void position_notify(void *data, int frame)
{
	fprintf(stdout, "Frame: %6d  Vol: %3d   Speed: %.0f    \r", 
		frame, vol, speed * 100.0);
	fflush(stdout);	
}


int interface_text_init()
{
	pthread_mutex_init(&finish_mutex, NULL);
	strcpy(addon_dir, ADDON_DIR);
	return 1;
}


int interface_text_running()
{
	if (pthread_mutex_trylock(&finish_mutex) != 0)
		return 1;
	pthread_mutex_unlock(&finish_mutex);
	return 0;
}


int interface_text_stop()
{
	going = false;
	pthread_mutex_lock(&finish_mutex);
	pthread_mutex_unlock(&finish_mutex);
	return 1;
}

void interface_text_close()
{
	pthread_mutex_destroy(&finish_mutex);
	return;
}


int interface_text_start(Playlist *playlist, int argc, char **argv)
{
	CorePlayer *coreplayer;
	char path[256];
	char *home;
	stream_info info;
	stream_info old_info;
	bool streamInfoRequested = false;
	bool displayProgress = true;
	int nr_frames, pos = -1, old_pos = -1;
	
	memset(&info, 0, sizeof(stream_info));
	memset(&old_info, 0, sizeof(stream_info));

	playlist->UnPause();

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
		fprintf(stdout, "Nothing to play.\n");
		pthread_mutex_unlock(&finish_mutex);
		return 0;
	}	
	while(going && !playlist->Eof()) {
		unsigned long secs, t_min, t_sec, c_min, c_sec;
		t_min = t_sec = c_min = c_sec = 0;
		streamInfoRequested = false;

		
		// single title loop
		coreplayer = playlist->GetCorePlayer();
		
		while (going && (coreplayer->IsActive() || coreplayer->IsPlaying())) {
			int cur_val, block_val, i;
			
			old_pos = pos;	
			pos = playlist->GetCurrent();
			if (pos != old_pos) {
				fprintf(stdout, "\n");
				streamInfoRequested = false;
			}	
			if (!streamInfoRequested) {
				coreplayer->GetStreamInfo(&info);
				if (*info.title && strcmp(info.title, old_info.title) != 0) {
					if (*info.artist)
						fprintf(stdout, "Playing: %s - %s\n", info.artist, info.title);
					else if (*info.title)
						fprintf(stdout, "Playing: %s\n", info.title);
					else
						fprintf(stdout, "Playing: (no information available)\n");
					memcpy(&old_info, &info, sizeof(stream_info));
				} else {
					fprintf(stdout, "Playing...\n");
				}	
				streamInfoRequested = true;
			}

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
			fprintf(stdout, "\r   Time: %02ld:%02ld ", c_min, c_sec);
			if (nr_frames >= 0) 
				fprintf(stdout, "(%02ld:%02ld) ", t_min, t_sec);
			else {
				fprintf(stdout, "-- Live broadcast -- ");
			}	
			// Draw nice indicator
			block_val /= NR_BLOCKS; 

			if (!block_val)
				cur_val = 0;
			else
				cur_val /= block_val;
			
			if (displayProgress && nr_frames >= 0) {
				fprintf(stdout, "[");
				for (i = 0; i < NR_BLOCKS; i++) {
					fputc(cur_val >= i ? '*':' ', stdout);
				}
				fprintf(stdout,"] ");
			}
			fprintf(stdout, "(%03d/%03d)  ",
				playlist->GetCurrent(), playlist->Length());
			fflush(stdout);
			dosleep(SLEEPTIME);
		}
		//fprintf(stdout, "\n\n");
	}
	fprintf(stdout, "\n\n...done playing\n");
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
