/*
 *  daemon-xosd.cpp - XOSD Daemon interface
 *  Copyright (C) 2003 Frank Baumgart <godot@upb.de>
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

#include "xosd.h"


#define OSD_FONT	"-*-*-medium-r-*-*-20-*-*-*-*-*-*-*"
#define OSD_TIMEOUT		5
#define OSD_LINES			5
//#define OSD_COLOR		"LawnGreen"
#define OSD_COLOR			"#55ff55"


static pthread_mutex_t finish_mutex;
static coreplayer_notifier notifier;

static volatile bool finished;


void stop_notify(void *data)
{
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
	finished = true;
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
	int session_id = -1;
	CorePlayer *coreplayer;
	stream_info info;
	bool streamInfoRequested = false;
	int pos = -1, pos_old = -1;
	xosd *osd = NULL;

	finished = false;

	playlist->Clear();
	playlist->UnPause();

	// The notifier is here only for convenience
	memset(&notifier, 0, sizeof(notifier));
	notifier.stop_notify = stop_notify;

	playlist->RegisterNotifier(&notifier, NULL);

	pthread_mutex_lock(&finish_mutex);

	// Wait on socket thread
	while (global_session_id < 0) {
		dosleep(10000);
	}
	//printf("Session %d is starting.\n", global_session_id);
	while (!ap_ping(global_session_id)) {
		dosleep(100000);
	}
	if (ap_get_session_name(global_session_id, session_name)) {
		printf("Session \"%s\" is ready.\n", session_name);
	}

	// playlist/wait loop
	while (!finished)
	{
		streamInfoRequested = false;

		// single title loop
		coreplayer = playlist->GetCorePlayer();

		while (coreplayer->IsActive() || coreplayer->IsPlaying())
		{
			pos_old = pos;
			pos = playlist->GetCurrent();

			if (pos != pos_old)
				streamInfoRequested = false;

			if (!streamInfoRequested)
			{
				// create xosd display on demand
				// reason: we might not be running under X11 yet
				if (!osd)
				{
					if ((osd = xosd_create(2)) == NULL)
						printf("osd creation failed: %s\n", xosd_error);
					else
					{
						xosd_set_pos(osd, XOSD_top);
						xosd_set_align(osd, XOSD_left);
						xosd_set_colour(osd, OSD_COLOR);
						xosd_set_font(osd, OSD_FONT);
						xosd_set_shadow_offset(osd, 1);
						xosd_set_horizontal_offset(osd, 20);
						xosd_set_vertical_offset(osd, 20);
						xosd_set_timeout(osd, OSD_TIMEOUT);
					}
				}

				if (osd)
				{
					coreplayer->GetStreamInfo(&info);
					if (*info.artist)
						xosd_display(osd, 0, XOSD_string, info.artist);
					if (*info.title)
						xosd_display(osd, 1, XOSD_string, info.title);
					else
						xosd_display(osd, 1, XOSD_string, "Playing unknown title");

					xosd_wait_until_no_display(osd);
				}

				streamInfoRequested = true;
			}

			dosleep(1000000);
		}

		dosleep(1000000);
	}

	xosd_uninit(osd);
	
	pthread_mutex_unlock(&finish_mutex);

	playlist->UnRegisterNotifier(&notifier);

	return 0;
}


static interface_plugin daemon_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	"XOSD DAEMON interface v0.1",
	"Frank Baumgart",
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
