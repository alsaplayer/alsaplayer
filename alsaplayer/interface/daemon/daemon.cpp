/*
 *  daemon.c - Daemon interface
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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

static pthread_mutex_t finish_mutex;
static coreplayer_notifier notifier;

static int vol = 0;
static float speed = 0;
static bool going = false;

void stop_notify(void *data)
{
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
	//fprintf(stdout, "Frame: %6d  Vol: %3d   Speed: %.0f    \r", 
	//	frame, vol, speed * 100.0);
	//fflush(stdout);	
}


int daemon_init()
{
	pthread_mutex_init(&finish_mutex, NULL);
	return 1;
}


int daemon_running()
{
	if (pthread_mutex_trylock(&finish_mutex) != 0)
		return 1;
	pthread_mutex_unlock(&finish_mutex);
	return 0;
}


int daemon_stop()
{
	going = false;
	pthread_mutex_lock(&finish_mutex);
	pthread_mutex_unlock(&finish_mutex);
	return 1;
}

void daemon_close()
{
	pthread_mutex_destroy(&finish_mutex);
	return;
}


int daemon_start(Playlist *playlist, int argc, char **argv)
{
	playlist->UnPause();

	going = true;

	memset(&notifier, 0, sizeof(notifier));
	notifier.speed_changed = speed_changed;
	notifier.volume_changed = volume_changed;
	notifier.position_notify = position_notify;
	notifier.stop_notify = stop_notify;

	playlist->Clear(); // Clear playlist
	playlist->RegisterNotifier(&notifier, NULL);

	pthread_mutex_lock(&finish_mutex);

	fprintf(stdout, "Daemon started.\n");

	while(going) {
		dosleep(10000); // What should be do here?
	}
	fprintf(stdout, "Daemon exitting.\n");

	pthread_mutex_unlock(&finish_mutex);

	playlist->UnRegisterNotifier(&notifier);
	
	return 0;
}


interface_plugin daemon_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	"DAEMON interface v1.0",
	"Andy Lo A Foe",
	NULL,
	daemon_init,
	daemon_start,
	daemon_running,
	daemon_stop,
	daemon_close
};

#ifdef __cplusplus
extern "C" {
#endif
	interface_plugin *interface_plugin_info()
	{
		return &daemon_plugin;
	}
#ifdef __cplusplus
}
#endif
