/*
 *  text.cpp - Command Line Interface 
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

#define NR_BLOCKS		30

static char addon_dir[1024];


int interface_text_init()
{
	strcpy(addon_dir, ADDON_DIR);
	return 1;
}


int interface_text_running()
{
	return 1;
}


int interface_text_stop()
{
	return 1;
}

void interface_text_close()
{
	return;
}


int interface_text_start(Playlist *playlist, int argc, char **argv)
{
	CorePlayer *coreplayer;
	char path[256];
	char *home;
	stream_info info;
	stream_info old_info;

	memset(&info, 0, sizeof(stream_info));
	memset(&old_info, 0, sizeof(stream_info));
	
	playlist->UnPause();
	
	sleep(2);

	// Fall through console player
	while((coreplayer = playlist->GetCorePlayer()) &&
				(coreplayer->IsActive() || coreplayer->IsPlaying() ||
				 playlist->GetCurrent() != playlist->Length())) {
			unsigned long secs, t_min, t_sec, c_min, c_sec;
			t_min = t_sec = c_min = c_sec = 0;
			while (coreplayer->IsActive() || coreplayer->IsPlaying()) {
					int cur_val, block_val, i;
					coreplayer->GetStreamInfo(&info);
					if (strcmp(info.title, old_info.title) != 0) {
							fprintf(stdout, "\nPlaying: %s\n", info.title);
							memcpy(&old_info, &info, sizeof(stream_info));
					}
					if (coreplayer->GetFrames() == 0 || coreplayer->GetCurrentTime() == 0) {
						dosleep(100000);
						continue;
					}	
					block_val = secs = coreplayer->GetCurrentTime(coreplayer->GetFrames());
					t_min = secs / 6000;
					t_sec = (secs % 6000) / 100;
					cur_val = secs = coreplayer->GetCurrentTime();
					c_min = secs / 6000;
					c_sec = (secs % 6000) / 100;	
					fprintf(stdout, "\r   Time: %02ld:%02ld (%02ld:%02ld) ",
									c_min, c_sec, t_min, t_sec);
					// Draw nice indicator
					block_val /= NR_BLOCKS; 
					cur_val /= block_val;
					//printf("%d - %d\n", block_val, cur_val);
					fprintf(stdout, "[");
					for (i = 0; i < NR_BLOCKS; i++) {
							fprintf(stdout, "%c", cur_val >= i ? '*':' ');
					}
					fprintf(stdout,"]   ");
					fflush(stdout);
					dosleep(100000);
			}
			dosleep(1000000);
			fprintf(stdout, "\n\n");
	}		
	fprintf(stdout, "...done playing\n");
	return 0;
}


interface_plugin default_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	{ "TEXT interface v1.0" },
	{ "Andy Lo A Foe" },
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
