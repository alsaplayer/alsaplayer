/*  StatusIcon.cpp
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

#include "StatusIcon.h"

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

/*	put nice and shiny status icon here or below this is example only	*/
#include "pixmaps/note.xpm"

static void
window_show_hide(GtkStatusIcon *status_icon, gpointer user_data)
{
	GtkWidget *win = (GtkWidget *) user_data;

	if (GTK_WIDGET_VISIBLE(win)){
			gtk_widget_hide(win);
		}
	else 
		gtk_widget_show_all(win);
}

gboolean
status_icon_create(GtkWidget *main_window)
{
	GdkPixbuf *icon;
	GtkStatusIcon *icon_status;
	
	icon = gdk_pixbuf_new_from_xpm_data((const char **)note_xpm);
	
	icon_status = gtk_status_icon_new_from_pixbuf (icon);
	g_object_unref(G_OBJECT(icon));
	
	g_signal_connect(G_OBJECT(icon_status), "activate", G_CALLBACK(window_show_hide), main_window);
	
	return gtk_status_icon_is_embedded (icon_status);
}
