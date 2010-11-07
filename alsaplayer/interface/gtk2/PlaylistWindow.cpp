/*  PlayListWindow.cpp - playlist window 'n stuff
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkdnd.h>
#include <glib.h>
#include <assert.h>

#include <vector>
#include <algorithm>

#include "AlsaPlayer.h"
#include "Playlist.h"
#include "PlaylistWindow.h"
#include "gtk_interface.h"
#include "utilities.h"
#include "prefs.h"
#include "alsaplayer_error.h"
#include "control.h"

// Drag & Drop stuff
#define TARGET_URI_LIST 0x0001

static GtkTargetEntry drag_types[] = {
	        {strdup("text/uri-list"), 0, TARGET_URI_LIST}
};
static int n_drag_types = sizeof(drag_types)/sizeof(drag_types[0]);

static void
dialog_cancel_response(GtkWidget *dialog, gpointer data)
{
// do we really need placing window ?
//	gint x,y;

//	gdk_window_get_root_origin(play_dialog->window, &x, &y);
	gtk_widget_hide(dialog);
//	gtk_widget_set_uposition(play_dialog, x, y);
}

static void
new_list_item(const PlayItem *item, gchar **list_item)
{
	gchar *dirname;
	gchar *filename;
	gchar *new_path = (gchar *)g_strdup(item->filename.c_str());
	char pt[1024];

	list_item[0] = NULL;

	if (item->playtime >= 0) {
		sprintf(pt, "%02d:%02d", (item->playtime > 0) ? item->playtime / 60 : 0,
			(item->playtime > 0) ? item->playtime % 60 : 0);
	} else {
		sprintf(pt, "00:00");
	}
	list_item[3] = (gchar *)g_strdup(pt);
	// Strip directory names
	dirname = strrchr(new_path, '/');	// do not free
	if (dirname) {
			dirname++;
			filename = (gchar *)g_strdup(dirname);
	} else
			filename = (gchar *)g_strdup(new_path);
	if (item->title.size())
		list_item[2] = g_strdup(item->title.c_str());
	else
		list_item[2] = g_strdup(filename);

	if (item->artist.size())
		list_item[1] = g_strdup((gchar *)item->artist.c_str());
	else
		list_item[1] = g_strdup(_("Unknown"));
		
	g_free(new_path);
	g_free(filename);
}

static void
load_dialog_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	if ((response == GTK_RESPONSE_ACCEPT) && playlist_window)
		playlist_window->LoadPlaylist();

		dialog_cancel_response(GTK_WIDGET(dialog), NULL);
}

static GtkWidget*
create_playlist_load(GtkWindow *main_window, PlaylistWindow *playlist_window)
{
	GtkWidget *filechooser;
	
	filechooser = gtk_file_chooser_dialog_new("Choose playlist" , main_window, GTK_FILE_CHOOSER_ACTION_OPEN, 
  																GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     											GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      											NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), FALSE);
	
	const char *path = prefs_get_string(ap_prefs, "gtk2_interface", "default_playlist_load_path", ".");
	if (g_path_is_absolute (path))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser), path);
	
	g_signal_connect(G_OBJECT(filechooser), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(filechooser), "response", G_CALLBACK(load_dialog_cb), (gpointer) playlist_window);
	
	return filechooser;
}

static void
save_dialog_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	if ((response == GTK_RESPONSE_ACCEPT) && playlist_window)
		playlist_window->SavePlaylist();
		
	dialog_cancel_response(GTK_WIDGET(dialog), NULL);
}

static GtkWidget*
create_playlist_save(GtkWindow *main_window, PlaylistWindow *playlist_window)
{
	GtkWidget *filechooser;
	
	filechooser = gtk_file_chooser_dialog_new("Save playlist" , main_window, GTK_FILE_CHOOSER_ACTION_SAVE, 
  																GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     											GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      											NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), FALSE);
	
	const char *path = prefs_get_string(ap_prefs, "gtk2_interface", "default_playlist_save_path", ".");
	if (g_path_is_absolute (path))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser), path);
	
	g_signal_connect(G_OBJECT(filechooser), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(filechooser), "response", G_CALLBACK(save_dialog_cb), (gpointer) playlist_window);
	
	return filechooser;
}

static void
play_file_ok(GtkWidget *play_dialog, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data; 
	Playlist *playlist = playlist_window->GetPlaylist();
	CorePlayer *p = playlist->GetCorePlayer();

	if (p) {

		GSList *file_list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(play_dialog));
	
		std::vector<std::string> paths;
		char *path;
		
		if (file_list) {
			gchar *dir = g_path_get_dirname((gchar *) file_list->data);

			prefs_set_string(ap_prefs, "gtk2_interface", "default_playlist_add_path", dir);		
			g_free(dir);
		}
		// Get the selections
		if (!file_list) {
			path = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(play_dialog));
			if (path) {
				paths.push_back(path);
				g_free(path);
			}
		}
		while (file_list) {
			path = (char *) file_list->data;
			if (path) {
				paths.push_back(path);
			}
			file_list = file_list->next;
		}

		// Sort them (they're sometimes returned in a slightly odd order)
		// don't sort them put them as they come
		//		sort(paths.begin(), paths.end());

		// Add selections to the queue, and start playing them
		GDK_THREADS_LEAVE();
		if (playlist_window->play_on_add) {
			playlist->AddAndPlay(paths);
			playlist->UnPause();
		}
		else {
			playlist->Insert(paths, playlist->Length());
			playlist->Pause();
		}
		GDK_THREADS_ENTER();

		gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER(play_dialog));
		g_slist_free(file_list);	
	}
	// Save path
}

static void
play_dialog_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	GtkWidget *cb;
	cb = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "check_button"));

	if (response == GTK_RESPONSE_ACCEPT) {
		play_file_ok(GTK_WIDGET(dialog), (gpointer) user_data);
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb)))
			return;
	}	
	
	dialog_cancel_response(GTK_WIDGET(dialog), NULL);
}	

static GtkWidget*
create_filechooser(GtkWindow *main_window, PlaylistWindow *playlist_window)
{
	GtkWidget *filechooser;
	GtkWidget *checkbutton;
	
	filechooser = gtk_file_chooser_dialog_new(_("Choose file or URL"), main_window, GTK_FILE_CHOOSER_ACTION_OPEN, 
  																GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     											GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      											NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), TRUE);
	
	const char *path = prefs_get_string(ap_prefs, "gtk2_interface", "default_playlist_add_path", ".");
	if (g_path_is_absolute (path))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser), path);

	checkbutton = gtk_check_button_new_with_label(_("Do not close the window after adding files"));
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(filechooser)->vbox), checkbutton, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(filechooser), "check_button", checkbutton);

	g_signal_connect(G_OBJECT(filechooser), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(filechooser), "response", G_CALLBACK(play_dialog_cb), (gpointer) playlist_window);
	
	return filechooser;
}

static int
get_path_number(GtkTreePath *data)
{
	int number;
	gchar *path = gtk_tree_path_to_string(data);
	gtk_tree_path_free(data);
	number = atoi(path);
	g_free(path);
	
	return number;
}

void
playlist_play_current(GtkWidget *tree, PlaylistWindow *playlist_window)
{
		int selected;
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
		
		if (gtk_tree_selection_count_selected_rows(selection) != 1)
			return;
			
		GList* data = gtk_tree_selection_get_selected_rows(selection, NULL);
		
		selected = get_path_number((GtkTreePath *)data->data);
		
		g_list_free(data);
				
		playlist_window->Play(selected + 1);
}

static gboolean
list_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	if (playlist_window && (event->type == GDK_2BUTTON_PRESS)) {
		playlist_play_current(widget, playlist_window);
	}

	gtk_widget_grab_focus(widget);
	
	return FALSE;
}

static void
shuffle_cb(GtkButton *button, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	if (playlist_window) {
		GDK_THREADS_LEAVE();
		playlist_window->GetPlaylist()->Shuffle();
		GDK_THREADS_ENTER();
	}	
}

static void
dialog_popup(GtkButton *button, gpointer user_data)
{
	gtk_widget_show_all(GTK_WIDGET(user_data));
} 

static void
clear_cb(GtkButton *button, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	if (playlist_window) {
		stop_cb(NULL, (gpointer) playlist_window->GetPlaylist()); 
		GDK_THREADS_LEAVE();
		playlist_window->GetPlaylist()->Clear();
		GDK_THREADS_ENTER();
	}	
}

void
playlist_remove(GtkWidget *, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	Playlist *playlist = NULL;
	GtkWidget *list = NULL;
	GList *next, *start;

	if (playlist_window) {
			playlist = playlist_window->GetPlaylist();
	} else
		return;

	list = playlist_window->GetList();	

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
	if (gtk_tree_selection_count_selected_rows(selection) < 1)
			return;		
	
	if (playlist && list) {
		GList* data = gtk_tree_selection_get_selected_rows(selection, NULL);
		int selected = -1;
	
		next = start = data;
		
		while (next->next != NULL) {
			next = next->next;
		}	
		while (next != start->prev) {
		selected = get_path_number((GtkTreePath *)next->data);
			GDK_THREADS_LEAVE();
			if (playlist->GetCurrent() == selected+1) {
				if (playlist->Length() == 1)
					playlist->Stop();
				else if (playlist->Length() == selected+1)
					playlist->Prev();
				else
					playlist->Next();
			}
			playlist->Remove(selected+1, selected+1);
			GDK_THREADS_ENTER();
			next = next->prev;	
		}
		g_list_free(data);
	}
}

static void
dnd_received(GtkWidget *widget,
		GdkDragContext   *drag_context,
		gint              x,
		gint              y,
		GtkSelectionData *data,
		guint             info,
		guint             time,
		gpointer          user_data)
{
	char *uri = NULL;
	char *filename = NULL;
	char *p, *s, *res;

	if (!data) {
		return;
	}
	GtkTreePath *path = NULL;
	gint pos = -1;
	
	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), x, y, &path, NULL, NULL, NULL);
	if (path) {
		gchar *position = gtk_tree_path_to_string(path);
		pos = atoi(position);
		g_free(position);
	} 
	
	switch(info) {
		case TARGET_URI_LIST:
			uri = (char *)malloc(strlen((const char *)data->data)+1);
			strcpy(uri, (const char *)data->data);
			filename = uri;
			while (filename) {
				if ((p=s=strstr(filename, "\r\n"))) {
					*s = '\0';
					p = s+2;
				}
				if (strlen(filename)) {
					if ((*filename == 'h') && (*(filename+1) == 't') && (*(filename+2) == 't') && (*(filename+3) == 'p'))
						res = g_strdup(filename);
					else
						res = g_filename_from_uri(filename, NULL ,NULL);
					if (res) {
						GDK_THREADS_LEAVE();
						if (is_playlist(res)) {
							ap_add_playlist(global_session_id, res);
						} else {
							if (pos < 0)
								ap_add_path(global_session_id, res);
							else
								ap_insert(global_session_id, res, pos);
						}	
						GDK_THREADS_ENTER();
						g_free(res);
					}	
				}
				filename = p;
			}
			free(uri);
			break;
		default:
			ap_message_error(gtk_widget_get_toplevel(widget), _("Unknown drop!"));
		break;
	}
	
	gtk_tree_path_free(path);
	
	return;
}

static void
dnd_get(GtkWidget *widget,
		GdkDragContext *drag_context,
		GtkSelectionData *data,
		guint info,
		guint time,
		gpointer user_data)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		
	GList *file_list = gtk_tree_selection_get_selected_rows(selection, NULL);
	
	if (!file_list)
		return;
		
	GList *next, *start;
	gint selected = 0;
	gchar path[AP_FILE_PATH_MAX];
	gint i = 0;
		
	next = start = file_list;
		
	while (next->next != NULL) {
		next = next->next;
		i++;
	}
		
	gchar *uris[i+2];
	i = 0;
	
	while (start != NULL) {
		selected = get_path_number((GtkTreePath *)start->data);
		ap_get_file_path_for_track(global_session_id, path, selected+1);
		if (is_uri(path))
			uris[i] = g_strdup(path);
		else
			uris[i] = g_filename_to_uri(path, NULL, NULL);
		i++;
		start=start->next;
	}
	uris[i] = NULL;
	
	g_list_free(file_list);
		 
	if (!gtk_selection_data_set_uris(data, uris))
		ap_message_error(gtk_widget_get_toplevel(widget), _("Cannot set uris"));

	for (--i; i >= 0; i--)
		g_free(uris[i]);
} 

static void dnd_delete(GtkWidget *widget, GdkDragContext *drag_context, gpointer user_data)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		
	GList *file_list = gtk_tree_selection_get_selected_rows(selection, NULL);
	
	gint selected = 0;
	
	if (!file_list)
		return;
	
	GList *next, *start; 
	
	next = start = file_list;
		
	while (next->next != NULL)
		next = next->next;
		
	while(next != start->prev) {
		selected = get_path_number((GtkTreePath *)next->data);
		GDK_THREADS_LEAVE();
		ap_remove(global_session_id, selected+1);
		GDK_THREADS_ENTER();
		next = next->prev;
	}
	
	g_list_free(file_list);	
}

static GtkWidget*
create_playlist_window (PlaylistWindow *playlist_window)
{
	GtkWidget *main_window;
	GtkWidget *main_box;
	GtkWidget *scrolledwindow;
	GtkListStore *playlist_model;
	GtkWidget *list;
	GtkWidget *button_box;
	GtkWidget *add_button;
	GtkWidget *del_button;
	GtkWidget *shuffle_button;
	GtkWidget *pl_button_box;
	GtkWidget *load_button;
	GtkWidget *save_button;
	GtkWidget *clear_button;
	GtkWidget *add_file;
	GtkWidget *load_list;
	GtkWidget *save_list;
	GtkTooltips *tooltips;
		
	tooltips = gtk_tooltips_new();
	
	main_window = gtk_frame_new(NULL);

	main_box = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER (main_window), main_box);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (main_box), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	playlist_model = gtk_list_store_new(4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
 
	list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(playlist_model));
	g_object_set_data(G_OBJECT(main_window), "list", list);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(list), TRUE);
	g_object_unref(playlist_model);
  
	gtk_container_add (GTK_CONTAINER (scrolledwindow), list);

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("playing", renderer, "pixbuf", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("artist", renderer, "text", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
		
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("title", renderer, "text", 2, NULL);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("time", renderer, "text", 3, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
	
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(list)), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);

	button_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_box), button_box, FALSE, FALSE, 0);

	add_button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_box_pack_start (GTK_BOX (button_box), add_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), add_button, _("Add a song into the playlist"), NULL);

	del_button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_box_pack_start (GTK_BOX (button_box), del_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), del_button, _("Remove the selected song from the playlist"), NULL);

	shuffle_button = gtk_button_new_with_label (_("Shuffle"));
	gtk_box_pack_start (GTK_BOX (button_box), shuffle_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), shuffle_button, _("Randomize the playlist"), NULL);

	pl_button_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX (button_box), pl_button_box, FALSE, FALSE, 0);

	load_button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
	gtk_box_pack_start (GTK_BOX (pl_button_box), load_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), load_button, _("Open a playlist"), NULL);

	save_button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
	gtk_box_pack_start (GTK_BOX (pl_button_box), save_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), save_button, _("Save the playlist"), NULL);

	clear_button = gtk_button_new_from_stock (GTK_STOCK_CLEAR);
	gtk_box_pack_start (GTK_BOX (pl_button_box), clear_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), clear_button, _("Remove the current playlist"), NULL);

	gtk_drag_dest_set(list, GTK_DEST_DEFAULT_ALL,
						drag_types, n_drag_types, (GdkDragAction) (GDK_ACTION_MOVE | GDK_ACTION_COPY));
	gtk_drag_source_set(list, GDK_BUTTON1_MASK,
						drag_types, n_drag_types, (GdkDragAction) (GDK_ACTION_MOVE | GDK_ACTION_COPY));

	add_file = create_filechooser(GTK_WINDOW(NULL), playlist_window);
	g_object_set_data(G_OBJECT(main_window), "add_file", add_file);
	load_list = create_playlist_load(GTK_WINDOW(NULL), playlist_window);
	g_object_set_data(G_OBJECT(main_window), "load_list", load_list);
	save_list = create_playlist_save(GTK_WINDOW(NULL), playlist_window);
	g_object_set_data(G_OBJECT(main_window), "save_list", save_list);
	
	g_signal_connect(G_OBJECT(list), "drag_data_received", G_CALLBACK(dnd_received), NULL);
	g_signal_connect(G_OBJECT(list), "drag_data_get", G_CALLBACK(dnd_get), NULL);
	g_signal_connect(G_OBJECT(list), "drag_data_delete", G_CALLBACK(dnd_delete), NULL);
	
	g_signal_connect(G_OBJECT(list), "button_press_event", G_CALLBACK(list_button_press_event), (gpointer)playlist_window);
	g_signal_connect(G_OBJECT(shuffle_button), "clicked", G_CALLBACK(shuffle_cb), (gpointer)playlist_window);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(dialog_popup), (gpointer)add_file);
	g_signal_connect(G_OBJECT(clear_button), "clicked", G_CALLBACK(clear_cb), (gpointer)playlist_window);
	g_signal_connect(G_OBJECT(del_button), "clicked", G_CALLBACK(playlist_remove), (gpointer)playlist_window);
	g_signal_connect(G_OBJECT(save_button), "clicked", G_CALLBACK(dialog_popup), (gpointer)save_list);
	g_signal_connect(G_OBJECT(load_button), "clicked", G_CALLBACK(dialog_popup), (gpointer)load_list);
	
	gtk_widget_grab_focus(GTK_WIDGET(list));
	
	return main_window;
}

PlaylistWindow::PlaylistWindow(Playlist *pl) {
	
	this->playlist = pl;
	
	this->window = create_playlist_window(this);
	this->list = (GtkWidget *)g_object_get_data(G_OBJECT(window), "list");
	
	this->current_entry = 1;
	this->width = window->allocation.width;
	this->height = window->allocation.height;
	this->play_on_add = prefs_get_bool(ap_prefs, "gtk2_interface", "play_on_add", FALSE);
	
	
	pthread_mutex_init(&playlist_list_mutex, NULL);

	memset(&pli, 0, sizeof(playlist_interface));
	pli.cbsetcurrent = CbSetCurrent;
	pli.cbupdated = CbUpdated;
	pli.cbinsert = CbInsert;
	pli.cbremove = CbRemove;
	pli.cbclear = CbClear;
	pli.data = this;
	GDK_THREADS_LEAVE();
	playlist->Register(&pli);
	GDK_THREADS_ENTER();
}

PlaylistWindow::~PlaylistWindow()
{
	int hidden = IsHidden()?0:1;
	prefs_set_bool(ap_prefs, "gtk2_interface", "playlist_active", hidden);
	prefs_set_int(ap_prefs, "gtk2_interface", "playlist_height", this->height);
	
	this->Hide();
	this->Clear();
	
	this->playlist->UnRegister(&pli);
}

void PlaylistWindow::LoadPlaylist()
{
	enum plist_result loaderr;

	GtkWidget *widget = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "load_list"));
	gchar *current = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
	
		if (current) {
			gchar *dir = g_path_get_dirname(current);

			prefs_set_string(ap_prefs, "gtk2_interface", "default_playlist_load_path", dir);
			g_free(dir);
		}

		if (!current) {
			current = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(widget));
		}
		
	GDK_THREADS_LEAVE();
	loaderr = playlist->Load(current, playlist->Length(), false);
	GDK_THREADS_ENTER();
	
	if(loaderr == E_PL_DUBIOUS) {
		if (ap_message_question(gtk_widget_get_toplevel(this->window), _("It doesn't look like playlist !\nAre you sure you want to proceed ?"))) {
			GDK_THREADS_LEAVE();
			loaderr = playlist->Load(current, playlist->Length(), true);
			GDK_THREADS_ENTER();
		}
	}
	if(loaderr) {
		// FIXME - pass playlist_window to this routine
	}
	g_free(current);
}

void PlaylistWindow::SavePlaylist()
{
	GtkWidget *widget = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "save_list"));
	
	gchar *current = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));

		if (current) {
			gchar *dir = g_path_get_dirname(current);

			prefs_set_string(ap_prefs, "gtk2_interface", "default_playlist_save_path", dir);
			g_free(dir);
			
		}
		

		
	enum plist_result saveerr;
	saveerr = playlist->Save(current, PL_FORMAT_M3U);
	if(saveerr) {
		// FIXME - pass playlist_window to this routine
	} else {
		// FIXME - pass playlist_window to this routine
	}
	g_free(current);
}

void PlaylistWindow::Clear()
{
	gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list))));
}

#include "pixmaps/current_play.xpm"
#include "pixmaps/current_stop.xpm"
static GdkPixbuf *current_play_pix = NULL;
static GdkPixbuf *current_stop_pix = NULL;

void PlaylistWindow::CbSetCurrent(void *data, unsigned current)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *)data;
	if (current == 0)
	    return;

	GDK_THREADS_ENTER();

	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_window->GetList())));
	GtkTreeIter iter;
fprintf(stderr ,"CBSetcurrent: %u\n", current);
	if (!current_play_pix) {	
		current_play_pix = gdk_pixbuf_new_from_xpm_data((const char **)current_play_xpm);
		current_stop_pix = gdk_pixbuf_new_from_xpm_data((const char **)current_stop_xpm);
		
	} else {
		if (playlist_window->current_entry <= playlist_window->GetPlaylist()->Length()) {
			gchar *current_string = g_strdup_printf("%d", playlist_window->current_entry - 1);
			gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, current_string);
			gtk_list_store_set (list, &iter, 0, NULL, -1);
			g_free(current_string);
		}
	}	
	playlist_window->current_entry = current;

	gchar *current_string = g_strdup_printf("%d", playlist_window->current_entry - 1);
	gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, current_string);
	
	if (playlist_window->GetPlaylist()->GetCorePlayer()->IsPlaying())
		gtk_list_store_set (list, &iter, 0, current_play_pix, -1);
	else
		gtk_list_store_set (list, &iter, 0, current_stop_pix, -1);
		
	g_free(current_string);
	GDK_THREADS_LEAVE();
}


void PlaylistWindow::CbUpdated(void *data,PlayItem & item, unsigned position) {
	PlaylistWindow *playlist_window = (PlaylistWindow *)data;

	pthread_mutex_lock(&playlist_window->playlist_list_mutex);

	GDK_THREADS_ENTER();

	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_window->GetList())));
	GtkTreeIter iter;

	gchar *position_string = g_strdup_printf("%d", position);
	gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, position_string);


	gchar *list_item[4];
	new_list_item(&item, list_item);

	gtk_list_store_set (list, &iter, 0, NULL, 1, list_item[1], 2, list_item[2], 3, list_item[3], -1);
			
	g_free(list_item[0]);
	g_free(list_item[1]);
	g_free(list_item[2]);
	g_free(list_item[3]);	

	g_free(position_string);
	
	GDK_THREADS_LEAVE();

	pthread_mutex_unlock(&playlist_window->playlist_list_mutex);
}

void PlaylistWindow::CbInsert(void *data,std::vector<PlayItem> & items, unsigned position) {
	PlaylistWindow *playlist_window = (PlaylistWindow *)data;
	
	pthread_mutex_lock(&playlist_window->playlist_list_mutex);

	GDK_THREADS_ENTER();

	std::vector<PlayItem> item_copy = items;

	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_window->GetList())));
	GtkTreeIter iter;

	if(items.size() > 0) {
		std::vector<PlayItem>::const_iterator item;
		for(item = items.begin(); item != items.end(); item++, position++) {

			gchar *list_item[4];
			new_list_item(&(*item), list_item);

			gtk_list_store_insert (list, &iter, position);
			gtk_list_store_set (list, &iter, 0, NULL, 1, list_item[1], 2, list_item[2], 3, list_item[3], -1);
			
			g_free(list_item[0]);
			g_free(list_item[1]);
			g_free(list_item[2]);
			g_free(list_item[3]);
		}
	}

	GDK_THREADS_LEAVE();

	pthread_mutex_unlock(&playlist_window->playlist_list_mutex);
}

void PlaylistWindow::CbRemove(void *data, unsigned start, unsigned end)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *)data;

	pthread_mutex_lock(&playlist_window->playlist_list_mutex);	

	GDK_THREADS_ENTER();

	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_window->GetList())));
	GtkTreeIter iter;
	gchar *start_string = NULL;
	
	unsigned i = start;
	while(i <= end) {
	
		start_string = g_strdup_printf("%d", start - 1);
		gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, start_string);
	
		gtk_list_store_remove(list, &iter);
	
		i++;
	}

	g_free(start_string);
	
	GDK_THREADS_LEAVE();

	pthread_mutex_unlock(&playlist_window->playlist_list_mutex);
}

void PlaylistWindow::CbClear(void *data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *)data;

	pthread_mutex_lock(&playlist_window->playlist_list_mutex);
	GDK_THREADS_ENTER();
	playlist_window->Clear();
	GDK_THREADS_LEAVE();
	pthread_mutex_unlock(&playlist_window->playlist_list_mutex);
}

void PlaylistWindow::Show()
{
	if(!GTK_WIDGET_VISIBLE(window)) {
		gtk_widget_show_all(window);
	}
}

void PlaylistWindow::Hide()
{
	if(GTK_WIDGET_VISIBLE(window)) {
		width = window->allocation.width;
		height = window->allocation.height;
		gtk_widget_hide_all(window);
	}
}

void PlaylistWindow::Play(int number)
{
	GDK_THREADS_LEAVE();
		playlist->Pause();
		playlist->Play(number);
		playlist->UnPause();
	GDK_THREADS_ENTER();
}

void PlaylistWindow::PlayPrev()
{
	GDK_THREADS_LEAVE();
		playlist->Pause();
		playlist->Prev();
		playlist->UnPause();
	GDK_THREADS_ENTER();
}

void PlaylistWindow::PlayNext()
{
	GDK_THREADS_LEAVE();
		playlist->Pause();
		playlist->Next();
		playlist->UnPause();
	GDK_THREADS_ENTER();
}

void PlaylistWindow::SetStop()
{
	if (!this->playlist->Length())
		return;
		
	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(GetList())));
	GtkTreeIter iter;
	
	if (!current_play_pix) {	
		current_play_pix = gdk_pixbuf_new_from_xpm_data((const char **)current_play_xpm);
		current_stop_pix = gdk_pixbuf_new_from_xpm_data((const char **)current_stop_xpm);
		
	} else {
		GDK_THREADS_ENTER();
		gchar *current_string = g_strdup_printf("%d", current_entry - 1);
		gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, current_string);
		gtk_list_store_set (list, &iter, 0, current_stop_pix, -1);
		g_free(current_string);		
		GDK_THREADS_LEAVE();
	}
}

void PlaylistWindow::SetPlay()
{
	if (!this->playlist->Length())
		return;
		
	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(GetList())));
	GtkTreeIter iter;
	
	if (!current_play_pix) {	
		current_play_pix = gdk_pixbuf_new_from_xpm_data((const char **)current_play_xpm);
		current_stop_pix = gdk_pixbuf_new_from_xpm_data((const char **)current_stop_xpm);
		
	} else {
		GDK_THREADS_ENTER();
		gchar *current_string = g_strdup_printf("%d", current_entry - 1);
		gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, current_string);
		gtk_list_store_set (list, &iter, 0, current_play_pix, -1);
		g_free(current_string);		
		GDK_THREADS_LEAVE();
	}
}

void PlaylistWindow::AddFile()
{
	GtkWidget *add = GTK_WIDGET(g_object_get_data(G_OBJECT(this->window), "add_file"));
	gtk_widget_show_all(add);
}
