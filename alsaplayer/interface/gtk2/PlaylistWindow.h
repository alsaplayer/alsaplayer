/*  PlaylistWindow.h
 *  Copyright (C) 1999 Andy Lo A Foe <andy@alsa-project.org>
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

#ifndef __PlaylistWindow_h__
#define __PlaylistWindow_h__
#include "Playlist.h"
#include <gtk/gtk.h>

extern void playlist_window_gtk_prev(GtkWidget *, gpointer);
extern void playlist_window_gtk_next(GtkWidget *, gpointer);

GtkWidget* create_filechooser(GtkWindow *main_window, Playlist *playlist);

void dialog_popup(GtkWidget *, gpointer data);
void playlist_remove(GtkWidget *, gpointer data);
void playlist_play_current(Playlist *playlist, GtkWidget *list);

class PlaylistWindowGTK
{
	private:
		playlist_interface pli;
		Playlist * playlist;
		GtkWidget * playlist_window;  // The window containing the list
		GtkWidget * playlist_list;    // The list itself
		GtkLabel * playlist_status;   // Label giving status of list
		pthread_mutex_t playlist_list_mutex; // Mutex for list
		bool showing;
	public:
		PlaylistWindowGTK(Playlist *, GtkWidget *);
		~PlaylistWindowGTK();

		GtkWidget *add_file;
		GtkWidget *save_list;
		GtkWidget *load_list;
		
		// Callbacks called by playlist when its state changes
		static void CbSetCurrent(void *,unsigned);
		static void CbInsert(void *,std::vector<PlayItem> &, unsigned);
		static void CbUpdated(void *,PlayItem &, unsigned);
		static void CbRemove(void *, unsigned, unsigned);
		static void CbLock(void *);
		static void CbUnlock(void *);
		static void CbClear(void *);

		// Other methods
		Playlist *GetPlaylist() { return playlist; } 
		GtkWidget *GetPlaylist_list() { return playlist_list; }
		void Show();  // Show the playlist window
		void Hide();  // Hide the playlist window
		void ToggleVisible();  // Show / Hide the playlist window
};

#endif
