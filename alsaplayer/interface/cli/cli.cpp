/*
 *  cli.cpp - Command Line Interface 
 *  Copyright (C) 2001 Andy Lo A Foe <andy@alsaplayer.org>
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

static char addon_dir[1024];


int interface_cli_init()
{
	strcpy(addon_dir, ADDON_DIR);
	return 1;
}


int interface_cli_running()
{
	return 1;
}


int interface_cli_stop()
{
	return 1;
}

void interface_cli_close()
{
	return;
}


int interface_cli_start(CorePlayer *coreplayer, Playlist *playlist, int argc, char **argv)
{
	char path[256];
	char *home;
	stream_info info;
	stream_info old_info;

	memset(&info, 0, sizeof(stream_info));
	memset(&old_info, 0, sizeof(stream_info));
	
	playlist->UnPause();
	
	sleep(2);

	// Fall through console player
	while(coreplayer->IsActive() || coreplayer->IsPlaying()) {
			unsigned long secs, t_min, t_sec, c_min, c_sec;
			t_min = t_sec = c_min = c_sec = 0;
			while (coreplayer->IsActive() || coreplayer->IsPlaying()) {
					coreplayer->GetStreamInfo(&info);
					if (strcmp(info.title, old_info.title) != 0) {
							fprintf(stdout, "\nPlaying: %s\n", info.title);
							memcpy(&old_info, &info, sizeof(stream_info));
					}		
					secs = coreplayer->GetCurrentTime(coreplayer->GetFrames());
					t_min = secs / 6000;
					t_sec = (secs % 6000) / 100;
					secs = coreplayer->GetCurrentTime();
					c_min = secs / 6000;
					c_sec = (secs % 6000) / 100;	
					fprintf(stdout, "\r[%02ld:%02ld/%02ld:%02ld]       ",
									c_min, c_sec, t_min, t_sec);
					fflush(stdout);
					//dosleep(100000);
					sleep(1);
			}
			//dosleep(1000000);
			sleep(1);
			fprintf(stdout, "\n");
	}		
	fprintf(stdout, "...done playing\n");
	return 0;
}


interface_plugin default_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	{ "Default CLI interface v1.0" },
	{ "Andy Lo A Foe" },
	NULL,
	interface_cli_init,
	interface_cli_start,
	interface_cli_running,
	interface_cli_stop,
	interface_cli_close
};

extern "C" {

interface_plugin *interface_plugin_info()
{
	return &default_plugin;
}

}
