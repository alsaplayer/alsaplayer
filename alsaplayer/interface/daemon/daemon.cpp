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
#include "AlsaPlayer.h"
#include "control.h"

static pthread_mutex_t finish_mutex;
static coreplayer_notifier notifier;

static int vol = 0;
static float speed = 0;
static int busypipe[2];


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
	char dummy;

	// signal finish via pipe
	write(busypipe[1], &dummy, 1);

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
	char session_name[AP_SESSION_MAX];

	playlist->Clear();
	playlist->UnPause();

	if (pipe(busypipe) < 0)
		return 1;

	// The notifier is here only for convenience
	memset(&notifier, 0, sizeof(notifier));
	notifier.speed_changed = speed_changed;
	notifier.volume_changed = volume_changed;
	notifier.position_notify = position_notify;
	notifier.stop_notify = stop_notify;

	playlist->RegisterNotifier(&notifier, NULL);

	pthread_mutex_lock(&finish_mutex);

	// Wait on socket thread
	while (global_session_id < 0) {
		dosleep(10000);
	}
	//fprintf(stdout, "Session %d is starting.\n", global_session_id);
	while (!ap_ping(global_session_id)) {
		dosleep(100000);
	}
	if (ap_get_session_name(global_session_id, session_name)) {
		fprintf(stdout, "Session \"%s\" is ready.\n", session_name);
	}

	// wait for finish, signalled via pipe
	fd_set busyset;
	FD_ZERO(&busyset);
	FD_SET(busypipe[0], &busyset);
	select(busypipe[0] + 1, &busyset, NULL, NULL, NULL);
	close(busypipe[0]);
	close(busypipe[1]);

	pthread_mutex_unlock(&finish_mutex);

	playlist->UnRegisterNotifier(&notifier);
	
	return 0;
}


static interface_plugin daemon_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	"DAEMON interface v1.1",
	"Andy Lo A Foe/Frank Baumgart",
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
