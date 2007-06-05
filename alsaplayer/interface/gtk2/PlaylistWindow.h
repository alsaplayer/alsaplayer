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

GtkWidget* create_filechooser(GtkWindow *main_window, Playlist *playlist);

void playlist_play_current(Playlist *playlist, GtkWidget *list);
void playlist_remove(GtkWidget *, gpointer user_data);

class PlaylistWindow
{
	private:
		playlist_interface pli;
		Playlist * playlist;
		GtkWidget *window;
		GtkWidget *list;
		gint width;
		gint height;
		pthread_mutex_t playlist_list_mutex;
	public:
		PlaylistWindow(Playlist *);
		~PlaylistWindow();

		GtkWidget *GetWindow() { return window; }
		void LoadPlaylist();
		void SavePlaylist();
		Playlist *GetPlaylist() { return playlist; }
		void Show();
		void Hide();
		bool IsHidden() { return (bool)!GTK_WIDGET_VISIBLE(window); }
		void Clear();
		GtkWidget *GetList() { return list; }
		void Play(int number);
		void PlayPrev();
		void PlayNext();
		
		gint GetWidth() { return width; }
		gint GetHeight() { return height; }
		
//		GtkWidget *add_file;
//		GtkWidget *save_list;
//		GtkWidget *load_list;
		
		// Callbacks called by playlist when its state changes
		static void CbSetCurrent(void *,unsigned);
		static void CbInsert(void *,std::vector<PlayItem> &, unsigned);
		static void CbUpdated(void *,PlayItem &, unsigned);
		static void CbRemove(void *, unsigned, unsigned);
		static void CbLock(void *);
		static void CbUnlock(void *);
		static void CbClear(void *);

		// Other methods
		
		 
//		void Show();  // Show the playlist window
//		void Hide();  // Hide the playlist window
//		void ToggleVisible();  // Show / Hide the playlist window
};

#endif
