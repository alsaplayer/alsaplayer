/*  ScopesWindow.cpp
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org>
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
 *  $Id: ScopesWindow.cpp 954 2003-04-08 15:23:31Z adnans $
 *
*/ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#define N_(String) noop_gettext(String)
#else
#define _(String) (String)
#define N_(String) String
#endif

#include "ScopesWindow.h"
#include "CorePlayer.h"
#include "Effects.h"
#include "gtk_interface.h"
#include "alsaplayer_error.h"
#include "alsaplayer_fft.h"
#include "prefs.h"
#include <pthread.h>
#include <dlfcn.h>
#include <math.h>

static GtkWidget *scopes_window = (GtkWidget *)NULL;
#ifdef STUPID_FLUFF		
static GdkPixmap *active_pix = (GdkPixmap *)NULL;
static GdkBitmap *active_mask = (GdkBitmap *)NULL;
#endif
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

	size <<= 1; // To bytes again
	
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
		CorePlayer *the_coreplayer = (CorePlayer *)arg;
		if (the_coreplayer) {
			the_node = the_coreplayer->GetNode();
		}
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


void apUnregiserScopePlugins()
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


int apRegisterScopePlugin(scope_plugin *plugin)
{
	GtkWidget *scopes_list;
	GtkListStore *list;
	GtkTreeIter iter;
	
	scopes_list = (GtkWidget *) g_object_get_data(G_OBJECT(scopes_window), "scopes_list");
	list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(scopes_list)));
	
	scope_entry *se;
	if (!scopes_window) {
		printf("No scopes_window\n");
		return 0;
	}	
	
	se = new scope_entry;
	se->next = (scope_entry *)NULL;
	se->sp = plugin;
	if (se->sp->version != SCOPE_PLUGIN_VERSION) {
		alsaplayer_error("Wrong version number on scope plugin (v%d, wanted v%d)",
			se->sp->version - 0x1000,
			SCOPE_PLUGIN_VERSION - 0x1000);
		delete se;
		return -1;
	}		
	se->active = 0;

	// Add new scope to list
	gtk_list_store_append (list, &iter);
	gtk_list_store_set(list, &iter, 0, se, 1, (gchar *)se->sp->name, -1);

	// Init scope
	se->sp->init(NULL);
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


static void close_all_cb(GtkWidget *, gpointer data)
{
	GtkWidget *list = (GtkWidget *)data;

	if (list) {
		scope_entry *current = root_scope;

		while (current) {
			GDK_THREADS_LEAVE();
			if (current->sp) current->sp->stop();
			GDK_THREADS_ENTER();
			current = current->next;
		}
	}	
}


static void close_scope_cb(GtkWidget *, gpointer user_data)
{
	GtkWidget *list = (GtkWidget *)user_data;
	
	if (list) {

		GtkTreeIter iter;
		gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(user_data)), NULL, &iter);
		
		scope_entry *se = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(gtk_tree_view_get_model(GTK_TREE_VIEW(user_data))), &iter, 0, &se, -1);
		
		if (se && se->sp) {
			GDK_THREADS_LEAVE();
			se->sp->stop();
			GDK_THREADS_ENTER();
		}	
	}
}


static void open_scope_cb(GtkWidget *, gpointer user_data)
{
	GtkTreeIter iter;
	gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(user_data)), NULL, &iter);
	
	gchar *data = NULL;
//	gtk_tree_model_get(GTK_TREE_MODEL(gtk_tree_view_get_model(GTK_TREE_VIEW(user_data))), &iter, 0, &data, -1);
	gtk_tree_model_get(GTK_TREE_MODEL(gtk_tree_view_get_model(GTK_TREE_VIEW(user_data))), &iter, 1, &data, -1);
//	scope_entry *se = (scope_entry *) data;
	scope_entry *se = NULL;
	
	for(se = root_scope; se != NULL; se=se->next) {
		int size = (strlen(data) < strlen(se->sp->name))?strlen(data):strlen(se->sp->name);
		int res = strncmp(data, se->sp->name, size);
		if (!res)
			break;
	}
		
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
		g_free(data);
}


static void exclusive_open_cb(GtkWidget *widget, gpointer user_data)
{
	close_all_cb(NULL, user_data);
	open_scope_cb(NULL, user_data);
}


gboolean scopes_list_button_press(GtkWidget *widget, GdkEvent *bevent, gpointer)
{
	GtkWidget *menu_item;
	GtkWidget *the_menu;

	if (bevent->button.button == 3) { // Right mouse
		// Construct a popup
		the_menu = gtk_menu_new();
		menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
		gtk_menu_append(GTK_MENU(the_menu), menu_item);
		g_signal_connect(G_OBJECT(menu_item), "activate",
			G_CALLBACK(open_scope_cb), widget);

	
		menu_item = gtk_menu_item_new_with_label(_("Open exclusively"));
		gtk_menu_append(GTK_MENU(the_menu), menu_item);
		g_signal_connect(G_OBJECT(menu_item), "activate", 
			G_CALLBACK(exclusive_open_cb), widget);

		// Separator
		menu_item = gtk_separator_menu_item_new();
		gtk_menu_append(GTK_MENU(the_menu), menu_item);


		 menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_CLOSE, NULL);
		 gtk_menu_append(GTK_MENU(the_menu), menu_item);
		 g_signal_connect(G_OBJECT(menu_item), "activate",
		 	G_CALLBACK(close_scope_cb), widget);

		 menu_item = gtk_menu_item_new_with_label(_("Close all"));
		 gtk_menu_append(GTK_MENU(the_menu), menu_item);
		 g_signal_connect(G_OBJECT(menu_item), "activate",
		 	G_CALLBACK(close_all_cb), widget);
		
		gtk_widget_show_all(the_menu);
		
		gtk_menu_popup(GTK_MENU(the_menu), NULL, NULL, NULL, NULL,
			bevent->button.button, bevent->button.time);
	} else if ((bevent->button.button == 1) && (bevent->type == GDK_2BUTTON_PRESS))
		open_scope_cb(NULL, widget);
	
	//alsaplayer_error("Row = %d, Col = %d", row, col);
	
	return FALSE;	
}

void destroy_scopes_window()
{
	if (!scopes_window)
		return;
	prefs_set_bool(ap_prefs, "gtk2_interface", "scopeswindow_active", GTK_WIDGET_VISIBLE(scopes_window));
}

gboolean
scopes_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if (GTK_WIDGET_VISIBLE(widget))
		gtk_widget_hide_all(widget);
	
	return TRUE;	
}

void
scopes_window_response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
	if (arg1 == GTK_RESPONSE_CLOSE) {
		scopes_delete_event(GTK_WIDGET(dialog), NULL, NULL);
	}
}

GtkWidget*
create_scopes_window(void)
{
	GtkWidget *scopes_window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *scrolledwindow;
	GtkWidget *scopes_list;
	GtkListStore *scopes_model;
	
	scopes_window = gtk_dialog_new_with_buttons(_("Scopes"), NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
												GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	
	gtk_window_set_default_size (GTK_WINDOW(scopes_window), 200, 300);
	 
	vbox = GTK_DIALOG(scopes_window)->vbox;
 
	label = gtk_label_new(_("Double click to activate"));
	gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 3);
	
	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start (GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);
	
	scopes_model = gtk_list_store_new(2, G_TYPE_POINTER, G_TYPE_STRING);
 	scopes_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(scopes_model));
 	g_object_set_data(G_OBJECT(scopes_window), "scopes_list", scopes_list);
 	gtk_container_add (GTK_CONTAINER(scrolledwindow), scopes_list);
	g_object_unref(scopes_model);

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Scope name"), renderer, "text", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (scopes_list), column);
			
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(scopes_list)), GTK_SELECTION_SINGLE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(scopes_list), FALSE);
	
	g_signal_connect(G_OBJECT(scopes_window), "delete_event", G_CALLBACK(scopes_delete_event), NULL);
	g_signal_connect(G_OBJECT(scopes_window), "response", G_CALLBACK(scopes_window_response), NULL);
	g_signal_connect(G_OBJECT(scopes_list), "button_press_event", G_CALLBACK(scopes_list_button_press), NULL);
	
	return scopes_window;
}

GtkWidget *init_scopes_window(GtkWidget *main_window)
{
	scopes_window = create_scopes_window();

	// Init scope list
	pthread_mutex_init(&sl_mutex, (pthread_mutexattr_t *)NULL);

	if (prefs_get_bool(ap_prefs, "gtk2_interface", "scopeswindow_active", 0)) {
		gtk_widget_show_all(scopes_window);
	}	

	return scopes_window;
}
