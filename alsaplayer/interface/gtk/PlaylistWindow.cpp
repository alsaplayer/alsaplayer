/*  PlayListWindow.cpp - playlist window 'n stuff
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkdnd.h>
#include <glib.h>

#include <vector>
#include <algorithm>

#include "AlsaPlayer.h"
#include "Playlist.h"
#include "PlaylistWindow.h"
#include "support.h"
#include "gladesrc.h"
#include "gtk_interface.h"
#include "utilities.h"
#include "prefs.h"
#include "alsaplayer_error.h"
#include "control.h"

// Forward declarations

static GtkWidget *init_playlist_window(PlaylistWindowGTK *, Playlist *pl);
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

PlaylistWindowGTK::PlaylistWindowGTK(Playlist * pl) {
	playlist = pl;
	
	//alsaplayer_error("PlaylistWindowGTK constructor entered");

	playlist_window = init_playlist_window(this, pl);
	playlist_list = get_widget(playlist_window, "playlist");
	playlist_status = (GtkLabel *)
		gtk_object_get_data(GTK_OBJECT(playlist_list), "status");
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
	prefs_set_bool(ap_prefs, "gtk_interface", "playlist_active", showing);
	
	Hide();
	gtk_clist_clear(GTK_CLIST(playlist_list));
	playlist->UnRegister(&pli); // XXX
}

#include "pixmaps/current_play.xpm"
#include "pixmaps/current_stop.xpm"
static GdkPixmap *current_play_pix = (GdkPixmap *)NULL;
static GdkBitmap *current_play_mask = (GdkBitmap *)NULL;
static GdkPixmap *current_stop_pix = (GdkPixmap *)NULL;
static GdkBitmap *current_stop_mask = (GdkBitmap *)NULL;


// Set item currently playing
void PlaylistWindowGTK::CbSetCurrent(void *data, unsigned current) {
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;
	GtkStyle *style;

	GDK_THREADS_ENTER();
	
	if (!current_play_pix) {
		style = gtk_widget_get_style(GTK_WIDGET(gtkpl->playlist_list));
		if (!GTK_WIDGET(gtkpl->playlist_window)->window) {
			gtk_widget_realize(gtkpl->playlist_window);
			gdk_flush();
		}	
		current_play_pix = gdk_pixmap_create_from_xpm_d(
			GTK_WIDGET(gtkpl->playlist_window)->window,
			&current_play_mask, &style->bg[GTK_STATE_NORMAL],
			current_play_xpm);
		current_stop_pix = gdk_pixmap_create_from_xpm_d(
			GTK_WIDGET(gtkpl->playlist_window)->window,
			&current_stop_mask, &style->bg[GTK_STATE_NORMAL],
			current_stop_xpm);
	} else {
		gtk_clist_set_text(GTK_CLIST(gtkpl->playlist_list), current_entry - 1,
			0, "");
	}	
	current_entry = current;	
	
	gtk_clist_set_pixmap(GTK_CLIST(gtkpl->playlist_list), current_entry - 1,
		0, current_play_pix, current_play_mask);
	
	GDK_THREADS_LEAVE();
}


void PlaylistWindowGTK::CbUpdated(void *data,PlayItem & item, unsigned position) {
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;
	char tmp[1024];

	//alsaplayer_error("About to lock list");
	pthread_mutex_lock(&gtkpl->playlist_list_mutex);
	//alsaplayer_error("Locked");

	GDK_THREADS_ENTER();

	gtk_clist_freeze(GTK_CLIST(gtkpl->playlist_list));
	if (item.title.size()) {
		sprintf(tmp, "%s %s", item.title.c_str(),
			item.artist.size() ? (std::string("- ") + item.artist).c_str() : "");
		gtk_clist_set_text(GTK_CLIST(gtkpl->playlist_list), position,
				1, g_strdup(tmp));
	}
	if (item.playtime >= 0) {
		sprintf(tmp, "%02d:%02d", item.playtime / 60, item.playtime % 60);
		gtk_clist_set_text(GTK_CLIST(gtkpl->playlist_list), position,
				2, g_strdup(tmp));
	}	
		
	gtk_clist_thaw(GTK_CLIST(gtkpl->playlist_list));
	
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

	gtk_clist_freeze(GTK_CLIST(gtkpl->playlist_list));

	if(items.size() > 0) {
		std::vector<PlayItem>::const_iterator item;
		for(item = items.begin(); item != items.end(); item++, position++) {
			// Make a new item
			gchar *list_item[4];
			new_list_item(&(*item), list_item);

			// Add it to the playlist
			int index = gtk_clist_insert(GTK_CLIST(gtkpl->playlist_list), position, list_item);
			gtk_clist_set_shift(GTK_CLIST(gtkpl->playlist_list), index, 1, 2, 2);
			gtk_clist_set_shift(GTK_CLIST(gtkpl->playlist_list), index, 2, 2, 2);


			index ++;
		}
	}
	gtk_clist_thaw(GTK_CLIST(gtkpl->playlist_list));

	GDK_THREADS_LEAVE();

	pthread_mutex_unlock(&gtkpl->playlist_list_mutex);
}

// Remove items from start to end
void PlaylistWindowGTK::CbRemove(void *data, unsigned start, unsigned end)
{
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;

	pthread_mutex_lock(&gtkpl->playlist_list_mutex);	

	GDK_THREADS_ENTER();
	gtk_clist_freeze(GTK_CLIST(gtkpl->playlist_list));

	unsigned i = start;
	while(i <= end) {
		gtk_clist_remove(GTK_CLIST(gtkpl->playlist_list), start - 1);
		i++;
	}

	gtk_clist_thaw(GTK_CLIST(gtkpl->playlist_list));
	GDK_THREADS_LEAVE();

	pthread_mutex_unlock(&gtkpl->playlist_list_mutex);
}

// Clear the displayed list
void PlaylistWindowGTK::CbClear(void *data)
{
	PlaylistWindowGTK *gtkpl = (PlaylistWindowGTK *)data;

	pthread_mutex_lock(&gtkpl->playlist_list_mutex);
	GDK_THREADS_ENTER();
	gtk_clist_clear(GTK_CLIST(gtkpl->playlist_list));
	GDK_THREADS_LEAVE();
	pthread_mutex_unlock(&gtkpl->playlist_list_mutex);
}


// Show the playlist
void PlaylistWindowGTK::Show() {
	if(!showing) {
		gtk_widget_show(playlist_window);
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



// Called when playlist is clicked to select an item
void playlist_click(GtkWidget *widget, gint /* row */, gint /* column */, 
					GdkEvent *bevent, gpointer data)
{
	Playlist *playlist = (Playlist *) data;
	GtkWidget *win = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget), "window");
	if (win && (bevent && bevent->type == GDK_2BUTTON_PRESS)) {
		// Double click - play from the clicked item
		playlist_play_current(playlist, widget);
	}
}


void playlist_play_current(Playlist *playlist, GtkWidget *list)
{
		int selected;
		if (!GTK_CLIST(list)->selection)
			return;
		selected = GPOINTER_TO_INT(GTK_CLIST(list)->selection->data);
		GDK_THREADS_LEAVE();
		playlist->Pause();
		playlist->Play(selected + 1);
		playlist->UnPause();
		GDK_THREADS_ENTER();
}

// Called when remove button is clicked
void playlist_remove(GtkWidget *, gpointer data)
{
	PlaylistWindowGTK *playlist_window_gtk = (PlaylistWindowGTK *) data;
	Playlist *playlist = NULL;
	GtkWidget *list = NULL;
	GList *next, *start;

	if (playlist_window_gtk) {
			playlist = playlist_window_gtk->GetPlaylist();
	} else
		return;
	list = playlist_window_gtk->GetPlaylist_list();	
	if (playlist && list) {
		int selected = 0;
		next = start = GTK_CLIST(list)->selection;
		if (next == NULL) { // Nothing was selected
			return;
		}	
		
		while (next->next != NULL) {
			next = next->next;
		}	
		while (next != start->prev) {
			selected = GPOINTER_TO_INT(next->data);
			GDK_THREADS_LEAVE();
			if (playlist->GetCurrent() == selected+1) {
				playlist->Stop();
				playlist->Next();
			}
			if (playlist->Length() == (selected+1)) {
				gtk_clist_unselect_row(GTK_CLIST(list), 
						selected, 0);
				//alsaplayer_error("Early trigger");
			}	
			playlist->Remove(selected+1, selected+1);
			GDK_THREADS_ENTER();
			next = next->prev;	
		}
		if (playlist->Length() == selected) {
			selected--;
		}	
		gtk_clist_select_row(GTK_CLIST(list), selected, 0);
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

gint list_resize(GtkWidget *widget, GdkEventConfigure *, gpointer data)
{
	GtkWidget *list = (GtkWidget *)data;	
	GtkWidget *window;
	static gint old_width = -1;
	gint w, h;
	
	window = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(list), "window");
	
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
	return 0;
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
		sprintf(pt, "%s %s", item->title.c_str(), 
			item->artist.size() ? (std::string("- ") + item->artist).c_str() :
				"");
	} else {
		sprintf(pt, "%s", filename);
	}	
	// Put data in list_item
	list_item[1] = (gchar *)g_strdup(pt);
	list_item[3] = new_path;
}




// Called when files have been selected for adding to playlist
void add_file_ok(GtkWidget *, gpointer data)
{
	gchar *sel;
	PlaylistWindowGTK * playlist_window_gtk = (PlaylistWindowGTK *) data;
	GtkWidget *add_file = playlist_window_gtk->add_file;

	GtkCList *file_list = GTK_CLIST(GTK_FILE_SELECTION(add_file)->file_list);
	Playlist *playlist = playlist_window_gtk->GetPlaylist();
	GList *next = file_list->selection;

	if (!playlist) {
		return;
	}	
	gchar *current_dir =
		g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(add_file)));

	int marker = strlen(current_dir)-1;
	while (marker > 0 && current_dir[marker] != '/') // Get rid of the filename
		current_dir[marker--] = '\0';
	prefs_set_string(ap_prefs, "gtk_interface", "default_playlist_add_path", current_dir);

	std::vector<std::string> paths;
	
	if (!next) {
		sel = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_FILE_SELECTION(add_file)->selection_entry)));
		if (sel && strlen(sel)) {
			if (!strstr(sel, "http://"))
				paths.push_back(std::string(current_dir) + "/" + sel);
			else
				paths.push_back(sel);
			GDK_THREADS_LEAVE();
			playlist->Insert(paths, playlist->Length());
			GDK_THREADS_ENTER();
			g_free(sel);
		}
		gtk_entry_set_text(GTK_ENTRY(GTK_FILE_SELECTION(add_file)->selection_entry), "");
		return;
	}	
	while (next) { // Walk the selection list
		char *path;
		int index = GPOINTER_TO_INT(next->data);
		
		gtk_clist_get_text(file_list, index, 0, &path);
		if (path) {
			paths.push_back(std::string(current_dir) + "/" + path);
		}
		next = next->next;
	}
	sort(paths.begin(), paths.end());
	gtk_clist_unselect_all(file_list); // Clear selection
	g_free(current_dir);

	// Insert all the selected paths
	if (playlist) {
		GDK_THREADS_LEAVE();
		playlist->Insert(paths, playlist->Length());
		GDK_THREADS_ENTER();
	} else {
		printf("No Playlist data found\n");
	}
}

// Called when a file has been selected to be loaded as a playlist
void load_list_ok(GtkWidget *, gpointer data)
{
  PlaylistWindowGTK *playlist_window_gtk = (PlaylistWindowGTK *)data;
	//gtk_widget_hide(GTK_WIDGET(playlist_window_gtk->load_list));
	Playlist *playlist = playlist_window_gtk->GetPlaylist();
	enum plist_result loaderr;

	gchar *current_dir =
		g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(playlist_window_gtk->load_list)));

	int marker = strlen(current_dir)-1;
	while (marker > 0 && current_dir[marker] != '/') // Get rid of the filename
		current_dir[marker--] = '\0';
	prefs_set_string(ap_prefs, "gtk_interface", "default_playlist_load_path", current_dir);
	g_free(current_dir);

	std::string file(gtk_file_selection_get_filename(
			GTK_FILE_SELECTION(playlist_window_gtk->load_list)));
	GDK_THREADS_LEAVE();
	loaderr = playlist->Load(file, playlist->Length(), false);
	GDK_THREADS_ENTER();
	if(loaderr == E_PL_DUBIOUS) {
		// FIXME - pop up a dialog and check if we really want to load
		alsaplayer_error("Dubious whether file is a playlist - trying anyway");
		GDK_THREADS_LEAVE();
		loaderr = playlist->Load(file, playlist->Length(), true);
		GDK_THREADS_ENTER();
	}
	if(loaderr) {
		// FIXME - pass playlist_window to this routine
	}
}

void save_list_ok(GtkWidget *, gpointer data)
{
	PlaylistWindowGTK *playlist_window_gtk = (PlaylistWindowGTK *)data;
	gtk_widget_hide(GTK_WIDGET(playlist_window_gtk->save_list));
	Playlist *playlist = playlist_window_gtk->GetPlaylist();

	gchar *current_dir =
		g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(playlist_window_gtk->save_list)));

	int marker = strlen(current_dir)-1;
	while (marker > 0 && current_dir[marker] != '/') // Get rid of the filename
		current_dir[marker--] = '\0';
	prefs_set_string(ap_prefs, "gtk_interface", "default_playlist_save_path", current_dir);

	std::string file(gtk_file_selection_get_filename(
			GTK_FILE_SELECTION(playlist_window_gtk->save_list)));
	enum plist_result saveerr;
	saveerr = playlist->Save(file, PL_FORMAT_M3U);
	if(saveerr) {
		// FIXME - pass playlist_window to this routine
	} else {
		// FIXME - pass playlist_window to this routine
	}
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


void playlist_window_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	PlaylistWindowGTK *playlist_window_gtk = (PlaylistWindowGTK *) data;
	Playlist *playlist = NULL;
	GtkWidget *list = NULL;

	playlist = playlist_window_gtk->GetPlaylist();
	list = playlist_window_gtk->GetPlaylist_list();
	

	//alsaplayer_error("Key pressed!");
	switch(event->keyval) {
		case GDK_Insert:
			dialog_popup(widget, (gpointer)
				playlist_window_gtk->add_file);
			break;	
		case GDK_Delete:
			playlist_remove(widget, data);
			break;
		case GDK_Return:
			playlist_play_current(playlist, list);
			break;
		case GDK_Right:
			// This is a hack, but quite legal
			ap_set_position_relative(global_session_id, 10);
			break;
		case GDK_Left:
			ap_set_position_relative(global_session_id, -10);
			break;
		default:
			break;
	}		
}


void dialog_popup(GtkWidget *, gpointer data)
{
        GtkWidget *dialog = (GtkWidget *)data;
        gtk_widget_show(dialog);
} 



void key_press_event(GtkWidget *, GdkEvent *event, gpointer data)
{
	//printf("Key down\n");
}


void button_press_event(GtkWidget *widget, GdkEvent *, gpointer data)
{
	gtk_widget_grab_focus(widget);
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





static GtkWidget *init_playlist_window(PlaylistWindowGTK *playlist_window_gtk, Playlist *playlist)
{
	GtkWidget *playlist_window;
	GtkWidget *working;
	GtkWidget *list;
	GtkWidget *list_status;
	GtkWidget *status;
	GtkWidget *toplevel;
	GtkStyle *style;
	GdkFont *bold_font;

	bold_font = gdk_font_load("-adobe-helvetica-bold-r-normal--12-*-*-*-*-*-*-*");
	if (!bold_font)
		assert ((bold_font = gdk_fontset_load("fixed")) != NULL);

	playlist_window = create_playlist_window();
	toplevel = gtk_widget_get_toplevel(playlist_window);
	list = get_widget(playlist_window, "playlist");
	gtk_object_set_data(GTK_OBJECT(list), "window", playlist_window);
	gtk_object_set_data(GTK_OBJECT(playlist_window), "list", list);
	gtk_object_set_data(GTK_OBJECT(playlist_window), "Playlist", playlist);
	list_status = get_widget(playlist_window, "playlist_status");
	status = gtk_label_new("");
	style = gtk_style_copy(gtk_widget_get_style(status));
	gdk_font_unref(style->font);
	style->font = bold_font;
	gdk_font_ref(style->font);
	gtk_widget_set_style(GTK_WIDGET(status), style);
	gtk_widget_show(status);
	gtk_box_pack_start(GTK_BOX(list_status), status, true, false, 1);
	
	gtk_object_set_data(GTK_OBJECT(list), "status", status);
	
	style = gtk_style_copy(gtk_widget_get_style(list));
	gtk_widget_set_style(GTK_WIDGET(list), style);	

	gtk_clist_set_column_width(GTK_CLIST(list), 0, 16);
	gtk_clist_set_column_max_width(GTK_CLIST(list), 0, 16);

	gtk_clist_set_column_min_width(GTK_CLIST(list), 1, 250);

	gtk_clist_set_column_width(GTK_CLIST(list), 2, 24);
	gtk_clist_set_column_max_width(GTK_CLIST(list), 2, 24);

	gtk_clist_set_row_height(GTK_CLIST(list), 20);

	gtk_signal_connect(GTK_OBJECT(playlist_window), "configure_event",
		GTK_SIGNAL_FUNC(list_resize), list);

	gtk_signal_connect(GTK_OBJECT(playlist_window), "destroy",
		GTK_SIGNAL_FUNC(playlist_delete_event), (gpointer)playlist_window_gtk);
	gtk_signal_connect(GTK_OBJECT(playlist_window), "delete_event",
		GTK_SIGNAL_FUNC(playlist_delete_event), (gpointer)playlist_window_gtk);
	playlist_window_gtk->add_file = gtk_file_selection_new("Add files or URLs");
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(playlist_window_gtk->add_file), prefs_get_string(ap_prefs, "gtk_interface", "default_playlist_add_path", "/"));

playlist_window_gtk->load_list = gtk_file_selection_new("Load Playlist");
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(playlist_window_gtk->load_list), prefs_get_string(ap_prefs, "gtk_interface", "default_playlist_load_path", "/"));

	playlist_window_gtk->save_list = gtk_file_selection_new("Save Playlist");
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(playlist_window_gtk->save_list), prefs_get_string(ap_prefs, "gtk_interface", "default_playlist_save_path", "/"));
	GtkCList *file_list = GTK_CLIST(GTK_FILE_SELECTION(playlist_window_gtk->add_file)->file_list);
	gtk_clist_set_selection_mode(file_list, GTK_SELECTION_EXTENDED);

	gtk_object_set_data(GTK_OBJECT(playlist_window_gtk->add_file), "list", list);
	gtk_object_set_data(GTK_OBJECT(playlist_window_gtk->save_list), "list", list);
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(playlist_window_gtk->add_file));
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(playlist_window_gtk->load_list));
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(playlist_window_gtk->save_list));

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(playlist_window_gtk->add_file)->cancel_button),
		"clicked", GTK_SIGNAL_FUNC(dialog_cancel), (gpointer)playlist_window_gtk->add_file);
 	gtk_signal_connect(GTK_OBJECT(playlist_window_gtk->add_file), "delete_event",
                GTK_SIGNAL_FUNC(dialog_delete), NULL);

	// Shortcut keys
	gtk_signal_connect(GTK_OBJECT(playlist_window), "key-press-event",
		GTK_SIGNAL_FUNC(playlist_window_keypress), 
		(gpointer)playlist_window_gtk);	

	// Modify button text of add_file dialog
	gtk_object_set(GTK_OBJECT(
		GTK_FILE_SELECTION(playlist_window_gtk->add_file)->ok_button),
		"label", "Add", NULL);
	gtk_object_set(GTK_OBJECT(
		GTK_FILE_SELECTION(playlist_window_gtk->add_file)->cancel_button),
		"label", "Close", NULL);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(playlist_window_gtk->add_file)->ok_button),
    	"clicked", GTK_SIGNAL_FUNC(add_file_ok), (gpointer)playlist_window_gtk);

	// Modify button text of load_list dialog
	gtk_object_set(GTK_OBJECT(
		GTK_FILE_SELECTION(playlist_window_gtk->load_list)->cancel_button),
		"label", "Close", NULL);
	gtk_object_set(GTK_OBJECT(
		GTK_FILE_SELECTION(playlist_window_gtk->load_list)->ok_button),
		"label", "Load", NULL);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(playlist_window_gtk->load_list)->cancel_button),
		"clicked", GTK_SIGNAL_FUNC(dialog_cancel), (gpointer)playlist_window_gtk->load_list);
	gtk_signal_connect(GTK_OBJECT(playlist_window_gtk->load_list), "delete_event",
		GTK_SIGNAL_FUNC(dialog_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(playlist_window_gtk->load_list)->ok_button),
		"clicked", GTK_SIGNAL_FUNC(load_list_ok), (gpointer)playlist_window_gtk);

	// MOdify button text of save_list dialog
	gtk_object_set(GTK_OBJECT(
		GTK_FILE_SELECTION(playlist_window_gtk->save_list)->cancel_button),
		"label", "Close", NULL);
	gtk_object_set(GTK_OBJECT(
		GTK_FILE_SELECTION(playlist_window_gtk->save_list)->ok_button),
		"label", "Save", NULL);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(playlist_window_gtk->save_list)->cancel_button),
		"clicked", GTK_SIGNAL_FUNC(dialog_cancel), (gpointer)playlist_window_gtk->save_list);
	gtk_signal_connect(GTK_OBJECT(playlist_window_gtk->save_list), "delete_event",
		GTK_SIGNAL_FUNC(dialog_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(playlist_window_gtk->save_list)->ok_button),
		"clicked", GTK_SIGNAL_FUNC(save_list_ok), (gpointer)playlist_window_gtk);

	working = get_widget(playlist_window, "shuffle_button");
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
		GTK_SIGNAL_FUNC(shuffle_cb), playlist);
	working = get_widget(playlist_window, "add_button");
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
		GTK_SIGNAL_FUNC(dialog_popup), (gpointer)playlist_window_gtk->add_file);
	working = get_widget(playlist_window, "clear_button");
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
		GTK_SIGNAL_FUNC(clear_cb), playlist);
	gtk_signal_connect(GTK_OBJECT(list), "key_press_event",
		GTK_SIGNAL_FUNC(key_press_event), list);
	gtk_signal_connect(GTK_OBJECT(list), "button_press_event",
		GTK_SIGNAL_FUNC(button_press_event), list);
	gtk_signal_connect(GTK_OBJECT(list), "select_row",
		GTK_SIGNAL_FUNC(playlist_click), playlist);
	working = get_widget(playlist_window, "del_button");
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
                GTK_SIGNAL_FUNC(playlist_remove),
								(gpointer)playlist_window_gtk);

	working = get_widget(playlist_window, "close_button");
	 gtk_signal_connect(GTK_OBJECT(working), "clicked",
				GTK_SIGNAL_FUNC(close_cb), (gpointer)playlist_window_gtk);

	working = get_widget(playlist_window, "save_button");
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
			GTK_SIGNAL_FUNC(dialog_popup), (gpointer)playlist_window_gtk->save_list);
	working = get_widget(playlist_window, "load_button");
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
			GTK_SIGNAL_FUNC(dialog_popup), (gpointer)playlist_window_gtk->load_list);

	// Setup drag & drop
	gtk_drag_dest_set(toplevel,
			GTK_DEST_DEFAULT_ALL,
			drag_types,
			n_drag_types,
			GDK_ACTION_COPY);
	gtk_signal_connect(GTK_OBJECT(toplevel), "drag_data_received",
			GTK_SIGNAL_FUNC(dnd_drop_event), NULL);

	
	gtk_widget_grab_focus(GTK_WIDGET(list));

	return playlist_window;
}
