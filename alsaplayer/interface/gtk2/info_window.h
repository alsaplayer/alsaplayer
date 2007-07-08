/*  info_window.h
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

#ifndef INFO_WINDOW_H_
#define INFO_WINDOW_H_

#include <cstdio>
#include <gtk/gtk.h>

class InfoWindow
{
	private:
		GtkWidget *window;
		GtkWidget *volume;
		GtkWidget *balance;
		GtkWidget *title;
		GtkWidget *format;
		GtkWidget *speed;
		GtkWidget *position;
		GtkWidget *layout;
		gint leftwidth;
		gint rightwidth;
		gint labelheight;
	public:
		InfoWindow();
		~InfoWindow();
		
		GtkWidget *GetWindow() {return window;}
		
		// set new values to show
		void set_volume(const gchar*);
		void set_balance(const gchar*);
		void set_title(const gchar*);
		void set_format(const gchar*);
		void set_speed(const gchar*);
		void set_position(const gchar*);
		
		//set new positions 
		void set_positions();
		
		//set apperance
		void set_background_color(const gchar*);
		void set_font_color(const gchar*);
		void set_fonts(const gchar*);
};
#endif /*INFO_WINDOW_H_*/
