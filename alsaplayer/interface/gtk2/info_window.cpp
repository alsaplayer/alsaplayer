/*  info_window.cpp
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

#include "info_window.h"

#ifdef ENABLE_NLS
#define _(String) gettext(String)
#define N_(String) noop_gettext(String)
#else
#define _(String) (String)
#define N_(String) String
#endif
                                            
static GtkWidget*
create_info_window()
{
	GtkWidget *frame;
	GtkWidget *main_box;
	GtkWidget *speed_label;
	GtkWidget *balance_label;
	GtkWidget *title_label;
	GtkWidget *format_label;
	GtkWidget *volume_label;
	GtkWidget *position_label;
	
	frame = gtk_frame_new(NULL);
	main_box = gtk_layout_new(NULL, NULL);

	g_object_set_data(G_OBJECT(frame), "layout", main_box);
	gtk_container_add(GTK_CONTAINER(frame), main_box);
	
	speed_label = gtk_label_new(NULL);
	g_object_set_data(G_OBJECT(frame), "speed_label", speed_label);
	gtk_layout_put(GTK_LAYOUT(main_box), speed_label, 2, 0);
		
	balance_label = gtk_label_new(NULL);
	g_object_set_data(G_OBJECT(frame), "balance_label", balance_label);
	gtk_layout_put(GTK_LAYOUT(main_box), balance_label, 0, 0);
	
	title_label = gtk_label_new(NULL);
	g_object_set_data(G_OBJECT(frame), "title_label", title_label);
	gtk_layout_put(GTK_LAYOUT(main_box), title_label, 0, 0);
	
	format_label = gtk_label_new(NULL);
	g_object_set_data(G_OBJECT(frame), "format_label", format_label);
	gtk_layout_put(GTK_LAYOUT(main_box), format_label, 0, 0);
	
	volume_label = gtk_label_new(NULL);
	g_object_set_data(G_OBJECT(frame), "volume_label", volume_label);
	gtk_layout_put(GTK_LAYOUT(main_box), volume_label, 0, 0);
	
	position_label = gtk_label_new(NULL);
	g_object_set_data(G_OBJECT(frame), "position_label", position_label);
	gtk_layout_put(GTK_LAYOUT(main_box), position_label, 0, 25);
	
	return frame;	
}

InfoWindow::InfoWindow()
{
	window = create_info_window();
	volume = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "volume_label"));
	balance = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "balance_label"));
	position = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "position_label"));
	title = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "title_label"));
	format = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "format_label"));
	speed = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "speed_label"));
	layout = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "layout"));
		
	leftwidth = 0;
	rightwidth = 0;
	labelheight = 0;
		
}

InfoWindow::~InfoWindow()
{
	gtk_widget_destroy(window);
}

void InfoWindow::set_volume(const gchar *text)
{
	gtk_label_set_text (GTK_LABEL(volume), text);
}

void InfoWindow::set_balance(const gchar *text)
{
	gtk_label_set_text (GTK_LABEL(balance), text);
}
void InfoWindow::set_positions()
{
	gint x, y, width, height;
	
	if ((labelheight < 2) || (leftwidth < 2) || (rightwidth < 2) || (labelheight != volume->allocation.height)) {
		leftwidth = (speed->allocation.width > balance->allocation.width)? speed->allocation.width:balance->allocation.width;
		rightwidth = (volume->allocation.width > position->allocation.width)? volume->allocation.width:position->allocation.width;
		labelheight = volume->allocation.height;
		
		gtk_widget_set_size_request(window, -1, labelheight * 2 + labelheight / 3);
	}
	
	width = layout->allocation.width;
	height = layout->allocation.height;
	
	//speed has fixed position
	// 2 px padding
	x = 2;
	y = height - labelheight;
	gtk_layout_move(GTK_LAYOUT(layout), balance, x, y);
	
	x = leftwidth + labelheight;
	y = 0;
	gtk_widget_set_size_request (title, width - x - rightwidth - labelheight, -1);
	gtk_layout_move(GTK_LAYOUT(layout), title, x, y);

	x = leftwidth + labelheight;
	y = height - labelheight;
	gtk_widget_set_size_request (format, width - x - rightwidth - labelheight, -1);
	gtk_layout_move(GTK_LAYOUT(layout), format, x, y);
	
	x = width - volume->allocation.width -2;
	y = 0;
	gtk_layout_move(GTK_LAYOUT(layout), volume, x, y);
	
	x = width - position->allocation.width - 2;
	y = height - labelheight;
	gtk_layout_move(GTK_LAYOUT(layout), position, x, y);
	
}

void InfoWindow::set_position(const gchar *text)
{
	gtk_label_set_text (GTK_LABEL(position), text);
}

void InfoWindow::set_title(const gchar *text)
{
	gtk_label_set_text (GTK_LABEL(title), text);
}

void InfoWindow::set_format(const gchar *text)
{
	gtk_label_set_text (GTK_LABEL(format), text);
}

void InfoWindow::set_speed(const gchar *text)
{
	gtk_label_set_text (GTK_LABEL(speed), text);
}

void InfoWindow::set_background_color(const gchar* str)
{
	GdkColor color;
	
	if (!gdk_color_parse(str, &color))
		return;

	gtk_widget_modify_bg(layout, GTK_STATE_NORMAL, &color);
}

void InfoWindow::set_font_color(const gchar* str)
{
	GdkColor color;
	
	if (!gdk_color_parse(str, &color))
		return;
	
	gtk_widget_modify_fg(volume, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_fg(position, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_fg(title, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_fg(format, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_fg(speed, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_fg(balance, GTK_STATE_NORMAL, &color);
}
