/*
 *  gtk.cpp - GTK interface plugin main file
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

	sprintf(path, "%s/scopes", addon_dir);

	DIR *dir = opendir(path);
	dirent *entry;

	if (dir) {
		while ((entry = readdir(dir)) != NULL) { // For each file in scopes
			if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			sprintf(path, "%s/scopes/%s", addon_dir, entry->d_name);
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
	
	// Scope functions
	scopes = new AlsaSubscriber();
	scopes->Subscribe(the_coreplayer->GetNode(), POS_END);
	scopes->EnterStream(scope_feeder_func, the_coreplayer);

	if (geteuid() == 0) // Drop root if we run suid root
		setuid(getuid());

	gtk_set_locale();
	gtk_init(&argc, &argv);
	gdk_rgb_init();

	home = getenv("HOME");
	if (home && strlen(home) < 128) {
		sprintf(path, "%s/.aprc", home);
		gtk_rc_parse(path);
	} else {
		sprintf(path, "%s/.gtkrc", home);
		gtk_rc_parse(path);
	}

		
	if (playlist->Length())
		playlist->UnPause();

	// Scope addons
	GDK_THREADS_ENTER();
	init_main_window(playlist);
	load_scope_addons();
	gtk_main();
	GDK_THREADS_LEAVE();
	unload_scope_addons();
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
	"GTK+ interface v1.2",
	"Andy Lo A Foe",
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
