/*  main.cpp - GTK2 main function
 *  Copyright (C) 2004 Andy Lo A Foe <andy@alsaplayer.org>
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
 *
 *
 *  $Id$
 *
 *  VIM: set ts=8
 *
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

// 222, 232, 198
// 218, 210, 190

#define BG_RED		180
#define BG_GREEN	180
#define BG_BLUE		180

#define INFO_TEXT_H	8000
#define TITLE_TEXT_H	9000

#define ROOT_WINDOW_H	88

#include <gtk/gtk.h>
#include <stdio.h>
#include <set>
#include <string>
#include "interface.h"
#include "support.h"
#include "scopes.h"
#include "internal_scope.h"
#include "interface_plugin.h"
#include "alsaplayer_error.h"
#include "AlsaPlayer.h"
#include "Playlist.h"
#include "control.h"
#include "playlist.h"

#include "pixmaps/new_prev.xpm"
#include "pixmaps/new_next.xpm"
#include "pixmaps/new_play.xpm"
#include "pixmaps/new_stop.xpm"

#define TARGET_URI_LIST	0x0001

static int vol_range[] = { 0,1,2,4,5,7,12,18,26,35,45,56,69,83,100 };

static GtkWidget *display = NULL;
static GdkPixmap *display_pixmap = NULL;
static GtkLabel *time_label = NULL;
static GtkLabel *format_label = NULL;
static GtkLabel *status_label = NULL;
static GtkWidget *pos_scale = NULL;
static GtkWidget *vol_scale = NULL;
static bool vol_pressed = false;
static bool pos_pressed = false;
static bool pos_changed = false;
static gfloat new_position = 0.0;
static GdkColor display_bg;
static GdkColor display_fg1;
static GdkColor display_fg2;
static GdkColor display_fg3;
static PangoLayout *info_layout = NULL;
static char time_string[1024];
static char status_string[1024];
static char format_string[1024];
static GdkRectangle display_rectangle;
static GdkRectangle position_rectangle;
static GdkGC *display_gc = NULL;
static GdkPixmap *misc_slider_pixmap = NULL;
static PangoLayout *misc_slider_layout = NULL;
static coreplayer_notifier notifier;

static GtkTargetEntry drag_types[] = {
	{"text/uri-list", 0, TARGET_URI_LIST}
};
static int n_drag_types = sizeof(drag_types)/sizeof(drag_types[0]);


void notifier_lock()
{
	GDK_THREADS_ENTER();
}


void notifier_unlock()
{
	gdk_flush();
	GDK_THREADS_LEAVE();
}


void stop_notify(void *data)
{
}


void volume_changed(void *data, float new_vol)
{
}


void speed_changed(void *data, float new_speed)
{
}


gboolean escape_string(char *str, int maxlen)
{
	std::string tmp = std::string(str);
	std::string::size_type pos = 0;
	std::string::size_type loc = 0;

	//alsaplayer_error("in string: %s", str);
	
	// replace &
	while ((pos = tmp.find("&", loc)) != std::string::npos) {
		loc = pos+1; // next iteration start
		tmp.replace(pos, 1, "&amp;");
		loc++;
	}
	strncpy(str, tmp.c_str(), maxlen);

	//alsaplayer_error("out string: %s", str);
	
	return true;
	
}


GtkWidget *xpm_label_box(gchar * xpm_data[], GtkWidget *to_win)
{
	GtkWidget *box1;
	GtkWidget *pixmapwid;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkStyle *style;

	box1 = gtk_hbox_new(false, 0);
	gtk_container_border_width(GTK_CONTAINER(box1), 0);

	style = gtk_widget_get_style(to_win);

	pixmap = gdk_pixmap_create_from_xpm_d(to_win->window, &mask, &style->bg[
			GTK_STATE_NORMAL], xpm_data);
	pixmapwid = gtk_pixmap_new(pixmap, mask);

	gtk_box_pack_start(GTK_BOX(box1), pixmapwid, true, false, 1);

	gtk_widget_show(pixmapwid);

	return (box1);
}


gboolean pos_release_event(GtkWidget *widget, GdkEventButton *, gpointer data)
{
	Playlist *pl;
	CorePlayer *p;
	GtkAdjustment *adj;

	if (data) {
		pl = (Playlist *)data;
		p = pl->GetCorePlayer();
		if (pos_changed) {
			adj = GTK_RANGE(widget)->adjustment;
			p->Seek((int)adj->value);
		}	
		pos_pressed = false;
		pos_changed = false;
	}	
	return false;
		
}
gboolean vol_release_event(GtkWidget *widget, GdkEventButton *, gpointer data)
{
	vol_pressed = false;
	return false;
}



gboolean vol_press_event(GtkWidget *widget, GdkEventButton *, gpointer)
{
	vol_pressed = true;
	return false;
}
gboolean vol_value_changed_event(GtkWidget *widget, gpointer data)
{
	Playlist *pl;
	CorePlayer *p;
	GtkAdjustment *adj;
	int pos, min, sec, idx;

	adj = GTK_RANGE(widget)->adjustment;
	if (vol_pressed && data) {
		pl = (Playlist *)data;
		p = pl->GetCorePlayer();
		adj = GTK_RANGE(widget)->adjustment;
		idx = (int)adj->value;
		idx = (idx < 0) ? 0 : ((idx > 14) ? 14 : idx);
		GDK_THREADS_LEAVE();
		p->SetVolume((float)vol_range[idx] / 100.0);
		GDK_THREADS_ENTER();

	}
	return false;
}

gboolean pos_press_event(GtkWidget *widget, GdkEventButton *, gpointer)
{
	pos_pressed = true;
	return false;
}
gboolean pos_value_changed_event(GtkWidget *widget, gpointer data)
{
	Playlist *pl;
	CorePlayer *p;
	GtkAdjustment *adj;
	int pos, min, sec, t_min, t_sec;

	adj = GTK_RANGE(widget)->adjustment;
	
	if (pos_pressed && data) {
		pl = (Playlist *)data;
		p = pl->GetCorePlayer();
		pos = p->GetCurrentTime((int)adj->value);
		min = pos / 6000;
		sec = (pos % 6000) / 100;
		t_sec = p->GetCurrentTime(p->GetFrames());
		t_min = t_sec / 6000;
		t_sec = (t_sec % 6000) / 100;
		sprintf(time_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\">Seek to %02d:%02d / %02d:%02d</span>", INFO_TEXT_H, min, sec, t_min, t_sec);
		if (time_label)
			gtk_label_set_markup(time_label, time_string);
		pos_changed = true;
	}
	return false;
}



void position_notify(void *data, int frame)
{
	if (data && !pos_pressed) {
		Playlist *l = (Playlist *)data;
		CorePlayer *p = l->GetCorePlayer();
		float secs;
		int curr;
		int nr_frames;
		
		if (p) {
			char *s;
			stream_info info;
			unsigned int c_min, c_sec, t_min, t_sec, len;
			bool playing = p->IsPlaying();
			bool streaming = false;

			c_min = c_sec = t_min = t_sec = len = 0;

			nr_frames = p->GetFrames();

			if (nr_frames >= 0) 
				secs = (float)p->GetCurrentTime(nr_frames);
			else {
				secs = -1.0;
				streaming = true;
			}
			curr = p->GetCurrentTime(frame);
			if (!p->GetStreamInfo(&info)) {
			} else {
				escape_string(info.title, AP_TITLE_MAX);
				escape_string(info.artist, AP_ARTIST_MAX);
			}	

			notifier_lock();
			
			if (pos_scale) {
				GtkAdjustment *adj = GTK_RANGE(pos_scale)->adjustment;
				adj->lower = 0;
				adj->upper = nr_frames > 16 ? nr_frames - 16 : 0;
				if (!playing || streaming) {
					gtk_adjustment_set_value(adj, 0);
					gtk_widget_set_sensitive(pos_scale, false);
				} else {
					gtk_widget_set_sensitive(pos_scale, true);
					gtk_adjustment_set_value(adj, frame);
				}	
			}	
			
			if (secs == 0.0) {
				c_min = c_sec = t_min = t_sec = 0;
			} else {
				c_min = curr / 6000;
				c_sec = (curr % 6000) / 100;
				t_min = (int)secs / 6000;
				t_sec = ((int)secs % 6000) / 100;
			}
			if (!playing) {
				sprintf(time_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\"></span>", INFO_TEXT_H);
			} else 	if (!streaming) {
				sprintf(time_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\">%02d:%02d / %02d:%02d</span>", INFO_TEXT_H, c_min, c_sec, t_min, t_sec);
			} else {
				sprintf(time_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\">%02d:%02d / streaming</span>", INFO_TEXT_H, c_min, c_sec);
			}
			if (playing) {	
				if (strlen(info.stream_type)) {
					sprintf(format_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\">%s</span>", INFO_TEXT_H, info.stream_type);
				}	
				if (!strlen(info.artist) && strlen(info.title)) {
					sprintf(status_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\">Now playing: %s</span>", TITLE_TEXT_H, info.title);
				} else {
					sprintf(status_string,
						"<span font_family=\"Arial\" foreground=\"black\" size=\"%d\">Now playing: %s - %s</span>", TITLE_TEXT_H,
						strlen(info.artist) ? info.artist : "Unkown Artist",
						strlen(info.title) ? info.title : "Unkown Title");
				}		
			} else {
				sprintf(status_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\">No stream loaded</span>", TITLE_TEXT_H);
				sprintf(format_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\"> </span>", INFO_TEXT_H);
				sprintf(time_string, "<span font_family=\"Arial\" foreground=\"black\" size=\"%d\"> </span>", INFO_TEXT_H);
				c_min = c_sec = t_min = t_sec = 0;
			}

			if (status_label)
				gtk_label_set_markup(status_label, status_string);
			if (time_label)
				gtk_label_set_markup(time_label, time_string);
			if (format_label)
				gtk_label_set_markup(format_label, format_string);
			if (status_label)
				gtk_label_set_markup(status_label, status_string);
			gdk_flush();
			notifier_unlock();
			
		}	
	}
}


gint misc_slider_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	if (misc_slider_pixmap) {
		g_object_unref(misc_slider_pixmap);
	}
	misc_slider_pixmap = gdk_pixmap_new(widget->window,
		widget->allocation.width, widget->allocation.height, -1);
	misc_slider_layout = gtk_widget_create_pango_layout(widget, "");
	pango_layout_set_markup(misc_slider_layout,
		"<span foreground=\"black\" size=\"7000\">100%</span>", -1);

	return true;	
}


gint misc_slider_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	GdkPixmap *the_pixmap = misc_slider_pixmap;
	gdk_draw_rectangle(the_pixmap,
		widget->style->bg_gc[GTK_STATE_NORMAL],
		true,
		0, 0,
		widget->allocation.width,
		widget->allocation.height);
	
	gdk_draw_rectangle(the_pixmap,
		widget->style->fg_gc[GTK_STATE_NORMAL],
		false,
		4, 2,
		widget->allocation.width-8,
		widget->allocation.height-4);

	gdk_draw_layout(the_pixmap,
		widget->style->fg_gc[GTK_STATE_NORMAL],
		widget->allocation.width/2 - 10, 5,
		misc_slider_layout);
	
	gdk_draw_pixmap(widget->window,
		widget->style->fg_gc[GTK_STATE_NORMAL],
		the_pixmap,
		event->area.x, event->area.y,
		event->area.x, event->area.y,
		event->area.width, event->area.height);
	return false;	
}


gint display_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	GdkColor bg, fg;

	if (display_pixmap) {
		g_object_unref(display_pixmap);
	}
	if (display_gc) {
		gdk_gc_unref(display_gc);
	}
	
	display_pixmap = gdk_pixmap_new(widget->window,
		widget->allocation.width, widget->allocation.height, -1);

	display_rectangle.x = 0;
	display_rectangle.y = 0;
	display_rectangle.width = widget->allocation.width;
	display_rectangle.height = widget->allocation.height;
	
	position_rectangle.x = 4;
	position_rectangle.y = 1;
	position_rectangle.width = widget->allocation.width - 8;
	position_rectangle.height = 12;

	// Allocate some colors
	display_gc = gdk_gc_new(widget->window);
	display_fg1.red = BG_RED << 8;
	display_fg1.green = BG_GREEN << 8;
	display_fg1.blue = BG_BLUE << 8;
	display_fg2.red = (BG_RED+4) << 8;
	display_fg2.green = (BG_GREEN+4) << 8;
	display_fg2.blue = (BG_BLUE+4) << 8;
	display_fg3.red = (BG_RED-40) << 8;
	display_fg3.green = (BG_GREEN-40) << 8;
	display_fg3.blue = (BG_BLUE-40) << 8;
	display_bg.red = display_bg.green = display_bg.blue = 0 << 8;
	
	gdk_colormap_alloc_color(gdk_colormap_get_system(), &display_bg, false, true);
	gdk_colormap_alloc_color(gdk_colormap_get_system(), &display_fg1, false, true);
	gdk_colormap_alloc_color(gdk_colormap_get_system(), &display_fg2, false, true);
	gdk_colormap_alloc_color(gdk_colormap_get_system(), &display_fg3, false, true);
	
	gdk_gc_set_background(display_gc, &display_bg);

	return true;	
}

gint display_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	GdkPixmap *the_pixmap = display_pixmap;
	int info_w, info_h;
	int time_w, time_h;
#if 1
	//Display clear
	gdk_draw_rectangle(the_pixmap,
		widget->style->bg_gc[GTK_STATE_NORMAL],
		true,
		display_rectangle.x,
		display_rectangle.y,
		display_rectangle.width,
		display_rectangle.height);
#endif	
	// Final render
	gdk_draw_pixmap(widget->window,
		widget->style->fg_gc[GTK_STATE_NORMAL],
		the_pixmap,
		event->area.x, event->area.y,
		event->area.x, event->area.y,
		event->area.width, event->area.height);
	return false;	
}


int interface_gtk2_init()
{
	sprintf(time_string, "");
	sprintf(status_string, "");
	sprintf(format_string, "");
	
	memset(&notifier, 0, sizeof(notifier));
	return 1;
}

int interface_gtk2_running()
{
	return 1;
}


int interface_gtk2_stop()
{
	GDK_THREADS_ENTER();
	gdk_flush();
	gtk_exit(0); // This is *NOT* clean :-(
	GDK_THREADS_LEAVE();
	return 1;
}


void interface_gtk2_close()
{
	return;
}


gint dnd_drop_event(GtkWidget *widget,
		GdkDragContext   * context,
		gint              x,
		gint              y,
		GtkSelectionData *selection_data,
		guint             info,
		guint             *time,
		void *data)
{
	switch(info) {
		case TARGET_URI_LIST:
			alsaplayer_error("TARGET_URI_LIST drop (%d,%d)", x, y);
			break;
		default:
			alsaplayer_error("Unkown drop!");
			break;
	}		
}


gboolean root_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	Playlist *playlist = (Playlist *)data;

	if (playlist) {
		GDK_THREADS_LEAVE();
		playlist->UnRegisterNotifier(&notifier);
		GDK_THREADS_ENTER();
	}	
	gdk_flush();

	gtk_main_quit();
	// Never reached
	return FALSE;
}


void playlist_button_cb(GtkWidget *widget, gpointer data)
{
	alsaplayer_error("button pressed...");	
}

void next_button_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	if (pl) {
		GDK_THREADS_LEAVE();
		pl->Pause();
		pl->Next();
		pl->UnPause();
		GDK_THREADS_ENTER();
	}
}


void stop_button_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p;
	
	if (pl) {
		p = pl->GetCorePlayer();
		GDK_THREADS_LEAVE();
		pl->Pause();
		p->Stop();
		p->Close();
		GDK_THREADS_ENTER();
	}	
			
}

void prev_button_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	if (pl) {
		GDK_THREADS_LEAVE();
		pl->Pause();
		pl->Prev();
		pl->UnPause();
		GDK_THREADS_ENTER();
	}
}


static GtkWidget *label_box(gchar *label_text) {
	GtkWidget *box;
	GtkWidget *label;

	/* Create box for label */
	box = gtk_hbox_new(FALSE, 0);

	/* Create label */
	label = gtk_label_new("");
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
	gtk_widget_show(label);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.0);

	return box;
}		


int interface_gtk2_start(Playlist *playlist, int argc, char **argv)
{
	GtkWidget *root_window = NULL;
	GtkWidget *playlist_window  = NULL;
        GtkTreeView *tree_view  = NULL;
	GtkWidget *toplevel  = NULL;
	GtkWidget *working  = NULL;
	GtkWidget *misc_slider  = NULL;
	GtkWidget *pix  = NULL;
	GdkGeometry geom;
	
	g_thread_init(NULL);

	if (!g_thread_supported()) {
		printf("Sorry, this interface requires working threads.\n");
		return 1;
	}	
	gdk_threads_init();

	gtk_set_locale();
	gtk_init(&argc, &argv);
	gdk_rgb_init();

	playlist_window = create_playlist_window ();
	gtk_widget_show(playlist_window);
	
	root_window = create_root_window ();
	gtk_widget_show (root_window);

	toplevel = gtk_widget_get_toplevel(root_window);
	geom.min_width = 326;
	geom.min_height = geom.max_height = ROOT_WINDOW_H;
	geom.max_width = 1600;
	gtk_window_set_geometry_hints(GTK_WINDOW(toplevel),
		GTK_WIDGET(root_window), &geom,
		(GdkWindowHints)(GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE));

	time_label = GTK_LABEL(lookup_widget(root_window, "time_label"));
	format_label = GTK_LABEL(lookup_widget(root_window, "format_label"));
	status_label = GTK_LABEL(lookup_widget(root_window, "status_label"));
                
	if (time_label) {
		gtk_label_set_use_markup(time_label, 1);
	}	
	if (format_label) {
		gtk_label_set_use_markup(format_label, 1);
	}	
	if (status_label) {
		gtk_label_set_use_markup(status_label, 1);
	}	
	if (working = lookup_widget(root_window, "vol_scale")) {
		vol_scale = working;
		g_signal_connect(G_OBJECT(working), "value_changed",
			G_CALLBACK(vol_value_changed_event), playlist);
		g_signal_connect(G_OBJECT(working), "button_press_event",
			G_CALLBACK(vol_press_event), playlist);
		g_signal_connect(G_OBJECT(working), "button_release_event",
			G_CALLBACK(vol_release_event), playlist);
	}	
	if (working = lookup_widget(root_window, "pos_scale")) {
		pos_scale = working;
		g_signal_connect(G_OBJECT(working), "value_changed",
			G_CALLBACK(pos_value_changed_event), playlist);
		g_signal_connect(G_OBJECT(working), "button_press_event",
			G_CALLBACK(pos_press_event), playlist);
		g_signal_connect(G_OBJECT(working), "button_release_event",
			G_CALLBACK(pos_release_event), playlist);	
	}
#if 0	
	if (working = lookup_widget(root_window, "vis_box")) {
		display = gtk_drawing_area_new();
		gtk_drawing_area_size(GTK_DRAWING_AREA(display), 72,28);
		gtk_widget_show(display);

		gtk_signal_connect(GTK_OBJECT(display), "configure_event",
			(GtkSignalFunc) display_configure, &display);
		//gtk_signal_connect(GTK_OBJECT(display), "expose_event",
		//	(GtkSignalFunc) display_expose, &display);

		gtk_box_pack_start(GTK_BOX(working), display,
			false, true, 0);
	}
#endif	
	if (working = lookup_widget(root_window, "prev_button")) {
		gtk_signal_connect(GTK_OBJECT(working), "clicked",
			GTK_SIGNAL_FUNC(prev_button_cb), 
			playlist);
		pix = xpm_label_box(new_prev_xpm, root_window);
		if (pix) {
			gtk_widget_show(pix);
			gtk_container_add(GTK_CONTAINER(working), pix);
		}	
	}
	if (working = lookup_widget(root_window, "playlist_button")) {
		GtkWidget *l;

		l = gtk_label_new("");
		gtk_label_set_use_markup(GTK_LABEL(l), true);
		gtk_label_set_markup(GTK_LABEL(l), "<span font_family=\"Arial\" foreground=\"black\" size=\"9000\">playlist</span>");

		gtk_widget_show(l);
		gtk_container_add(GTK_CONTAINER(working), l);
	}	
	if (working = lookup_widget(root_window, "stop_button")) {
		 gtk_signal_connect(GTK_OBJECT(working), "clicked",
				 GTK_SIGNAL_FUNC(stop_button_cb),
				 playlist);
		pix = xpm_label_box(new_stop_xpm, root_window);
		if (pix) {
			gtk_widget_show(pix);
			gtk_container_add(GTK_CONTAINER(working), pix);
		}	

	}
	if (working = lookup_widget(root_window, "play_button")) {
		pix = xpm_label_box(new_play_xpm, root_window);
		if (pix) {
			gtk_widget_show(pix);
			gtk_container_add(GTK_CONTAINER(working), pix);
		}	

	}
	if (working = lookup_widget(root_window, "playlist_button")) {
		gtk_signal_connect(GTK_OBJECT(working), "clicked",
			GTK_SIGNAL_FUNC(playlist_button_cb),
			playlist);
	}	
	if (working = lookup_widget(root_window, "next_button")) {
		gtk_signal_connect(GTK_OBJECT(working), "clicked",
			GTK_SIGNAL_FUNC(next_button_cb),
			playlist);
		pix = xpm_label_box(new_next_xpm, root_window);
		if (pix) {
			gtk_widget_show(pix);
			gtk_container_add(GTK_CONTAINER(working), pix);
		}	
	}		
	// Connect some signals
	gtk_signal_connect(GTK_OBJECT(root_window), "delete_event",
		GTK_SIGNAL_FUNC(root_window_delete), playlist);

	
	
	gdk_flush();

	notifier.speed_changed = speed_changed;
	notifier.volume_changed = volume_changed;
	notifier.position_notify = position_notify;
	notifier.stop_notify = stop_notify;
	playlist->RegisterNotifier(&notifier, playlist);

        // Register playlist
        tree_view = (GtkTreeView *)lookup_widget(playlist_window, "tree_view");
        if (tree_view) {
                playlist_register(playlist, tree_view);
        } 
	// Setup drag & drop 
	gtk_drag_dest_set(toplevel,
		GTK_DEST_DEFAULT_ALL,
		drag_types,
		n_drag_types,
		GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(toplevel), "drag_data_received",
		G_CALLBACK(dnd_drop_event), NULL);

#if 0
	gdk_window_set_decorations(GTK_WIDGET(root_window)->window,
		(GdkWMDecoration)0);
#endif
	init_scopes(playlist->GetNode());
	register_scope(&internal_scope, true, display);

	gdk_threads_enter();
	gdk_flush();
	gtk_main ();
	gdk_threads_leave();

	unregister_scopes();

	//playlist->UnRegisterNotifier(&notifier);
	return 0;
}


#ifdef NO_MAIN_FUNCTION


interface_plugin default_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	"GTK2 interface v0.0",
	"Andy Lo A Foe",
	NULL,
	interface_gtk2_init,
	interface_gtk2_start,
	interface_gtk2_running,
	interface_gtk2_stop,
	interface_gtk2_close
};


extern "C" {

	interface_plugin *interface_plugin_info()
	{
		return &default_plugin;
	}

}


#endif

