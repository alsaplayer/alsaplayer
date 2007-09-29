/*
 *  gtk.cpp - GTK interface plugin main file
 *  Copyright (C) 2001 Andy Lo A Foe <andy@alsaplayer.org>
 *  Copyright (C) 2007 Madej
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
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <cmath>
#include <glib.h>

#include "config.h"

#include "AlsaPlayer.h"
#include "SampleBuffer.h"
#include "CorePlayer.h"
#include "Playlist.h"
#include "ScopesWindow.h"
#include "gtk_interface.h"
#include "utilities.h"
#include "interface_plugin.h"
#include "alsaplayer_error.h"

static char addon_dir[1024];

static AlsaSubscriber *scopes = NULL;

static CorePlayer *the_coreplayer = NULL;

void unload_scope_addons()
{
	if (scopes)
		delete scopes;
	apUnregiserScopePlugins();
}

void load_scope_addons()
{
	char path[1024];
	struct stat buf;
	scope_plugin *tmp;

	scope_plugin_info_type scope_plugin_info;

	snprintf(path, sizeof(path)-1, "%s/scopes2", addon_dir);

	DIR *dir = opendir(path);
	dirent *entry;

	if (dir) {
		while ((entry = readdir(dir)) != NULL) { // For each file in scopes
			if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			sprintf(path, "%s/scopes2/%s", addon_dir, entry->d_name);
			//alsaplayer_error(path);
			if (stat(path, &buf)) continue;
			if (S_ISREG(buf.st_mode)) {
				void *handle;

				char *ext = strrchr(path, '.');
				if (!ext)
					continue;
				ext++;
				if (strcasecmp(ext, "so"))
					continue;
				if ((handle = dlopen(path, RTLD_NOW |RTLD_GLOBAL))) {
					scope_plugin_info = (scope_plugin_info_type) dlsym(handle, "scope_plugin_info");
					if (scope_plugin_info) { 
#ifdef DEBUG					
						alsaplayer_error("Loading scope addon: %s\n", path);
#endif
						tmp = scope_plugin_info();
						if (tmp) {
								tmp->handle = handle;
								if (apRegisterScopePlugin(tmp) == -1) {
									alsaplayer_error("%s is deprecated", path);
								}
						}		
					} else {
						dlclose(handle);
					}
				} else {
					printf("%s\n", dlerror());
				}
			}
		}
		closedir(dir);
	}	
}

int interface_gtk_init()
{
	strcpy(addon_dir, ADDON_DIR);
	return 1;
}


int interface_gtk_running()
{
	return 1;
}


int interface_gtk_stop()
{
	global_update = -1;
	
	GDK_THREADS_ENTER();

	gdk_flush();
	gtk_exit(0); // This is *NOT* clean :-(
	GDK_THREADS_LEAVE();
	return 1;
}

void interface_gtk_close()
{
	return;
}


void dl_close_scopes();

int interface_gtk_start(Playlist *playlist, int argc, char **argv)
{
	char path[256];
	char *home;

	the_coreplayer = playlist->GetCorePlayer();

	g_thread_init(NULL);
	if (!g_thread_supported()) {
		alsaplayer_error("Sorry - this interface requires working threads.\n");
		return 1;
	}

	gdk_threads_init();

	// Scope functions
	scopes = new AlsaSubscriber();
	scopes->Subscribe(the_coreplayer->GetNode(), POS_END);
	scopes->EnterStream(scope_feeder_func, the_coreplayer);

	gtk_set_locale();
	gtk_init(&argc, &argv);
	gdk_rgb_init();

	home = getenv("HOME");
	if (home) {
		snprintf(path, 255, "%s/.gtkrc-2.0", home);
		gtk_rc_parse(path);
	}

	// Scope addons
	gdk_flush();
	GDK_THREADS_ENTER();
	init_main_window(playlist);
	load_scope_addons();
	gdk_flush();
	gtk_main();
	gdk_flush();
	GDK_THREADS_LEAVE();
	unload_scope_addons();
	destroy_scopes_window();
	GDK_THREADS_ENTER();
	gdk_flush();
	GDK_THREADS_LEAVE();

	playlist->Pause();

	dl_close_scopes();
	return 0;
}


interface_plugin default_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	"GTK+-2.x interface",
	"Andy Lo A Foe/Madej",
	NULL,
	interface_gtk_init,
	interface_gtk_start,
	interface_gtk_running,
	interface_gtk_stop,
	interface_gtk_close
};

extern "C" {

interface_plugin *interface_plugin_info()
{
	return &default_plugin;
}

}
