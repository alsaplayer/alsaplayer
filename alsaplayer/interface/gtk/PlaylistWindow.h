/*  PlaylistWindow.h
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

#ifndef __PlaylistWindow_h__
#define __PlaylistWindow_h__
#include "Playlist.h"
#include <gtk/gtk.h>

extern void playlist_window_gtk_prev(GtkWidget *, gpointer);
extern void playlist_window_gtk_next(GtkWidget *, gpointer);

class PlaylistWindowGTK : public virtual PlaylistInterface
{
	private:
		Playlist * playlist;
		GtkWidget * playlist_window;  // The window containing the list
		GtkWidget * playlist_list;    // The list itself
		GtkLabel * playlist_status;   // Label giving status of list
		bool showing;
	public:
		PlaylistWindowGTK(Playlist *);
		~PlaylistWindowGTK();

		GtkWidget *add_file;
		GtkWidget *save_list;
		GtkWidget *load_list;
		
		// Callbacks called by playlist when its state changes
		void CbSetCurrent(unsigned);
		void CbInsert(std::vector<PlayItem> &, unsigned);
		void CbRemove(unsigned, unsigned);
		void CbClear();

		// Other methods
		Playlist *GetPlaylist() { return playlist; } 
		GtkWidget *GetPlaylist_list() { return playlist_list; }
		void GiveStatus(std::string status); // Display a status message
		void Show();  // Show the playlist window
		void Hide();  // Hide the playlist window
		void ToggleVisible();  // Show / Hide the playlist window
};

#endif
