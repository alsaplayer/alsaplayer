/*  ScopesWindow.cpp
*  Copyright (C) 1999 Andy Lo A Foe <andy@alsa-project.org>
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
#include "ScopesWindow.h"
#include "CorePlayer.h"
#include "Effects.h"
#include "support.h"
#include "gladesrc.h"
#include "gtk_interface.h"
#include "pixmaps/note.xpm"
#include <pthread.h>
#include <dlfcn.h>

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
			printf("dlclosing %s\n", current->sp->name);
			dlclose(current->sp->handle);
		}	
		current = current->next;
	}	
}

void scope_entry_destroy_notify(gpointer data)
{
	//scope_entry *se = (scope_entry *)data;
	//if (se) {
	//	printf("should destroy \"%s\"\n", se->sp->name);
	//	delete se->sp; // HACK!!!!!!!!
	//	delete se;
}

bool  scope_feeder_func(void *arg, void *data, int size) 
{
	scope_entry *se = root_scope;
	
	CorePlayer *p = (CorePlayer *)arg;
	unsigned int latency = p->GetLatency();
	char *point;
	buffer_effect(data, size);
	latency -= (latency % 4);
	point = delay_feed(latency, size);
	if (pthread_mutex_trylock(&sl_mutex) != 0) {
		return true;	// List is being manipulated
	}	
#if 1
	while (se && se->sp && se->active) {
		if (se->sp->running())
			se->sp->set_data((short *)point, size >> 1);
		//printf("feeding %s\n", se->sp->name);
		if (se->next) 
			se = se->next;
		else 
			break;
	}
#endif
	pthread_mutex_unlock(&sl_mutex);
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
		//if (current->sp->handle) {
		//	fprintf(stdout, "Unloading Scope plugin: %s (%x)\n",
		//			current->sp->name, current->sp->handle);
		//	handle = current->sp->handle;	
			//delete current->sp;
			//current->sp = NULL;
			//dlclose(handle);
		//}
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
	//se->sp = new scope_plugin;
	//memcpy(se->sp, plugin, sizeof(scope_plugin));
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
	//gtk_clist_set_shift(GTK_CLIST(list), index, 1, 5, 2);

	// Init scope
	se->sp->init();
	//se->sp->start(NULL);
	// Add scope to scope list
	// NOTE: WE CURRENTLY NEVER UNLOAD SCOPES
	pthread_mutex_lock(&sl_mutex);	
	if (root_scope == NULL) {
		//printf("registering first scope...\n");
		root_scope = se;
		root_scope->next = (scope_entry *)NULL;
		root_scope->active = 1;
		//root_scope.sp = se->sp;
		//root_scope.next = (scope_entry *)NULL;
		//root_scope.active = 1;
	} else { // Not root scope, so insert it at the start
		//printf("registering other scope...\n");
		se->next = root_scope->next;
		se->active = 1;
		root_scope->next = se;
		//scope_entry *tmp = (scope_entry *)NULL;
		//tmp = root_scope.next;
		//se->next = tmp;
		//se->active = 1;
		//root_scope.next = se;
	}
	pthread_mutex_unlock(&sl_mutex);
	fprintf(stdout, "Loading Scope plugin: %s (%x)\n", se->sp->name, se->sp->handle);
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
    //gtk_clist_set_row_height(GTK_CLIST(list), 26);
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
	//root_scope.next = (scope_entry *)NULL;
	//root_scope.active = 0;
	//root_scope.sp = (scope_plugin *)NULL;
	pthread_mutex_init(&sl_mutex, (pthread_mutexattr_t *)NULL);
	
	return scopes_window;
}
