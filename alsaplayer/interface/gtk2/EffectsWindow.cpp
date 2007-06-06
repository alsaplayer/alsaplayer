/*  EffectsWindow.cpp
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
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
#include "EffectsWindow.h"
#include "gtk_interface.h"

extern int global_reverb_delay;
extern int global_reverb_feedback;

void effects_scale_cb(GtkWidget *widget, GdkEvent *, gpointer data)
{
	GtkAdjustment *adj;
	gint *val;

	val = (gint *)data;
	
	adj = GTK_RANGE(widget)->adjustment;

	*val = (gint) adj->value;
}


void effects_delete_event(GtkWidget *widget, GdkEvent *, gpointer data)
{
        gint x, y;
       	static gint e_windows_x_offset = 0;
	static gint e_windows_y_offset = 0;

	gdk_window_get_origin(widget->window, &x, &y);
	if (windows_x_offset >= 0) {
                x -= e_windows_x_offset;
                y -= e_windows_y_offset;
        }	
        gtk_widget_hide(widget);
        gtk_widget_set_uposition(widget, x, y);
}


void reverb_off(GtkWidget *, gpointer data)
{
	GtkWidget *feedback_scale;
	GtkAdjustment *adj;

	feedback_scale = (GtkWidget *) g_object_get_data(G_OBJECT(data), "feedback_scale");
	adj = GTK_RANGE(feedback_scale)->adjustment;
        gtk_adjustment_set_value(adj, 0.0);
        global_reverb_feedback = 0;
}


GtkWidget*
create_effects_window (void)
{
  GtkWidget *effects_window;
  GtkWidget *vbox23;
  GtkWidget *hbox31;
  GtkWidget *plugin_list_box;
  GtkWidget *effects_list;
  GtkWidget *label16;
  GtkWidget *label17;
  GtkWidget *parameter_box;
  GtkWidget *button_box;
  GtkWidget *ok_button;

  effects_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (effects_window), "effects_window", effects_window);
  gtk_window_set_default_size (GTK_WINDOW(effects_window), 500, 300);
  gtk_window_set_title (GTK_WINDOW (effects_window), "Effects");

  vbox23 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox23);
  g_object_set_data_full (G_OBJECT (effects_window), "vbox23", vbox23,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox23);
  gtk_container_add (GTK_CONTAINER (effects_window), vbox23);

  hbox31 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox31);
  g_object_set_data_full (G_OBJECT (effects_window), "hbox31", hbox31,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox31);
  gtk_box_pack_start (GTK_BOX (vbox23), hbox31, TRUE, TRUE, 0);

  plugin_list_box = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (plugin_list_box);
  g_object_set_data_full (G_OBJECT (effects_window), "plugin_list_box", plugin_list_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (plugin_list_box);
  gtk_box_pack_start (GTK_BOX (hbox31), plugin_list_box, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (plugin_list_box), 8);

  effects_list = gtk_ctree_new (2, 0);
  gtk_widget_ref (effects_list);
  g_object_set_data_full (G_OBJECT (effects_window), "effects_list", effects_list,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (effects_list);
  gtk_box_pack_start (GTK_BOX (plugin_list_box), effects_list, TRUE, TRUE, 0);
  gtk_clist_set_column_width (GTK_CLIST (effects_list), 0, 80);
  gtk_clist_set_column_width (GTK_CLIST (effects_list), 1, 80);
  gtk_clist_column_titles_hide (GTK_CLIST (effects_list));

  label16 = gtk_label_new ("label16");
  gtk_widget_ref (label16);
  g_object_set_data_full (G_OBJECT (effects_window), "label16", label16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label16);
  gtk_clist_set_column_widget (GTK_CLIST (effects_list), 0, label16);

  label17 = gtk_label_new ("label17");
  gtk_widget_ref (label17);
  g_object_set_data_full (G_OBJECT (effects_window), "label17", label17,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label17);
  gtk_clist_set_column_widget (GTK_CLIST (effects_list), 1, label17);

  parameter_box = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (parameter_box);
  g_object_set_data_full (G_OBJECT (effects_window), "parameter_box", parameter_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (parameter_box);
  gtk_box_pack_start (GTK_BOX (hbox31), parameter_box, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (parameter_box), 8);

  button_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (button_box);
  g_object_set_data_full (G_OBJECT (effects_window), "button_box", button_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_box);
  gtk_box_pack_start (GTK_BOX (vbox23), button_box, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (button_box), 8);

  ok_button = gtk_button_new_with_label ("OK");
  gtk_widget_ref (ok_button);
  g_object_set_data_full (G_OBJECT (effects_window), "ok_button", ok_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (ok_button);
  gtk_box_pack_end (GTK_BOX (button_box), ok_button, FALSE, TRUE, 0);

  return effects_window;
}


GtkWidget *init_effects_window()
{
	GtkWidget *effects_window;

	effects_window = create_effects_window();
	
	// Close/delete signals
	g_signal_connect(G_OBJECT(effects_window), "destroy",
                G_CALLBACK(effects_delete_event), NULL);
	g_signal_connect(G_OBJECT(effects_window), "delete_event",
                G_CALLBACK(effects_delete_event), NULL);
	return effects_window;
}
