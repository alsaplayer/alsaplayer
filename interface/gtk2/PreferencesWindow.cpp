/*  PreferencesWindow.cpp
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

#include "PreferencesWindow.h"
#include "info_window.h"
#include "PlaylistWindow.h"
#include "prefs.h"

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

static void
pref_dialog_accept(GtkWidget *dialog, GtkWidget *main_window)
{
	InfoWindow *info_window = (InfoWindow *) g_object_get_data(G_OBJECT(main_window), "info_window");
	PlaylistWindow *pl = (PlaylistWindow *) g_object_get_data(G_OBJECT(main_window), "playlist_window");

	/*	general	*/
	GtkWidget *bg = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "pref_general_bg_colour_button"));
	GtkWidget *fg = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "pref_general_fg_colour_button"));
	GtkWidget *font = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "pref_general_fg_font_button"));
	GdkColor color;
	gchar *str;
	
	gtk_color_button_get_color(GTK_COLOR_BUTTON(bg), &color);
	str = gtk_color_selection_palette_to_string(&color, 1);
	prefs_set_string(ap_prefs, "gtk2_interface", "background_colour", str);
	info_window->set_background_color(str);
	g_free(str);
	
	gtk_color_button_get_color(GTK_COLOR_BUTTON(fg), &color);
	str = gtk_color_selection_palette_to_string(&color, 1);
	prefs_set_string(ap_prefs, "gtk2_interface", "font_colour", str);
	info_window->set_font_color(str);
	g_free(str);
	
	str = (gchar *) gtk_font_button_get_font_name(GTK_FONT_BUTTON(font)); // don't free str !
	prefs_set_string(ap_prefs, "gtk2_interface", "fonts", str); 
	info_window->set_fonts(str);
	/*	end	*/
	
	/*	play	*/
	GtkWidget *start = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "pref_play_on_start"));;
	GtkWidget *add = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "pref_play_on_add"));
	GtkWidget *title = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "pref_play_on_title"));
	gboolean what;
	
	what = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(start)); 
	prefs_set_bool(ap_prefs, "main", "play_on_start", what);
	
	what = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(add));
	prefs_set_bool(ap_prefs, "gtk2_interface", "play_on_add", what);
	pl->play_on_add = what;
	
	what = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title)); 
	prefs_set_bool(ap_prefs, "gtk2_interface", "play_on_title", what);
	if (!what)
		gtk_window_set_title(GTK_WINDOW(main_window), "AlsaPlayer");
}

static void
pref_dialog_hide(GtkWidget *dialog)
{
	if (GTK_WIDGET_VISIBLE(dialog))
		gtk_widget_hide_all(dialog);
}

static void
pref_dialog_response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
	if (arg1 == GTK_RESPONSE_REJECT)
		pref_dialog_hide(GTK_WIDGET(dialog));
	else if (arg1 == GTK_RESPONSE_ACCEPT)
		pref_dialog_accept(GTK_WIDGET(dialog), GTK_WIDGET(user_data));
	else if (arg1 == GTK_RESPONSE_OK) {
		pref_dialog_accept(GTK_WIDGET(dialog), GTK_WIDGET(user_data));
		pref_dialog_hide(GTK_WIDGET(dialog));
	}
} 

static GtkWidget*
play_tab(GtkWidget *dialog)
{
	GtkWidget *pref_play;
	GtkWidget *pref_play_on_start;
	GtkWidget *pref_play_on_add;
	GtkWidget *pref_play_on_title;
	
	pref_play = gtk_vbox_new(FALSE, 0);
	
	pref_play_on_start = gtk_check_button_new_with_label(_("Play on start"));	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pref_play_on_start), prefs_get_bool(ap_prefs, "main", "play_on_start", FALSE));
	g_object_set_data(G_OBJECT(dialog), "pref_play_on_start", pref_play_on_start);
	gtk_box_pack_start(GTK_BOX(pref_play), pref_play_on_start, FALSE, FALSE, 0);
	
	pref_play_on_add = gtk_check_button_new_with_label(_("Play song after adding to playlist"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pref_play_on_add), prefs_get_bool(ap_prefs, "gtk2_interface", "play_on_add", FALSE));
	g_object_set_data(G_OBJECT(dialog), "pref_play_on_add", pref_play_on_add); 
	gtk_box_pack_start(GTK_BOX(pref_play), pref_play_on_add, FALSE, FALSE, 0);
	
	pref_play_on_title = gtk_check_button_new_with_label(_("Show title in title-bar"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pref_play_on_title), prefs_get_bool(ap_prefs, "gtk2_interface", "play_on_title", FALSE));
	g_object_set_data(G_OBJECT(dialog), "pref_play_on_title", pref_play_on_title); 
	gtk_box_pack_start(GTK_BOX(pref_play), pref_play_on_title, FALSE, FALSE, 0);
	
	return pref_play;	
}

static GtkWidget*
general_tab(GtkWidget *dialog)
{
	GtkWidget *pref_general;
	GtkWidget *pref_general_bg_colour;
	GtkWidget *pref_general_bg_colour_button;
	GtkWidget *pref_general_fg_colour;
	GtkWidget *pref_general_fg_colour_button;
	GtkWidget *pref_general_fg_font;
	GtkWidget *pref_general_fg_font_button;
	GtkWidget *label;
	const char *str;
	GdkColor color;

	pref_general = gtk_vbox_new(FALSE, 0);
	
	pref_general_bg_colour = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pref_general), pref_general_bg_colour, FALSE, FALSE, 0); 
	label = gtk_label_new(_("Background color"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(pref_general_bg_colour), label, TRUE, TRUE, 3);
	
	str = prefs_get_string(ap_prefs, "gtk2_interface", "background_colour", "#000000");
	if (!gdk_color_parse(str, &color)) {
		color.red = 0;
		color.green = 0;
		color.blue = 0;
	}
	pref_general_bg_colour_button = gtk_color_button_new_with_color(&color);
	g_object_set_data(G_OBJECT(dialog), "pref_general_bg_colour_button", pref_general_bg_colour_button); 
	gtk_box_pack_start(GTK_BOX(pref_general_bg_colour), pref_general_bg_colour_button, FALSE, FALSE, 0);
	
	pref_general_fg_colour = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pref_general), pref_general_fg_colour, FALSE, FALSE, 0);
	label = gtk_label_new(_("Font color"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(pref_general_fg_colour), label, TRUE, TRUE, 3);
	
	str = prefs_get_string(ap_prefs, "gtk2_interface", "font_colour", "#ffffff");
	if (!gdk_color_parse(str, &color)) {
		color.red = 255;
		color.green = 255;
		color.blue = 255;
	}
	pref_general_fg_colour_button = gtk_color_button_new_with_color(&color);
	g_object_set_data(G_OBJECT(dialog), "pref_general_fg_colour_button", pref_general_fg_colour_button);
	gtk_box_pack_start(GTK_BOX(pref_general_fg_colour), pref_general_fg_colour_button, FALSE, FALSE, 0);
	
	pref_general_fg_font = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pref_general), pref_general_fg_font, FALSE, FALSE, 0);
	label = gtk_label_new(_("Fonts"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(pref_general_fg_font), label, TRUE, TRUE, 3);
	
	str = prefs_get_string(ap_prefs, "gtk2_interface", "fonts", "");
	pref_general_fg_font_button = gtk_font_button_new_with_font(str);
	g_object_set_data(G_OBJECT(dialog), "pref_general_fg_font_button", pref_general_fg_font_button);
	gtk_box_pack_start(GTK_BOX(pref_general_fg_font), pref_general_fg_font_button, FALSE, FALSE, 0);

	return pref_general;	
}
                                                 
GtkWidget *init_preferences_window(GtkWidget *main_window)
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *label;
	GtkWidget *tab;
	
	dialog = gtk_dialog_new_with_buttons(_("Preferences"), GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT,
											GTK_STOCK_OK,
											GTK_RESPONSE_OK,
											GTK_STOCK_APPLY,
											GTK_RESPONSE_ACCEPT,
											GTK_STOCK_CANCEL,
											GTK_RESPONSE_REJECT,
											NULL);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 300);
											
	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), notebook);
	
	tab = general_tab(dialog);
	label = gtk_label_new(_("General"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, label);
	
	tab = play_tab(dialog);
	label = gtk_label_new(_("Play"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, label);
	
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(pref_dialog_response), (gpointer) main_window);
	g_signal_connect(G_OBJECT(dialog), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	
	return dialog;
}
