/*  ScopesWindow.cpp
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org>
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
#include "ScopesWindow.h"
#include "CorePlayer.h"
#include "Effects.h"
#include "support.h"
#include "gladesrc.h"
#include "gtk_interface.h"
#include "pixmaps/note.xpm"
#include "error.h"
#include "fft.h"
#include <pthread.h>
#include <dlfcn.h>
#include <math.h>

extern int global_scopes_show;
static GtkWidget *scopes_window = (GtkWidget *)NULL;
static GdkPixmap *active_pix = (GdkPixmap *)NULL;
static GdkBitmap *active_mask = (GdkBitmap *)NULL;
static scope_entry *root_scope = NULL;
static pthread_mutex_t sl_mutex;

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

void scope_entry_destroy_notify(gpointer data)
{
}

#define SCOPE_BUFFER	4096

bool  scope_feeder_func(void *arg, void *data, int size) 
{
	static char buf[16384];
	static int fft_buf[512];
	static int fill = 0;
	static int left = 0;
	static int warn = 0;
	static int init = 0;
	static int buf_size = 0;
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
		init = 1;	
	}	

	scope_entry *se = root_scope;

	if (fill + size >= SCOPE_BUFFER) {
		left = SCOPE_BUFFER - fill;
		memcpy(buf + fill, data, left);

		if (pthread_mutex_trylock(&sl_mutex) != 0) {
			return true;	// List is being manipulated
		}
#if 1	
		// Do global FFT
		left_newset = left_actEq;
		right_newset = right_actEq;
		
		sound = (short *)buf;
			
		for (i = 0; i < buf_size; i++) {
			*left_newset++ = (sound_sample)((int)(*sound));
			*right_newset++ = (sound_sample)((int)(*(sound+1)));
			sound +=2;
		}
		
		fft_perform(right_actEq, right_fftout, right_fftstate);
		fft_perform(left_actEq, left_fftout, left_fftstate);
			
		for (i = 0, left_pos = fft_buf, right_pos = fft_buf + 256; i < 256; i++) {
			left_pos[i] = (int)(sqrt(left_fftout[i]) * fftmult[i]);
			right_pos[i] = (int)(sqrt(right_fftout[i]) * fftmult[i]);
		}	
#endif
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
		pthread_mutex_unlock(&sl_mutex);
	} else {
		memcpy(buf + fill, data, size);
		fill += size;
	}	
	return true;
}


void apUnregiserScopePlugins()
{
	scope_entry *current = root_scope;
	GtkWidget *list;
	void *handle;
	
	pthread_mutex_lock(&sl_mutex);
	while (current && current->sp) {
		//printf("closing and unloading scope plugin %s\n", current->sp->name);
		current->active = 0;
		current->sp->stop();
		current->sp->close();
		current = current->next;
	}
	pthread_mutex_unlock(&sl_mutex);
}	


int apRegisterScopePlugin(scope_plugin *plugin)
{
	GtkWidget *list;
	char *list_item[2];
	scope_entry *se;
	if (!scopes_window) {
		printf("No scopes_window\n");
		return 0;
	}	
	list = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(scopes_window),
			"list");
	se = new scope_entry;
	se->next = (scope_entry *)NULL;
	se->sp = plugin;
	if (se->sp->version != SCOPE_PLUGIN_VERSION) {
			fprintf(stderr, "Wrong version number on plugin v%d, wanted v%d\n",
				se->sp->version - 0x1000, SCOPE_PLUGIN_VERSION - 0x1000);
			delete se->sp;
			delete se;
			return 0;
	}		
	se->active = 0;

	// Add new scope to GtkClist
	list_item[0] = g_strdup(" ");
	list_item[1] = g_strdup(se->sp->name);
	int index = gtk_clist_append(GTK_CLIST(list), list_item);
	gtk_clist_set_row_data_full(GTK_CLIST(list), index, se, scope_entry_destroy_notify);

	// Init scope
	se->sp->init();
	// Add scope to scope list
	// NOTE: WE CURRENTLY NEVER UNLOAD SCOPES
	pthread_mutex_lock(&sl_mutex);	
	if (root_scope == NULL) {
		//printf("registering first scope...\n");
		root_scope = se;
		root_scope->next = (scope_entry *)NULL;
		root_scope->active = 1;
	} else { // Not root scope, so insert it at the start
		se->next = root_scope->next;
		se->active = 1;
		root_scope->next = se;
	}
	pthread_mutex_unlock(&sl_mutex);
	//fprintf(stdout, "Loading Scope plugin: %s (%x)\n", se->sp->name, se->sp->handle);
	return 1;
}

void scopes_list_click(GtkWidget *widget, gint row, gint column,
	GdkEvent *bevent, gpointer data)
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
			se->sp->start(NULL);
#endif			
		}
	}
}


void scopes_window_ok_cb(GtkWidget *button_widget, gpointer data)
{
	gint x, y;
	GtkWidget *widget = (GtkWidget *)data;
        
        gdk_window_get_origin(widget->window, &x, &y);
        if (windows_x_offset >= 0) {
                x -= windows_x_offset;
                y -= windows_y_offset;
        }       
        gtk_widget_hide(widget);
        gtk_widget_set_uposition(widget, x, y);
        global_scopes_show = 0;

}

gboolean scopes_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
        gint x, y;

        gdk_window_get_origin(widget->window, &x, &y);
        if (windows_x_offset >= 0) {
                x -= windows_x_offset;
                y -= windows_y_offset;
        }	
        gtk_widget_hide(widget);
        gtk_widget_set_uposition(widget, x, y);
        global_scopes_show = 0;

		return TRUE;
}


GtkWidget *init_scopes_window()
{
	GtkWidget *working;
	GtkStyle *style;

	scopes_window = create_scopes_window();
	gtk_widget_realize(scopes_window);
	GtkWidget *list = get_widget(scopes_window, "scopes_list");

	style = gtk_widget_get_style(list);	
	active_pix = gdk_pixmap_create_from_xpm_d(scopes_window->window, &active_mask,
		&style->bg[GTK_STATE_NORMAL], note_xpm);
	

	gtk_object_set_data(GTK_OBJECT(scopes_window), "list", list);
	gtk_clist_set_column_width(GTK_CLIST(list), 0, 16);
	gtk_signal_connect(GTK_OBJECT(list), "select_row",
		GTK_SIGNAL_FUNC(scopes_list_click), NULL);
	working = get_widget(scopes_window, "ok_button");
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
		GTK_SIGNAL_FUNC(scopes_window_ok_cb), scopes_window);

	// Close/delete signals
	gtk_signal_connect(GTK_OBJECT(scopes_window), "destroy",
                GTK_SIGNAL_FUNC(scopes_window_delete_event), NULL);
	gtk_signal_connect(GTK_OBJECT(scopes_window), "delete_event",
                GTK_SIGNAL_FUNC(scopes_window_delete_event), NULL);

	// Init scope list
	pthread_mutex_init(&sl_mutex, (pthread_mutexattr_t *)NULL);
	
	return scopes_window;
}
