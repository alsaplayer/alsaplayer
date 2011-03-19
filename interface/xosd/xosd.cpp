/*
 *  daemon-xosd.cpp - XOSD Daemon interface
 *  Copyright (C) 2003 Frank Baumgart <frank.baumgart@gmx.net>
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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
#include "AlsaPlayer.h"
#include "prefs.h"
#include "control.h"

#include "xosd.h"


#define OSD_FONT	"-adobe-helvetica-medium-r-normal-*-24-*-*-*-*-*-*-*"
#define OSD_COLOR	"#55ff55"
#define OSD_TIMEOUT	8
#define OSD_LINES	5
#define OSD_H_OFF	20
#define OSD_V_OFF	20


static pthread_mutex_t finish_mutex;
static coreplayer_notifier notifier;

static xosd *osd = NULL;

static const char *osd_font;
static const char *osd_color;
static int osd_h_off;
static int osd_v_off;
static int osd_timeout;

static volatile bool finished;


void stop_notify(void *data)
{
}


int daemon_init()
{
	pthread_mutex_init(&finish_mutex, NULL);

	osd_font = prefs_get_string(ap_prefs, "xosd_interface", "font", OSD_FONT);
	osd_color = prefs_get_string(ap_prefs, "xosd_interface", "color", OSD_COLOR);
	osd_h_off = prefs_get_int(ap_prefs, "xosd_interface", "h_offset", OSD_H_OFF);
	osd_v_off = prefs_get_int(ap_prefs, "xosd_interface", "v_offset", OSD_V_OFF);
	osd_timeout = prefs_get_int(ap_prefs, "xosd_interface", "timeout", OSD_TIMEOUT);
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


static void displayStreamInfo(CorePlayer *coreplayer)
{
	stream_info info;

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
			xosd_set_colour(osd, osd_color);
			xosd_set_font(osd, osd_font);
			xosd_set_shadow_offset(osd, 1);
			xosd_set_horizontal_offset(osd, osd_h_off);
			xosd_set_vertical_offset(osd, osd_v_off);
			xosd_set_timeout(osd, osd_timeout);
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
}


int daemon_start(Playlist *playlist, int argc, char **argv)
{
	char session_name[AP_SESSION_MAX];
	CorePlayer *coreplayer;
	int pos = -1, pos_old = -1;

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

	while (!finished)
	{
		coreplayer = playlist->GetCorePlayer();

		while (coreplayer->IsActive() || coreplayer->IsPlaying())
		{
			pos_old = pos;
			pos = playlist->GetCurrent();

			if (pos != pos_old)
				displayStreamInfo(coreplayer);

			dosleep(1000000);
		}

		if (!finished)
			dosleep(1000000);
	}

	if (osd)
	{
		xosd_destroy(osd);
		osd = NULL;
	}

	pthread_mutex_unlock(&finish_mutex);

	playlist->UnRegisterNotifier(&notifier);

	return 0;
}


static interface_plugin daemon_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	"XOSD DAEMON interface v0.2",
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
