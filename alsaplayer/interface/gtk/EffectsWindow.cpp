/*  EffectsWindow.cpp
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
#include "EffectsWindow.h"
#include "support.h"
#include "gladesrc.h"
#include "gtk_interface.h"

extern int global_effects_show;
extern int global_reverb_delay;
extern int global_reverb_feedback;

void effects_scale_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkAdjustment *adj;
	gint *val;

	val = (gint *)data;
	
	adj = GTK_RANGE(widget)->adjustment;

	*val = (gint) adj->value;
}


void effects_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
        gint x, y;

        gdk_window_get_origin(widget->window, &x, &y);
        if (windows_x_offset >= 0) {
                x -= windows_x_offset;
                y -= windows_y_offset;
        }	
        gtk_widget_hide(widget);
        gtk_widget_set_uposition(widget, x, y);
        global_effects_show = 0;
}


void reverb_off(GtkWidget *widget, gpointer data)
{
	GtkWidget *feedback_scale;
	GtkAdjustment *adj;

	feedback_scale = get_widget(GTK_WIDGET(data), "feedback_scale");
	adj = GTK_RANGE(feedback_scale)->adjustment;
        gtk_adjustment_set_value(adj, 0.0);
        global_reverb_feedback = 0;
}


GtkWidget *init_effects_window()
{
	GtkWidget *effects_window;

	effects_window = create_effects_window();
	
	// Close/delete signals
	gtk_signal_connect(GTK_OBJECT(effects_window), "destroy",
                GTK_SIGNAL_FUNC(effects_delete_event), NULL);
	gtk_signal_connect(GTK_OBJECT(effects_window), "delete_event",
                GTK_SIGNAL_FUNC(effects_delete_event), NULL);
	return effects_window;
}
