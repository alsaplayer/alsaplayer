/*  scopes.cpp
 *  Copyright (C) 2003 Andy Lo A Foe <andy@alsaplayer.org>
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
 *
 *  $Id$
 *
*/ 
#include "scopes.h"
#include "support.h"
#include "AlsaPlayer.h"
#include "AlsaNode.h"
#include "AlsaSubscriber.h"
#include "alsaplayer_error.h"
#include "alsaplayer_fft.h"
#include "prefs.h"
#include <pthread.h>
#include <dlfcn.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static scope_entry *root_scope = NULL;
static pthread_mutex_t sl_mutex;
static AlsaSubscriber *scopes = NULL;

void unregister_scopes(void);
bool register_scope(scope_plugin *plugin, bool, void *);

void unload_scope_addons()
{
	unregister_scopes();
	if (scopes)
		delete scopes;
}


void load_scope_addons()
{
	char path[1024];
	struct stat buf;
	scope_plugin *tmp;
	scope_plugin_info_type scope_plugin_info;

	snprintf(path, sizeof(path)-1, "%s/scopes2", global_pluginroot);

	DIR *dir = opendir(path);
	dirent *entry;

	if (dir) {
		while ((entry = readdir(dir)) != NULL) { // For each file in scopes
			if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0) {
				continue;
			}
			sprintf(path, "%s/scopes2/%s", global_pluginroot, entry->d_name);
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
						alsaplayer_error("Loading scope addon: %s", path);
						tmp = scope_plugin_info();
						if (tmp) {
								tmp->handle = handle;
								if (!register_scope(tmp, false, NULL)) {
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


void dl_close_scopes()
{
	scope_entry *current = root_scope;

	while (current) {
		if (current->sp) {
			dlclose(current->sp->handle);
		}	
		current = current->next;
	}	
}

void scope_entry_destroy_notify(gpointer)
{
}

#define SCOPE_BUFFER 2048

bool  scope_feeder_func(void *arg, void *data, int size) 
{
	static char buf[32768];
	static int latency = -1;
	static int fft_buf[512];
	static int fill = 0;
	static int left = 0;
	static int init = 0;
	static int buf_size = 0;
	static AlsaNode *the_node = NULL;
	int i;
	short *sound;
	int *left_pos;
	int *right_pos;

	static double fftmult[FFT_BUFFER_SIZE / 2 + 1];

	static sound_sample left_actEq[SCOPE_BUFFER];
	static double left_fftout[FFT_BUFFER_SIZE / 2 + 1];
	static fft_state *left_fftstate;

	static sound_sample right_actEq[SCOPE_BUFFER];
	static double right_fftout[FFT_BUFFER_SIZE / 2 + 1];
	static fft_state *right_fftstate;

	sound_sample *left_newset;
	sound_sample *right_newset;

	//alsaplayer_error("In scope_feeder_func()");
	
	if (size > 32768) 
		return true;
	if (!init) {
		for(i = 0; i <= FFT_BUFFER_SIZE / 2 + 1; i++) {
			double mult = (double)128 / ((FFT_BUFFER_SIZE * 16384) ^ 2);
			mult *= log(i + 1) / log(2);
			mult *= 3;
			fftmult[i] = mult;
		}
		right_fftstate = fft_init();
		left_fftstate = fft_init();
		if (!left_fftstate || !right_fftstate)
			alsaplayer_error("WARNING: could not do fft_init()");
		buf_size = SCOPE_BUFFER <= (FFT_BUFFER_SIZE * 2) ? SCOPE_BUFFER : FFT_BUFFER_SIZE;
		
		the_node = (AlsaNode *)arg;
		if (the_node) {
			latency = the_node->GetLatency();
		}
		if (latency < SCOPE_BUFFER)
			latency = SCOPE_BUFFER;
		//init_effects();
		
		init = 1;	
	}	

	scope_entry *se = root_scope;

	//buffer_effect(data, size);

	if (fill + size >= SCOPE_BUFFER) {
		left = SCOPE_BUFFER - fill;
		memcpy(buf + fill, data, left);

		// Do global FFT
		left_newset = left_actEq;
		right_newset = right_actEq;
		
		sound = (short *)buf;
		//sound = (short *)delay_feed(latency, SCOPE_BUFFER);
		
		for (i = 0; i < buf_size; i++) {
			*left_newset++ = (sound_sample)((int)(*sound));
			*right_newset++ = (sound_sample)((int)(*(sound+1)));
			sound +=2;
		}
		
		fft_perform(right_actEq, right_fftout, right_fftstate);
		fft_perform(left_actEq, left_fftout, left_fftstate);
			
		for (i = 0, left_pos = fft_buf, right_pos = fft_buf + 256; i < 256; i++) {
			left_pos[i] = (int)(sqrt(left_fftout[i + 1])) >> 8; //* fftmult[i]);
			right_pos[i] = (int)(sqrt(right_fftout[i + 1])) >> 8; //* fftmult[i]);
		}	
		while (se && se->sp && se->active) {
			if (se->sp->running()) {
				if (se->sp->set_data)
					se->sp->set_data((short *)buf, SCOPE_BUFFER >> 1);
				if (se->sp->set_fft)
					se->sp->set_fft((int *)fft_buf, 256, 2);
			}	
			if (se->next) 
				se = se->next;
			else 
				break;
		}
		// Copy the remainder
		fill = 0;
		memcpy(buf + fill, ((char *)data) + left, size - left);
	} else {
		memcpy(buf + fill, data, size);
		fill += size;
	}	
	return true;
}


void unregister_scopes(void)
{
	scope_entry *current = root_scope;
	
	pthread_mutex_lock(&sl_mutex);
	while (current && current->sp) {
		//printf("closing and unloading scope plugin %s\n", current->sp->name);
		current->active = 0;
		current->sp->shutdown();
		current = current->next;
	}
	pthread_mutex_unlock(&sl_mutex);
}	


bool register_scope(scope_plugin *plugin, bool run, void *arg)
{
	scope_entry *se;
	
	se = new scope_entry;
	se->next = (scope_entry *)NULL;
	se->sp = plugin;
	if (se->sp->version != SCOPE_PLUGIN_VERSION) {
		alsaplayer_error("Wrong version number on scope plugin (v%d, wanted v%d)",
			se->sp->version - 0x1000,
			SCOPE_PLUGIN_VERSION - 0x1000);
		delete se;
		return false;
	}		
	se->active = 0;

	// Init scope
	if (!se->sp->init(arg)) {
		delete se;
		return false;
	}	
	// Add scope to scope list
	// NOTE: WE CURRENTLY NEVER UNLOAD SCOPES
	pthread_mutex_lock(&sl_mutex);	
	if (root_scope == NULL) {
		//alsaplayer_error("registering first scope...");
		root_scope = se;
		root_scope->next = (scope_entry *)NULL;
		root_scope->active = 1;
	} else { // Not root scope, so insert it at the start
		se->next = root_scope->next;
		se->active = 1;
		root_scope->next = se;
	}
	pthread_mutex_unlock(&sl_mutex);
	if (run)
		se->sp->start();
	return true;
}


void scopes_list_click(GtkWidget *widget, gint row, gint /* column */,
	GdkEvent *bevent, gpointer /* data */)
{
	if (bevent && bevent->type == GDK_2BUTTON_PRESS) {
		scope_entry *se = (scope_entry *)
			gtk_clist_get_row_data(GTK_CLIST(widget), row);
		if (se && se->sp) {
#ifdef STUPID_FLUFF		
			if (se->active)
				se->sp->stop();
			else
				se->sp->start(NULL);
			se->active = 1 - se->active;
			if (se->active) {
				gtk_clist_set_pixmap(GTK_CLIST(widget),
					row, 0, active_pix, active_mask);
			} else {
				gtk_clist_set_text(GTK_CLIST(widget),
					row, 0, "");	
			}
#else
			se->sp->start();
#endif			
		}
	}
}


bool init_scopes(AlsaNode *node)
{
	// Init scope list
	pthread_mutex_init(&sl_mutex, (pthread_mutexattr_t *)NULL);
	// Load the scopes
	load_scope_addons();
	// Subscriber
	if (!node) {
		alsaplayer_error("Huh, no node?");
		return false;
	}	
	scopes = new AlsaSubscriber();
	scopes->Subscribe(node, POS_END);
	scopes->EnterStream(scope_feeder_func, node);
	return true;
}
