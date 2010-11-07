/*  AboutWindow.cpp
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

#include "AboutWindow.h"

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

#ifndef VERSION
#define VERSION NULL
#endif

void
about_dialog_show(GtkWidget *aboutdialog)
{
	if (!GTK_WIDGET_VISIBLE(aboutdialog))
		gtk_widget_show(aboutdialog);
}

gboolean
about_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if (GTK_WIDGET_VISIBLE(widget))
		gtk_widget_hide(widget);
	
	return TRUE;	
}

void
about_response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
	if (arg1 == GTK_RESPONSE_CANCEL)
		about_delete_event(GTK_WIDGET(dialog), NULL, NULL);
} 
  
#include "pixmaps/logo.xpm"
                                                                                                   
GtkWidget*
init_about_window(GtkWidget *main_window)
{
	GtkWidget *about_window = NULL;
	GdkPixbuf *about_logo = NULL;
	const gchar *authors[] = {"Andy Lo A Foe\t\t\t<andy@alsaplayer.org>", 
								"Dominique Michel\t\t<dominique@tuxfamilly.org>",
								"Madej",
								"\nPatches:",
								"Hubert Chan",
								"Viktor Radnai and Paul Brossier",
								"\nCheck AUTHORS file for a more complete list",
								NULL};
//	const gchar *artists[] = {"", NULL};
	const gchar *documenters[] = {"Yvo Timmermans", 
									"Dominique Michel",
									"Andy Lo A Foe",
									NULL};
	const gchar *license = _("AlsaPlayer is free software; you can redistribute it and/or modify\n\
it under the terms of the GNU General Public Licence as published by\n\
the Free Software Foundation; either version 3 of the Licence, or\n\
(at your option) any later version.\n\
\n\
AlsaPlayer is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public Licence for more details.\n\
\n\
You should have received a copy of the GNU General Public Licence\n\
along with AlsaPlayer; if not, see <http://www.gnu.org/licenses/>.");
	
	about_logo = gdk_pixbuf_new_from_xpm_data((const char **)logo_xpm);

	about_window = gtk_about_dialog_new();
	
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(about_window), "AlsaPlayer");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_window), VERSION);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_window), "Copyright © 1998-2010");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about_window), _("No comments yet"));
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about_window), license);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about_window), "www.alsaplayer.org");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about_window), authors);
//	gtk_about_dialog_set_artists(GTK_ABOUT_DIALOG(about_window), artists);
	gtk_about_dialog_set_documenters(GTK_ABOUT_DIALOG(about_window), documenters);
	gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about_window), _("translator-credits"));
	gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(about_window), about_logo);

	g_signal_connect(G_OBJECT(about_window), "delete-event", G_CALLBACK(about_delete_event), NULL);
	g_signal_connect(G_OBJECT(about_window), "response", G_CALLBACK(about_response), NULL);
	
	g_object_unref(G_OBJECT(about_logo));
	
	return about_window;
}

