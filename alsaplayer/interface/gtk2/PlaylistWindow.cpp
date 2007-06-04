/*  PlayListWindow.cpp - playlist window 'n stuff
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
 *  Copyright (C) 2007 Madej
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

#ifdef ENABLE_NLS
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

// Forward declarations

static GtkWidget *init_playlist_window(PlaylistWindowGTK *, Playlist *pl, GtkWidget *main_window);
static void new_list_item(const PlayItem *item, gchar **list_item);
static int current_entry = -1;
void playlist_play_current(Playlist *playlist, GtkWidget *gtklist);
void dialog_popup(GtkWidget *widget, gpointer data);


// Drag & Drop stuff
#define TARGET_URI_LIST 0x0001

static GtkTargetEntry drag_types[] = {
	        {"text/uri-list", 0, TARGET_URI_LIST}
};
static int n_drag_types = sizeof(drag_types)/sizeof(drag_types[0]);



// Member functions

PlaylistWindowGTK::PlaylistWindowGTK(Playlist * pl, GtkWidget *main_window) {
	playlist = pl;
	
	//alsaplayer_error("PlaylistWindowGTK constructor entered");

	playlist_window = init_playlist_window(this, pl, main_window);
	playlist_list = (GtkWidget *)g_object_get_data(G_OBJECT(playlist_window), "playlist");
	playlist_status = (GtkLabel *) g_object_get_data(G_OBJECT(playlist_list), "status");
	showing = false;

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

PlaylistWindowGTK::~PlaylistWindowGTK() {
	prefs_set_bool(ap_prefs, "gtk2_interface", "playlist_active", showing);
	
	Hide();
	gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(playlist_list))));
	playlist->UnRegister(&pli); // XXX
}

#include "pixmaps/current_play.xpm"
#include "pixmaps/current_stop.xpm"
static GdkPixbuf *current_play_pix = NULL;
static GdkPixbuf *current_stop_pix = NULL;

// Set item currently playing
void PlaylistWindowGTK::CbSetCurrent(void *data, unsigned current) {
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;
	GtkStyle *style;

	if (current == 0)
	    return;

	GDK_THREADS_ENTER();

	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkpl->playlist_list)));
	GtkTreeIter iter;
		
	if (!current_play_pix) {
		style = gtk_widget_get_style(GTK_WIDGET(gtkpl->playlist_list));
		if (!GTK_WIDGET(gtkpl->playlist_window)->window) {
			gtk_widget_realize(gtkpl->playlist_window);
			gdk_flush();
		}	
		current_play_pix = gdk_pixbuf_new_from_xpm_data((const char **)current_play_xpm);
		current_stop_pix = gdk_pixbuf_new_from_xpm_data((const char **)current_stop_xpm);
		
	} else {
		if (current_entry <= gtkpl->GetPlaylist()->Length()) {
			gchar *current_string = g_strdup_printf("%d", current_entry - 1);
			gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, current_string);
			gtk_list_store_set (list, &iter, 0, NULL, -1);
			g_free(current_string);
		}
	}	
	current_entry = current;

	gchar *current_string = g_strdup_printf("%d", current_entry - 1);
	gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, current_string);
	gtk_list_store_set (list, &iter, 0, current_play_pix, -1);
	g_free(current_string);
	
	GDK_THREADS_LEAVE();
}


void PlaylistWindowGTK::CbUpdated(void *data,PlayItem & item, unsigned position) {
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;
	char tmp[1024];

	//alsaplayer_error("About to lock list");
	pthread_mutex_lock(&gtkpl->playlist_list_mutex);
	//alsaplayer_error("Locked");

	GDK_THREADS_ENTER();

	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkpl->playlist_list)));
	GtkTreeIter iter;

	gchar *position_string = g_strdup_printf("%d", position);
	gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, position_string);

	if (item.title.size()) {
		std::string s = item.title;
		if (item.artist.size())
		{
			s += std::string(" - ") + item.artist;
		}
		gchar *data =  g_strdup(s.c_str());
		gtk_list_store_set (list, &iter, 1, data, -1);
		g_free(data);
	}
	if (item.playtime >= 0) {
		sprintf(tmp, "%02d:%02d", item.playtime / 60, item.playtime % 60);
		
		gchar *data =  g_strdup(tmp);
		gtk_list_store_set (list, &iter, 2, data, -1);
		g_free(data);

	}	
		
	g_free(position_string);
	
	GDK_THREADS_LEAVE();

	pthread_mutex_unlock(&gtkpl->playlist_list_mutex);
}

// Insert new items into the displayed list
void PlaylistWindowGTK::CbInsert(void *data,std::vector<PlayItem> & items, unsigned position) {
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;
	
	//alsaplayer_error("CbInsert(`%d items', %d)", items.size(), position);

	pthread_mutex_lock(&gtkpl->playlist_list_mutex);

	GDK_THREADS_ENTER();

	std::vector<PlayItem> item_copy = items;

	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkpl->playlist_list)));
	GtkTreeIter iter;

	if(items.size() > 0) {
		std::vector<PlayItem>::const_iterator item;
		for(item = items.begin(); item != items.end(); item++, position++) {
			// Make a new item
			gchar *list_item[4];
			new_list_item(&(*item), list_item);

			gtk_list_store_insert (list, &iter, position);
			gtk_list_store_set (list, &iter, 0, NULL, 1, list_item[1], 2, list_item[2], -1);
			
			g_free(list_item[0]);
			g_free(list_item[1]);
			g_free(list_item[2]);
			g_free(list_item[3]);
//			index ++;
		}
	}
//	gtk_clist_thaw(GTK_CLIST(gtkpl->playlist_list));

	GDK_THREADS_LEAVE();

	pthread_mutex_unlock(&gtkpl->playlist_list_mutex);
}

// Remove items from start to end
void PlaylistWindowGTK::CbRemove(void *data, unsigned start, unsigned end)
{
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;

	pthread_mutex_lock(&gtkpl->playlist_list_mutex);	

	GDK_THREADS_ENTER();

	GtkListStore *list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkpl->playlist_list)));
	GtkTreeIter iter;
	gchar *start_string = NULL;
	
	unsigned i = start;
	while(i <= end) {
	
		start_string = g_strdup_printf("%d", start - 1); //should be i ?
		gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(list), &iter, start_string);
	
		gtk_list_store_remove(list, &iter);
	
		i++;
	}

	g_free(start_string);
	
	GDK_THREADS_LEAVE();

	pthread_mutex_unlock(&gtkpl->playlist_list_mutex);
}

// Clear the displayed list
void PlaylistWindowGTK::CbClear(void *data)
{
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;

	pthread_mutex_lock(&gtkpl->playlist_list_mutex);
	GDK_THREADS_ENTER();
	gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkpl->playlist_list))));
	GDK_THREADS_LEAVE();
	pthread_mutex_unlock(&gtkpl->playlist_list_mutex);
}


// Show the playlist
void PlaylistWindowGTK::Show() {
	if(!showing) {
		gtk_widget_show_all(playlist_window);
		showing = true;
	}
}

// Hide the playlist
void PlaylistWindowGTK::Hide() {
	
	if(showing) {
		gint x, y;
		gdk_window_get_origin(playlist_window->window, &x, &y);
		if (windows_x_offset >= 0) {
			x -= windows_x_offset;
			y -= windows_y_offset;
		}
		gtk_widget_hide(playlist_window);
		gtk_widget_set_uposition(playlist_window, x, y);
		showing = false;
	}
}

// Toggle visibility
void PlaylistWindowGTK::ToggleVisible() {
	if (showing) {
		Hide();
	} else {
		Show();
	}
}


// Called when "prev" button is pressed
extern void playlist_window_gtk_prev(GtkWidget *, gpointer data) {
	Playlist * playlist = (Playlist *) data;
	if(playlist) {
		playlist->Pause();
		GDK_THREADS_LEAVE();
		playlist->Prev();
		GDK_THREADS_ENTER();
		playlist->UnPause();
	}
}


// Called when "next" button is pressed
void playlist_window_gtk_next(GtkWidget *widget, gpointer data)
{
	Playlist * playlist = (Playlist *) data;
	if(playlist) {
		playlist->Pause();
		GDK_THREADS_LEAVE();
		playlist->Next();
		GDK_THREADS_ENTER();
		playlist->UnPause();
	}
}

int get_path_number(GtkTreePath *data)
{
	int number;
	gchar *path = gtk_tree_path_to_string(data);
	gtk_tree_path_free(data);
	number = atoi(path);
	g_free(path);
	
	return number;
}

void playlist_play_current(Playlist *playlist, GtkWidget *tree)
{
		int selected;
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
		
		if (gtk_tree_selection_count_selected_rows(selection) != 1)
			return;
			
		GList* data = gtk_tree_selection_get_selected_rows(selection, NULL);
		
		selected = get_path_number((GtkTreePath *)data->data);
		
		g_list_free(data);
				
		GDK_THREADS_LEAVE();
		playlist->Pause();
		playlist->Play(selected + 1);
		playlist->UnPause();
		GDK_THREADS_ENTER();
}

// Called when remove button is clicked
void playlist_remove(GtkWidget *, gpointer user_data)
{
	PlaylistWindowGTK *playlist_window_gtk = (PlaylistWindowGTK *) user_data;
	Playlist *playlist = NULL;
	GtkWidget *list = NULL;
	GList *next, *start;

	if (playlist_window_gtk) {
			playlist = playlist_window_gtk->GetPlaylist();
	} else
		return;

	list = playlist_window_gtk->GetPlaylist_list();	

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
				playlist->Stop();
				playlist->Next();
			}
//			if (playlist->Length() == (selected+1)) {
//			}	
			playlist->Remove(selected+1, selected+1);
			GDK_THREADS_ENTER();
			next = next->prev;	
		}
//		if (playlist->Length() == selected) {
//			selected--;
//		}	
		g_list_free(data);
	}
}


// Called when shuffle button clicked
void shuffle_cb(GtkWidget *, gpointer data)
{
	Playlist *playlist = (Playlist *)data;
	if (playlist) {
		GDK_THREADS_LEAVE();
		playlist->Shuffle();
		GDK_THREADS_ENTER();
	}	
}

// Called when clear button clicked
void clear_cb(GtkWidget *widget, gpointer data)
{
	Playlist *playlist = (Playlist *)data;
	if (playlist) {
		GDK_THREADS_LEAVE();
		playlist->Clear();
		GDK_THREADS_ENTER();
	}	
}

gboolean list_resize(GtkWidget *widget, GdkEventConfigure *, gpointer data)
{
	GtkWidget *list = (GtkWidget *)data;	
	GtkWidget *window;
	static gint old_width = -1;
	gint w, h;
	
	window = (GtkWidget *)g_object_get_data(G_OBJECT(list), "window");
	
	if (list) {
		// For some reason the clist widget is always a few events
		// behind reality. What fun! :-(
		if (widget->allocation.width != old_width) {
			if (window) {
				gdk_window_get_size(window->window, &w, &h);
				//alsaplayer_error("(%d, %d)", w, h);
				gtk_clist_set_column_width(GTK_CLIST(list),
						1, w-200);	

			}	
		}
		old_width = widget->allocation.width;
	}
	return FALSE;
}

// Called when window gets closed, so we don't try to close it again later.
static gboolean
playlist_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PlaylistWindowGTK * playlist_window_gtk = (PlaylistWindowGTK *) data;
	if (playlist_window_gtk) {
		playlist_window_gtk->Hide();
	}
	return TRUE;
}

void close_cb(GtkWidget *, gpointer data)
{
	PlaylistWindowGTK * playlist_window_gtk = (PlaylistWindowGTK *) data;
	if (playlist_window_gtk) {
		playlist_window_gtk->Hide();
	}
}

// Make a new item to go in the list
static void new_list_item(const PlayItem *item, gchar **list_item)
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
	list_item[2] = (gchar *)g_strdup(pt);
	// Strip directory names
	dirname = strrchr(new_path, '/');
	if (dirname) {
			dirname++;
			filename = (gchar *)g_strdup(dirname);
	} else
			filename = (gchar *)g_strdup(new_path);
	if (item->title.size()) {
		std::string s = item->title;
		if (item->artist.size())
		{
			s += std::string(" - ") + item->artist;
		}
		list_item[1] = (gchar *)g_strdup(s.c_str());
	} else {
		list_item[1] = (gchar *)g_strdup(filename);
	}	
	// Put data in list_item
	list_item[3] = new_path;
}


// Called when a file has been selected to be loaded as a playlist
void load_list_ok(GtkWidget *widget, gpointer user_data)
{
//  PlaylistWindowGTK *playlist_window_gtk = (PlaylistWindowGTK *)user_data;
	//gtk_widget_hide(GTK_WIDGET(playlist_window_gtk->load_list));
	//Playlist *playlist = playlist_window_gtk->GetPlaylist();
	Playlist *playlist = (Playlist*) user_data;
	enum plist_result loaderr;

	gchar *current = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
	
//	int marker = strlen(current_dir)-1;
//	while (marker > 0 && current_dir[marker] != '/') // Get rid of the filename
//		current_dir[marker--] = '\0';
//	prefs_set_string(ap_prefs, "gtk2_interface", "default_playlist_load_path", current);

//	std::string file(gtk_file_selection_get_filename(GTK_FILE_SELECTION(playlist_window_gtk->load_list)));

		if (!current) {
			current = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(widget));
		}
		
	GDK_THREADS_LEAVE();
	loaderr = playlist->Load(current, playlist->Length(), false);
	GDK_THREADS_ENTER();
	
	if(loaderr == E_PL_DUBIOUS) {
		// FIXME - pop up a dialog and check if we really want to load
		alsaplayer_error("Dubious whether file is a playlist - trying anyway");
		GDK_THREADS_LEAVE();
		loaderr = playlist->Load(current, playlist->Length(), true);
		GDK_THREADS_ENTER();
	}
	if(loaderr) {
		// FIXME - pass playlist_window to this routine
	}
	g_free(current);
}

void save_list_ok(GtkWidget *widget, gpointer user_data)
{
	Playlist *playlist = (Playlist*) user_data;

	gchar *current = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));

//	prefs_set_string(ap_prefs, "gtk2_interface", "default_playlist_save_path", current_dir);
		
	enum plist_result saveerr;
	saveerr = playlist->Save(current, PL_FORMAT_M3U);
	if(saveerr) {
		// FIXME - pass playlist_window to this routine
	} else {
		// FIXME - pass playlist_window to this routine
	}
	g_free(current);
}

//
// Old stuff below here - this needs to be sorted out
//

void dialog_cancel(GtkWidget *, gpointer data)
{
	GtkWidget *dialog = (GtkWidget *)data;

	gtk_widget_hide(dialog);
}


void dialog_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gtk_widget_hide(widget);
}


void dialog_popup(GtkWidget *, gpointer data)
{
        gtk_widget_show_all(GTK_WIDGET(data));
} 



gboolean key_press_event(GtkWidget *, GdkEvent *event, gpointer data)
{
//	printf("Key down\n");
	return FALSE;
}


gboolean button_press_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	Playlist *playlist = (Playlist *) user_data;
	GtkWidget *win = (GtkWidget *)g_object_get_data(G_OBJECT(widget), "window");
	if (win && (event && event->type == GDK_2BUTTON_PRESS)) {
		// Double click - play from the clicked item
		playlist_play_current(playlist, widget);
	}

	gtk_widget_grab_focus(widget);
	
	return FALSE;
}


gint dnd_drop_event(GtkWidget *widget,
		GdkDragContext   * context,
		gint              /* x */,
		gint              /* y */,
		GtkSelectionData *selection_data,
		guint             info,
		guint             * /* time */,
		void * /* data */)
{
	char *uri = NULL;
	char *filename = NULL;
	char *p, *s, *res;

	if (!selection_data)
		return 0;
	
	switch(info) {
		case TARGET_URI_LIST:
			//alsaplayer_error("TARGET_URI_LIST drop (%d,%d)", x, y);
			uri = (char *)malloc(strlen((const char *)selection_data->data)+1);
			strcpy(uri, (const char *)selection_data->data);
			filename = uri;
			while (filename) {
				if ((p=s=strstr(filename, "\r\n"))) {
					*s = '\0';
					p = s+2;
				}	
				if (strlen(filename)) {
					//alsaplayer_error("Adding: %s", filename);
					res = parse_file_uri(filename);
					if (res) {
						GDK_THREADS_LEAVE();
						if (is_playlist(res)) {
							ap_add_playlist(global_session_id, res);
						} else {
							ap_add_path(global_session_id, res);
						}	
						GDK_THREADS_ENTER();
						parse_file_uri_free(res);
					}	
				}
				filename = p;
			}
			free(uri);
			break;
		default:
			alsaplayer_error("Unknown drop!");
			break;
	}               
	return 1;
}

void load_dialog_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
		load_list_ok(GTK_WIDGET(dialog), (gpointer) user_data);

		dialog_cancel_response(GTK_WIDGET(dialog), NULL);
}

GtkWidget*
create_playlist_load(GtkWindow *main_window, Playlist *playlist)
{
	GtkWidget *filechooser;
	
	filechooser = gtk_file_chooser_dialog_new("Choose playlist" , main_window, GTK_FILE_CHOOSER_ACTION_OPEN, 
  																GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     											GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      											NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), FALSE);
	
	g_signal_connect(G_OBJECT(filechooser), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(filechooser), "response", G_CALLBACK(load_dialog_cb), (gpointer) playlist);
	
	return filechooser;
}

void save_dialog_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
		save_list_ok(GTK_WIDGET(dialog), (gpointer) user_data);

		dialog_cancel_response(GTK_WIDGET(dialog), NULL);
}

GtkWidget*
create_playlist_save(GtkWindow *main_window, Playlist *playlist)
{
	GtkWidget *filechooser;
	
	filechooser = gtk_file_chooser_dialog_new("Save playlist" , main_window, GTK_FILE_CHOOSER_ACTION_SAVE, 
  																GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     											GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      											NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), FALSE);
	
	g_signal_connect(G_OBJECT(filechooser), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(filechooser), "response", G_CALLBACK(save_dialog_cb), (gpointer) playlist);
	
	return filechooser;
}

GtkWidget*
create_filechooser(GtkWindow *main_window, Playlist *playlist)
{
	GtkWidget *filechooser;
	
	filechooser = gtk_file_chooser_dialog_new(_("Choose file or URL"), main_window, GTK_FILE_CHOOSER_ACTION_OPEN, 
  																GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     											GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      											NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), TRUE);
	
	g_signal_connect(G_OBJECT(filechooser), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(filechooser), "response", G_CALLBACK(play_dialog_cb), (gpointer) playlist);
	
	return filechooser;
}


GtkWidget*
create_playlist_window (PlaylistWindowGTK *playlist_window_gtk, Playlist *pl)
{
	GtkWidget *playlist_window;
	GtkWidget *main_box;
	GtkWidget *scrolledwindow;
	GtkListStore *playlist_model;
	GtkWidget *playlist;
	GtkWidget *button_box;
	GtkWidget *add_button;
	GtkWidget *del_button;
	GtkWidget *close_button;
	GtkWidget *shuffle_button;
	GtkWidget *pl_button_box;
	GtkWidget *playlist_label;
	GtkWidget *load_button;
	GtkWidget *save_button;
	GtkWidget *clear_button;
	GtkWidget *loop_button;
	GtkTooltips *tooltips;
		
	tooltips = gtk_tooltips_new();
	
	playlist_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW(playlist_window), 480, 390);
	gtk_window_set_title (GTK_WINDOW (playlist_window), _("Playlist"));

	main_box = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER (playlist_window), main_box);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (main_box), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	playlist_model = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
 
	playlist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(playlist_model));
	g_object_set_data(G_OBJECT(playlist_window), "playlist", playlist);
	g_object_unref(playlist_model);
  
	gtk_container_add (GTK_CONTAINER (scrolledwindow), playlist);

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes("playing", renderer, "pixbuf", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (playlist), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("title", renderer, "text", 1, NULL);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (playlist), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("time", renderer, "text", 2, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (playlist), column);
	
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(playlist)), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(playlist), FALSE);

	button_box = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (main_box), button_box, FALSE, TRUE, 6);

	add_button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_box_pack_start (GTK_BOX (button_box), add_button, FALSE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), add_button, _("Add a song into the playlist"), NULL);

	del_button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_box_pack_start (GTK_BOX (button_box), del_button, FALSE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), del_button, _("Remove the selected song from the playlist"), NULL);

	close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end (GTK_BOX (button_box), close_button, FALSE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), close_button, _("Close this window"), NULL);

	shuffle_button = gtk_button_new_with_label (_("Shuffle"));
	gtk_box_pack_start (GTK_BOX (button_box), shuffle_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), shuffle_button, _("Randomize the playlist"), NULL);

	pl_button_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_box), pl_button_box, FALSE, TRUE, 18);

	playlist_label = gtk_label_new ("PLAYLIST");
	gtk_box_pack_start (GTK_BOX (pl_button_box), playlist_label, FALSE, FALSE, 3);

	load_button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
	gtk_box_pack_start (GTK_BOX (pl_button_box), load_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), load_button, _("Open a playlist"), NULL);

	save_button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
	gtk_box_pack_start (GTK_BOX (pl_button_box), save_button, FALSE, FALSE, 6);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), save_button, _("Save the playlist"), NULL);

	clear_button = gtk_button_new_from_stock (GTK_STOCK_CLEAR);
	gtk_box_pack_start (GTK_BOX (pl_button_box), clear_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), clear_button, _("Remove the current playlist"), NULL);

	loop_button = gtk_button_new_with_label (_("Loop"));
	g_object_set_data(G_OBJECT(playlist_window), "loop_button", loop_button);
	gtk_box_pack_start (GTK_BOX (button_box), loop_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), loop_button, _("Create loop"), _("AlsaPlayer will play a sound which is played between clicks on this button"));

	gtk_drag_dest_set(playlist_window, GTK_DEST_DEFAULT_ALL,
						drag_types, n_drag_types, (GdkDragAction) (GDK_ACTION_MOVE | GDK_ACTION_COPY));

	playlist_window_gtk->add_file = create_filechooser(GTK_WINDOW(playlist_window), pl);
	playlist_window_gtk->load_list = create_playlist_load(GTK_WINDOW(playlist_window), pl);
	playlist_window_gtk->save_list = create_playlist_save(GTK_WINDOW(playlist_window), pl);
	
	g_object_set_data(G_OBJECT(playlist_window_gtk->add_file), "list", playlist);
	g_object_set_data(G_OBJECT(playlist_window_gtk->save_list), "list", playlist);
		
	g_signal_connect(G_OBJECT(playlist_window), "destroy", G_CALLBACK(playlist_delete_event), (gpointer)playlist_window_gtk);
	g_signal_connect(G_OBJECT(playlist_window), "delete_event", G_CALLBACK(playlist_delete_event), (gpointer)playlist_window_gtk);
	g_signal_connect(G_OBJECT(playlist_window), "key_press_event", G_CALLBACK(key_press_cb), (gpointer)playlist_window_gtk);
	g_signal_connect(G_OBJECT(gtk_widget_get_toplevel(playlist_window)), "drag_data_received", G_CALLBACK(dnd_drop_event), NULL);

	g_signal_connect(G_OBJECT(playlist), "button_press_event", G_CALLBACK(button_press_event), pl);
	g_signal_connect(G_OBJECT(shuffle_button), "clicked", G_CALLBACK(shuffle_cb), pl);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(dialog_popup), (gpointer)playlist_window_gtk->add_file);
	g_signal_connect(G_OBJECT(clear_button), "clicked", G_CALLBACK(clear_cb), pl);
	g_signal_connect(G_OBJECT(del_button), "clicked", G_CALLBACK(playlist_remove), (gpointer)playlist_window_gtk);
	g_signal_connect(G_OBJECT(close_button), "clicked",	G_CALLBACK(close_cb), (gpointer)playlist_window_gtk);
	g_signal_connect(G_OBJECT(save_button), "clicked", G_CALLBACK(dialog_popup), (gpointer)playlist_window_gtk->save_list);
	g_signal_connect(G_OBJECT(load_button), "clicked", G_CALLBACK(dialog_popup), (gpointer)playlist_window_gtk->load_list);
	
	gtk_widget_grab_focus(GTK_WIDGET(playlist));
		
	return playlist_window;
}

static GtkWidget *init_playlist_window(PlaylistWindowGTK *playlist_window_gtk, Playlist *playlist, GtkWidget *main_window)
{
	GtkWidget *playlist_window;
	GtkWidget *list;
	
	playlist_window = create_playlist_window(playlist_window_gtk, playlist);
	list = (GtkWidget *)g_object_get_data(G_OBJECT(playlist_window), "playlist");
	g_object_set_data(G_OBJECT(list), "window", playlist_window);
	g_object_set_data(G_OBJECT(playlist_window), "Playlist", playlist);

	g_signal_connect(g_object_get_data(G_OBJECT(playlist_window), "loop_button"), "clicked", G_CALLBACK(loop_cb), g_object_get_data(G_OBJECT(main_window), "pos_scale"));

	return playlist_window;
}

