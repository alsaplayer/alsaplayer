/*  gtk_interface.cpp - gtk+ callbacks, etc
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#include "AlsaPlayer.h"
#include "config.h"
#include "prefs.h"
#include "alsaplayer_error.h"
#include <unistd.h>
#include <sys/types.h>
//#define NEW_SCALE
//#define TESTING
//#define SUBSECOND_DISPLAY 

#include <algorithm>
#include "utilities.h"

// Include things needed for gtk interface:
#include <gtk/gtk.h>
#include <glib.h>

#include "support.h"
#include "gladesrc.h"
#include "pixmaps/f_play.xpm"
#include "pixmaps/r_play.xpm"
#include "pixmaps/pause.xpm"
#include "pixmaps/next.xpm"
#include "pixmaps/prev.xpm"
#include "pixmaps/stop.xpm"
#include "pixmaps/volume_icon.xpm"
#include "pixmaps/balance_icon.xpm"
#if 0
#include "pixmaps/eject.xpm"
#endif
#include "pixmaps/play.xpm"
#include "pixmaps/playlist.xpm"
#include "pixmaps/cd.xpm"
#include "pixmaps/menu.xpm"

#include "PlaylistWindow.h"

// Include other things: (ultimate aim is to wrap this up into one nice file)
#include "CorePlayer.h"
#include "Playlist.h"
#include "EffectsWindow.h"
#include "Effects.h"
#include "ScopesWindow.h"
Playlist *playlist = NULL;

// Defines
#ifdef SUBSECOND_DISPLAY 
#define UPDATE_TIMEOUT  20000
#else
#define UPDATE_TIMEOUT  100000
#endif
#define BAL_CENTER  100
#define UPDATE_COUNT    5
#define MIN_BAL_TRESH   BAL_CENTER-10   // Center is a special case
#define MAX_BAL_TRESH   BAL_CENTER+10   // so we build in some slack
#define ZERO_PITCH_TRESH 2

// Global variables (get rid of these too... ;-) )
static int global_update = 1;

static int global_draw_volume = 1;
static GtkWidget *play_pix;
static GdkPixmap *val_ind = NULL;
static gint global_rb = 1;
static PlaylistWindowGTK *playlist_window_gtk = NULL;

/* These are used to contain the size of the window manager borders around
   our windows, and are used to show/hide windows in the same positions. */
gint windows_x_offset = -1;
gint windows_y_offset = -1;

static gint main_window_x = 150;
static gint main_window_y = 175;

typedef struct  _update_struct {
	gpointer data;
	GtkWidget *drawing_area;
	GtkWidget *vol_scale;
	GtkWidget *bal_scale;
	GtkWidget *pos_scale;
	GtkWidget *speed_scale;
} update_struct;

update_struct global_ustr;

// Static variables  (to be moved into a class, at some point)
static GtkWidget *play_dialog;
static pthread_t indicator_thread;

gint global_effects_show = 0;
gint global_scopes_show = 0;

static int vol_scale[] = {
				0,1,2,4,7,12,18,26,35,45,56,69,83,100 };

#ifdef SUBSECOND_DISPLAY
#define INDICATOR_WIDTH 80
#else
#define INDICATOR_WIDTH 64
#endif

////////////////////////
// Callback functions //
////////////////////////


gint indicator_callback(gpointer data, int locking);



gboolean main_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkFunction f = (GtkFunction)data;
	global_update = -1;
	//alsaplayer_error("Going to wait for indicator_thread join");
	GDK_THREADS_LEAVE(); // Drop lock so indicator_thread can finish
	pthread_join(indicator_thread, NULL);
	GDK_THREADS_ENTER();
	//alsaplayer_error("About to delete playlist_window_gtk");
	if (playlist_window_gtk)
		delete playlist_window_gtk;
	//alsaplayer_error("About to run f()");
	if (f) { // Oh my, a very ugly HACK indeed! But it works
		GDK_THREADS_LEAVE();
		f(NULL);
		GDK_THREADS_ENTER();
	}
	//alsaplayer_error("About to call gtk_main_quit()");
	gtk_main_quit();
	
	// Never reached
	return FALSE;
}


void press_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	global_update = 0;
}

void draw_volume();

void volume_move_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	draw_volume();
}

void draw_title(char *title)
{
	update_struct *ustr = &global_ustr;
	GdkRectangle update_rect;
	static char old_title[128] = "";
	static int count = UPDATE_COUNT;

	if (count-- > 0 && strcmp(old_title, title) == 0)
		return;
	else {
		count = UPDATE_COUNT;
		if (strlen(title) > 127) {
			strncpy(old_title, title, 126);
			old_title[127] = 0;
		} else
		strcpy(old_title, title);
	}		
	update_rect.x = 82;
	update_rect.y = 0;
	update_rect.width = ustr->drawing_area->allocation.width - 82;
	update_rect.height = 18;

	if (val_ind) {	
			// Clear area
			gdk_draw_rectangle(val_ind,
							ustr->drawing_area->style->black_gc,
							true, update_rect.x, update_rect.y, update_rect.width,
							update_rect.height);
			// Draw string
			gdk_draw_string(val_ind, ustr->drawing_area->style->font,
							ustr->drawing_area->style->white_gc, update_rect.x+6,
							update_rect.y+14, title);
			// Do the drawing
			gtk_widget_draw (ustr->drawing_area, &update_rect);	
	}
}

void draw_format(char *format)
{
	update_struct *ustr = &global_ustr;
	GdkRectangle update_rect;
	static char old_format[128] = "";
	static int count = UPDATE_COUNT;

	if (count-- > 0 && strcmp(old_format, format) == 0) 
		return;
	else {
		count = UPDATE_COUNT;
		if (strlen(format) > 126) {
			strncpy(old_format, format, 126);
			old_format[127] = 0;
		} else
		strcpy(old_format, format);
	}

	update_rect.x = 82;
	update_rect.y = 16;
	update_rect.width = ustr->drawing_area->allocation.width - 82 - INDICATOR_WIDTH;  
	update_rect.height = 18;

	if (val_ind) {
			// Clear area
			gdk_draw_rectangle(val_ind,
							ustr->drawing_area->style->black_gc,
							true, update_rect.x, update_rect.y, update_rect.width,
							update_rect.height);
			// Draw string
			gdk_draw_string(val_ind, ustr->drawing_area->style->font,
							ustr->drawing_area->style->white_gc, update_rect.x+6,
							update_rect.y+12, format);
			// Do the drawing
			gtk_widget_draw (ustr->drawing_area, &update_rect);
	}
}


void draw_volume()
{
	update_struct *ustr = &global_ustr;
	GtkAdjustment *adj;
	Playlist *pl = (Playlist *)ustr->data;
	CorePlayer *p = pl->GetCorePlayer();
	GdkRectangle update_rect;
	char str[60];
	static int old_vol = -1;
	static int count = UPDATE_COUNT;

	if (!ustr->vol_scale)
		return;
#ifdef NEW_SCALE	
	adj = GTK_BSCALE(ustr->vol_scale)->adjustment;
#else	
	adj = GTK_RANGE(ustr->vol_scale)->adjustment;
#endif
	int val = (int)GTK_ADJUSTMENT(adj)->value;

	if (count-- > 0 && val == old_vol)
		return;
	else {
		count = UPDATE_COUNT;
		old_vol = val;
		//p->SetVolume(val);
	}	
	int idx = val;
  idx = (idx < 0) ? 0 : ((idx > 13) ? 13 : idx);
	val = vol_scale[idx];

	val ? sprintf(str, "Volume: %d%%  ", val) : sprintf(str, "Volume: mute");

	update_rect.x = 0;
	update_rect.y = 16;
	update_rect.width = 82;
	update_rect.height = 16;
	if (val_ind) {	
			gdk_draw_rectangle(val_ind,
							ustr->drawing_area->style->black_gc,
							true, update_rect.x, update_rect.y, update_rect.width, update_rect.height);
			gdk_draw_string(val_ind, ustr->drawing_area->style->font,
							ustr->drawing_area->style->white_gc, update_rect.x+6, update_rect.y+12, str);
			gtk_widget_draw (ustr->drawing_area, &update_rect);
	}		
}

void draw_balance();


void balance_move_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	global_draw_volume = 0;
	draw_balance();
}


void balance_release_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	global_draw_volume = 1;
}


void draw_balance()
{
	update_struct *ustr = &global_ustr;
	GdkRectangle update_rect;
	Playlist *pl = (Playlist *)ustr->data;
	CorePlayer *p = pl->GetCorePlayer();
	char str[60];
	int pan, left, right;

	pan = p->GetPan();
	if (pan < 0) {
		sprintf(str, "Pan: left %d%%", - pan);
	} else if (pan > 0) {
		sprintf(str, "Pan: right %d%%", pan);
	} else {
		sprintf(str, "Pan: center");
	} 
	update_rect.x = 0;
	update_rect.y = 16;
	update_rect.width = 82; 
	update_rect.height = 18;
	if (val_ind) {
			gdk_draw_rectangle(val_ind,
							ustr->drawing_area->style->black_gc,
							true, update_rect.x, update_rect.y, 
							update_rect.width, update_rect.height);
			gdk_draw_string(val_ind,
							ustr->drawing_area->style->font,
							ustr->drawing_area->style->white_gc,
							update_rect.x+6, update_rect.y+12,
							str);
			gtk_widget_draw (ustr->drawing_area, &update_rect);
	}		
}

void draw_speed();

void speed_move_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	draw_speed();
}

void draw_speed()
{
	update_struct *ustr = &global_ustr;
	GtkAdjustment *adj;
	GdkRectangle update_rect;
	char str[60];
	int speed_val;
	static int old_val = -20000;
	static int count = UPDATE_COUNT;
	static int pause_blink =  2000000;
	static int pause_active = 1;
#if 1
	adj = GTK_RANGE(ustr->speed_scale)->adjustment;
#else
	adj = GTK_BSCALE(ustr->speed_scale)->adjustment;
#endif	

	speed_val = (int)GTK_ADJUSTMENT(adj)->value;
	if (count-- > 0 && speed_val == old_val) 
		return;
	count = UPDATE_COUNT;	
	old_val = speed_val;	
	if (speed_val < ZERO_PITCH_TRESH && speed_val > -ZERO_PITCH_TRESH) {
#if 0
		if ((pause_blink -= (UPDATE_TIMEOUT * UPDATE_COUNT)) < 0) {
			pause_blink = 200000;
			pause_active = 1 - pause_active;
		}
		sprintf(str, "Speed: %s", pause_active ? "paused" : "");
#else
		sprintf(str, "Speed: pause");
#endif
	}
	else
		sprintf(str, "Speed: %d%%  ", (int)GTK_ADJUSTMENT(adj)->value);
	update_rect.x = 0; 
	update_rect.y = 0;
	update_rect.width = 82;
	update_rect.height = 16;
	if (val_ind) {
			gdk_draw_rectangle(val_ind,
							ustr->drawing_area->style->black_gc,
							true,
							update_rect.x, update_rect.y,
							update_rect.width,
							update_rect.height);
			gdk_draw_string(val_ind,
							ustr->drawing_area->style->font,
							ustr->drawing_area->style->white_gc,
							update_rect.x+6, update_rect.y+14,
							str);
			gtk_widget_draw (ustr->drawing_area, &update_rect);		
	}
}	


void val_release_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	update_struct *ustr = &global_ustr;
	GdkRectangle update_rect;

	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = 106;
	update_rect.height = 20;
	if (val_ind) {
			gdk_draw_rectangle(val_ind,
							ustr->drawing_area->style->black_gc,
							true,
							0, 0,
							ustr->drawing_area->allocation.width-64,
							20);
			gtk_widget_draw (ustr->drawing_area, &update_rect);
	}
}


void release_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkAdjustment *adj;
	update_struct *ustr = &global_ustr;
	Playlist *pl = (Playlist *)ustr->data;
	CorePlayer *p = pl->GetCorePlayer();

	adj = GTK_RANGE(widget)->adjustment;
	p->Seek((int)adj->value);
	global_update = 1;	
}


void move_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	indicator_callback(data, 0);
}

void speed_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();
	float val =  GTK_ADJUSTMENT(widget)->value;
	if (val < ZERO_PITCH_TRESH && val > -ZERO_PITCH_TRESH)
		val = 0;
	p->SetSpeed(  (float) val / 100.0 );
	draw_speed();
}

pthread_t smoother_thread;
pthread_mutex_t smoother_mutex = PTHREAD_MUTEX_INITIALIZER;
static float destination = 100.0;


void smoother(void *data)
{
	GtkAdjustment *adj = (GtkAdjustment *)data;
	float temp, cur_val;
	int done = 0;

	if (pthread_mutex_trylock(&smoother_mutex) != 0) {
		pthread_exit(NULL);
	}
	
	nice(5);
	
	if (adj) {
		//alsaplayer_error("going from %.2f to %.2f",
		//	adj->value, destination);
		cur_val = adj->value;
		while (!done) {
			temp = cur_val - destination;
			if (temp < 0.0) temp = -temp;
			if (temp <= 2.5) {
				done = 1;
				continue;
			}	
			if (cur_val < destination) {
				GDK_THREADS_ENTER();
				gtk_adjustment_set_value(adj, cur_val);
				gdk_flush();
				GDK_THREADS_LEAVE();
				cur_val += 5.0;
			} else {
				GDK_THREADS_ENTER();
				gtk_adjustment_set_value(adj, cur_val);
				gdk_flush();
				GDK_THREADS_LEAVE();
				cur_val -= 5.0;
			}
			dosleep(10000);
		}
		GDK_THREADS_ENTER();
		gtk_adjustment_set_value(adj, destination);
		gdk_flush();
		GDK_THREADS_LEAVE();
	}
	pthread_mutex_unlock(&smoother_mutex);
	pthread_exit(NULL);
}


void forward_play_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj;
	int smooth_trans;

	smooth_trans = prefs_get_bool(ap_prefs, "gtk_interface", "smooth_transition", 0);
	adj = GTK_RANGE(data)->adjustment;

	if (smooth_trans) {
		destination = 100.0;
		pthread_create(&smoother_thread, NULL,
				(void * (*)(void *))smoother, adj);
		pthread_detach(smoother_thread);
	} else {	
		gtk_adjustment_set_value(adj, 100.0);
	}	
}


void reverse_play_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj;
	int smooth_trans;

	smooth_trans = prefs_get_bool(ap_prefs, "gtk_interface", "smooth_transition", 0);

	adj = GTK_RANGE(data)->adjustment;

	if (smooth_trans) {
		destination = -100.0;
		pthread_create(&smoother_thread, NULL,
				(void * (*)(void *))smoother, adj);
		pthread_detach(smoother_thread);
	} else {	
		gtk_adjustment_set_value(adj, -100.0);
	}	
}


void pause_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj;
	float new_val, temp;
	int smooth_trans;

	adj = GTK_RANGE(data)->adjustment;

	smooth_trans = prefs_get_bool(ap_prefs, "gtk_interface", "smooth_transition", 0);
		
	if (smooth_trans) {
		if (destination <= adj->value && destination != 0.0) {
			destination = 0.0;
		} else {
			destination = 100.0;
		}	
		pthread_create(&smoother_thread, NULL,
			(void * (*)(void *))smoother, adj);
		pthread_detach(smoother_thread);
	} else {
		if (adj->value != 0.0) {
			gtk_adjustment_set_value(adj, 0.0);
		} else {
			gtk_adjustment_set_value(adj, 100.0);
		}
	}	
}


void stop_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if (p && p->IsPlaying()) {
		pl->Pause();
		p->Stop();
		clear_buffer();
	}	
}

void eject_cb(GtkWidget *, gpointer);

void play_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();
	if (p) {
		pl->UnPause();
		if (p->IsPlaying() || !pl->Length()) {
			eject_cb(widget, data);
		} else if (!p->IsPlaying() && pl->Length()) {
			pl->Play(pl->GetCurrent());
		}	
	}	
}


void eject_cb(GtkWidget *wdiget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if (p) {
		gtk_widget_show(play_dialog);
		gdk_window_raise(play_dialog->window);
	}
}	


void volume_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj = (GtkAdjustment *)widget;
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if (p) {
		int idx = (int)adj->value;
		idx = (idx < 0) ? 0 : ((idx > 13) ? 13 : idx);
		p->SetVolume(vol_scale[idx]);
	}
}


void balance_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj = (GtkAdjustment *)widget;
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();
	int val;

	if (p) {
		val = (int)adj->value;
		if (val > MIN_BAL_TRESH && val < MAX_BAL_TRESH) val = BAL_CENTER;
		p->SetPan(val - 100);
	}	
}


gint indicator_callback(gpointer data, int locking)
{
	update_struct *ustr;
	Playlist *pl;
	CorePlayer *p;
	GtkAdjustment *adj;
	GdkDrawable *drawable;
	GdkRectangle  update_rect;
	GdkColor color;
	stream_info info;
	char title_string[256];
	char str[60];
	long slider_val, t_min, t_sec;
	long c_hsec, secs, c_min, c_sec;
	long sr;
	static char old_str[60] = "";

	ustr = &global_ustr;
	pl = (Playlist *)ustr->data;
	p = pl->GetCorePlayer();
	drawable = ustr->drawing_area->window;

	adj = GTK_RANGE(ustr->speed_scale)->adjustment;
	//gtk_adjustment_set_value(adj, p->GetSpeed() * 100.0);

	adj = GTK_RANGE(ustr->pos_scale)->adjustment;
	if (p->CanSeek()) {
		adj->lower = 0;
		adj->upper = p->GetFrames() - 32; // HACK!!
		if (locking)
			GDK_THREADS_ENTER();
		gtk_widget_set_sensitive(GTK_WIDGET(ustr->pos_scale), true);
		if (locking)
			GDK_THREADS_LEAVE();
	} else {
		adj->lower = adj->upper = 0;
		if (locking)
			GDK_THREADS_ENTER();
		gtk_adjustment_set_value(adj, 0);
		gtk_widget_set_sensitive(GTK_WIDGET(ustr->pos_scale), false);
		if (locking)
			GDK_THREADS_LEAVE();
	}	
	memset(&info, 0, sizeof(stream_info));

	color.red = color.blue = color.green = 0;
	if (locking)
		GDK_THREADS_ENTER();
	gdk_color_alloc(gdk_colormap_get_system(), &color);
	if (locking)
		GDK_THREADS_LEAVE();
	sr = p->GetSampleRate();
	if (p->IsActive()) { 
		int pos;
		pos = global_update ? p->GetPosition() : (int) adj->value;
				slider_val = pos;
		secs = global_update ? 
						p->GetCurrentTime() : p->GetCurrentTime((int) adj->value);
		c_min = secs / 6000;
		c_sec = (secs % 6000) / 100;
#ifdef SUBSECOND_DISPLAY		
		c_hsec = secs % 100;
#endif		
		secs = p->GetCurrentTime(p->GetFrames());
		t_min = secs / 6000;
		t_sec = (secs % 6000) / 100;
		if (locking)
			GDK_THREADS_ENTER();
		gtk_adjustment_set_value(adj, pos);
		if (locking)
			GDK_THREADS_LEAVE();
		p->GetStreamInfo(&info);
	} else {
		t_min = 0;
		t_sec = 0;
		c_sec = 0;
		c_min = 0;
		c_hsec = 0;
		sprintf(info.title, "No stream");
	}
	if (t_min == 0 && t_sec == 0 && !strlen(info.status)) {
		sprintf(str, "No status");
	} else {
#ifdef SUBSECOND_DISPLAY	
		sprintf(str, "%02ld:%02ld.%02d/%02d:%02d", c_min, c_sec, c_hsec, t_min, t_sec);
#else
		if (!strlen(info.status))
			sprintf(str, "%02ld:%02ld/%02ld:%02ld", c_min, c_sec, t_min, t_sec);
		else
			sprintf(str, "%s", info.status);
#endif
	}
	if (val_ind && strcmp(old_str, str) != 0) {
		strcpy(old_str, str);
		// Painting in pixmap here
		update_rect.x = ustr->drawing_area->allocation.width-INDICATOR_WIDTH;
		update_rect.y = 16;
		update_rect.width = INDICATOR_WIDTH;
		update_rect.height = 18;
		if (locking)
			GDK_THREADS_ENTER();
		gdk_draw_rectangle(val_ind, 
						   ustr->drawing_area->style->black_gc,
						   true,
						   update_rect.x,
						   update_rect.y,
						   update_rect.width,
						   update_rect.height);

		gdk_draw_string(val_ind,
						ustr->drawing_area->style->font,
						ustr->drawing_area->style->white_gc,
						update_rect.x + 2, 
						update_rect.y + 12,
						str);	
		gtk_widget_draw (ustr->drawing_area, &update_rect);
		if (locking)
			GDK_THREADS_LEAVE();
	}
	if (locking)
		GDK_THREADS_ENTER();
	draw_format(info.stream_type);
	if (strlen(info.author)) {
		sprintf(title_string, "%s - %s", info.author, info.title);
		draw_title(title_string);
	} else	
		draw_title(info.title);
	draw_speed();
	if (global_draw_volume)
		draw_volume();
	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = ustr->drawing_area->allocation.width;
	update_rect.height = ustr->drawing_area->allocation.height;
	gdk_flush();
	if (locking)
		GDK_THREADS_LEAVE();
	return true;
}


void cd_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if (p) {
		pl->Pause();
		if (p->Open("CD.cdda"))
			p->Start();
		pl->UnPause();
	}
}


void exit_cb(GtkWidget *widget, gpointer data)
{
	GtkFunction f = (GtkFunction)data;
	global_update = -1;
	GDK_THREADS_LEAVE(); // Drop thread so indicator_thread can finish
	pthread_join(indicator_thread, NULL);
	GDK_THREADS_ENTER();
	if (f) { // Oh my, a very ugly HACK indeed! But it works
		GDK_THREADS_LEAVE();
		f(NULL);
		GDK_THREADS_ENTER();
	}
	// This is more HACK stuff, but then again GTK IS A BIG FREAKING HACK!
	GDK_THREADS_LEAVE();
	gtk_main_quit();
	gdk_flush();
	GDK_THREADS_ENTER();
}

void scopes_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *win = (GtkWidget *)data;
	int x, y;
	if (global_scopes_show) {
		gdk_window_get_origin(win->window, &x, &y);
		if (windows_x_offset >= 0) {
			x -= windows_x_offset;
			y -= windows_y_offset;
		}
		gtk_widget_hide(win);
		gtk_widget_set_uposition(win, x, y);
	} else {
		gtk_widget_show(win);
	}
	global_scopes_show = 1 - global_scopes_show;	
}


void effects_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *win = (GtkWidget *)data;
	int x, y;

	if (global_effects_show) {
		gdk_window_get_origin(win->window, &x, &y);
		if (windows_x_offset >= 0) {
			x -= windows_x_offset;
			y -= windows_y_offset;
		}
		gtk_widget_hide(win);
		gtk_widget_set_uposition(win, x, y);
	} else {
		gtk_widget_show(win);
	}
	global_effects_show = 1 - global_effects_show;
}


void play_file_ok(GtkWidget *widget, gpointer data)
{
	Playlist *playlist = (Playlist *)data;
	CorePlayer *p = playlist->GetCorePlayer();

	if (p) {
		char *selected;
		GtkCList *file_list =
		GTK_CLIST(GTK_FILE_SELECTION(play_dialog)->file_list);
		GList *next = file_list->selection;
		if (!next) { // Nothing was selected
			return;
		}
		gchar *current_dir =
		g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(play_dialog)));
		char *path;
		int index;	
		int marker = strlen(current_dir)-1;
		while (marker > 0 && current_dir[marker] != '/')
			current_dir[marker--] = '\0';
		// Write default_play_path
		prefs_set_string(ap_prefs, "gtk_interface", "default_play_path", current_dir);
		
		// Get the selections
		std::vector<std::string> paths;
		while (next) {
			index = GPOINTER_TO_INT(next->data);

			gtk_clist_get_text(file_list, index, 0, &path);
			if (path) {
				paths.push_back(std::string(current_dir) + "/" + path);
			}
			next = next->next;
		}

		// Sort them (they're sometimes returned in a slightly odd order)
		sort(paths.begin(), paths.end());

		// Add selections to the queue, and start playing them
		playlist->AddAndPlay(paths);
		playlist->UnPause();
		
		gtk_clist_unselect_all(file_list);
		g_free(current_dir);
	}
	// Save path
	gtk_widget_hide(GTK_WIDGET(play_dialog));
}

void play_file_cancel(GtkWidget *widget, gpointer data)
{
	gint x,y;

	gdk_window_get_root_origin(GTK_WIDGET(data)->window, &x, &y);
	gtk_widget_hide(GTK_WIDGET(data));
	gtk_widget_hide(GTK_WIDGET(data));
	gtk_widget_set_uposition(GTK_WIDGET(data), x, y);
}


gboolean play_file_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gint x, y;

	gdk_window_get_root_origin(widget->window, &x, &y);
	gtk_widget_hide(widget);
	gtk_widget_set_uposition(widget, x, y);

	return TRUE;
}

void playlist_cb(GtkWidget *widget, gpointer data)
{
	PlaylistWindowGTK *pl = (PlaylistWindowGTK *)data;
	pl->ToggleVisible();
}

gint alsaplayer_button_press(GtkWidget *widget, GdkEvent *event)
{
	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *bevent = (GdkEventButton *) event;
		gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL,
						bevent->button, bevent->time);
		return true;
	}
	return false;
}


void indicator_looper(void *data)
{
#ifdef DEBUG
	//alsaplayer_error("THREAD-%d=indicator thread", getpid());
#endif
	while (global_update >= 0) {
		if (global_update == 1) {
			indicator_callback(data, 1);
		}
		dosleep(UPDATE_TIMEOUT);
	}
	//alsaplayer_error("Exitting indicator_looper");	
	pthread_exit(NULL);
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


void on_expose_event (GtkWidget * widget, GdkEvent * event, gpointer data)
{
  gint x, y;

  if (windows_x_offset == -1)
    {
      gdk_window_get_origin (widget->window, &x, &y);
      windows_x_offset = x - main_window_x;
      /* Make sure offset seems reasonable. If not, set it to -2 so we don't
         try this again later. */
      if (windows_x_offset < 0 || windows_x_offset > 50)
        windows_x_offset = -2;
      else
        windows_y_offset = y - main_window_y;
    }
}

gint pixmap_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	GdkPixmap *the_pixmap = val_ind;
	gdk_draw_pixmap(widget->window,
		widget->style->black_gc,
		the_pixmap,
		event->area.x, event->area.y,
		event->area.x, event->area.y,
		event->area.width, event->area.height);
	return false;
}


gint val_area_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	if (val_ind) {
		global_update = 0;
		gdk_pixmap_unref(val_ind);
	}
	val_ind = gdk_pixmap_new(widget->window,
		widget->allocation.width,
		32, -1);
	gdk_draw_rectangle(val_ind,
						widget->style->black_gc,
                        true, 
                        0, 0,
                        widget->allocation.width,
                        32);
	// Set up expose event handler 
	gtk_signal_connect(GTK_OBJECT(widget), "expose_event",
                (GtkSignalFunc) pixmap_expose, val_ind);
	global_update = 1;	
	return true;
}

void init_main_window(Playlist *pl, GtkFunction f)
{
	GtkWidget *root_menu;
	GtkWidget *menu_item;
	GtkWidget *main_window;
	GtkWidget *effects_window;
	GtkWidget *scopes_window;
	GtkWidget *working;
	GtkWidget *speed_scale;
	GtkWidget *pix;
	GtkWidget *val_area;
	GtkStyle *style;
	GdkFont *smallfont;
	GtkAdjustment *adj;

	// Dirty trick
	playlist = pl;

	main_window = create_main_window();
	gtk_window_set_policy(GTK_WINDOW(main_window), false, false, false);
	gtk_window_set_title(GTK_WINDOW(main_window), global_session_name == NULL ?
		"AlsaPlayer" : global_session_name);
	gtk_window_set_wmclass(GTK_WINDOW(main_window), "AlsaPlayer", "alsaplayer");
	gtk_widget_realize(main_window);

	playlist_window_gtk = new PlaylistWindowGTK(playlist);

	effects_window = init_effects_window();	
	scopes_window = init_scopes_window();
	play_dialog = gtk_file_selection_new("Play file");
	gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (play_dialog));
	GtkCList *file_list = GTK_CLIST(GTK_FILE_SELECTION(play_dialog)->file_list);

	gtk_clist_set_selection_mode(file_list, GTK_SELECTION_EXTENDED);

	gtk_signal_connect(GTK_OBJECT(
								  GTK_FILE_SELECTION(play_dialog)->cancel_button), "clicked",
					   GTK_SIGNAL_FUNC(play_file_cancel), play_dialog);
	gtk_signal_connect(GTK_OBJECT(play_dialog), "delete_event",
					   GTK_SIGNAL_FUNC(play_file_delete_event), play_dialog);
	gtk_signal_connect(GTK_OBJECT(
								  GTK_FILE_SELECTION(play_dialog)->ok_button),
					   "clicked", GTK_SIGNAL_FUNC(play_file_ok), playlist);
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(play_dialog), 
		prefs_get_string(ap_prefs, "gtk_interface", "default_play_path", "~/")); 


	gtk_signal_connect (GTK_OBJECT (main_window), "expose_event",
						GTK_SIGNAL_FUNC (on_expose_event), NULL);


	speed_scale = get_widget(main_window, "pitch_scale"); 

	smallfont = gdk_font_load("-adobe-helvetica-medium-r-normal--10-*-*-*-*-*-*-*");

	if (!smallfont)
		assert((smallfont = gdk_fontset_load("fixed")) != NULL);

	style = gtk_style_new();
	style = gtk_style_copy(gtk_widget_get_style(main_window));
	gdk_font_unref(style->font);
	style->font = smallfont;
	gdk_font_ref(style->font); 	

	working = get_widget(main_window, "stop_button");
	pix = xpm_label_box(stop_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
					   GTK_SIGNAL_FUNC(stop_cb), playlist);
	gtk_button_set_relief(GTK_BUTTON(working), global_rb ? GTK_RELIEF_NONE :
						  GTK_RELIEF_NORMAL);

	working = get_widget(main_window, "reverse_button");
	pix = xpm_label_box(r_play_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
					   GTK_SIGNAL_FUNC(reverse_play_cb), speed_scale);
	gtk_button_set_relief(GTK_BUTTON(working), global_rb ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);

	working = get_widget(main_window, "forward_button");
	pix = xpm_label_box(f_play_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix); 
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
					   GTK_SIGNAL_FUNC(forward_play_cb), speed_scale);
	gtk_button_set_relief(GTK_BUTTON(working), 
						  global_rb ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);

	working = get_widget(main_window, "pause_button");
	pix = xpm_label_box(pause_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
					   GTK_SIGNAL_FUNC(pause_cb), speed_scale);
	gtk_button_set_relief(GTK_BUTTON(working), 
						  global_rb ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);

	working = get_widget(main_window, "prev_button");
	pix = xpm_label_box(prev_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
					   GTK_SIGNAL_FUNC(playlist_window_gtk_prev), playlist);
	gtk_button_set_relief(GTK_BUTTON(working), 
						  global_rb ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);

	working = get_widget(main_window, "next_button");
	pix = xpm_label_box(next_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix); 
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
					   GTK_SIGNAL_FUNC(playlist_window_gtk_next), playlist);
	gtk_button_set_relief(GTK_BUTTON(working), 
						  global_rb ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);

	working = get_widget(main_window, "play_button");
	play_pix = xpm_label_box(play_xpm, main_window);
	gtk_widget_show(play_pix);
	gtk_container_add(GTK_CONTAINER(working), play_pix);
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
					   GTK_SIGNAL_FUNC(play_cb), playlist);
	gtk_button_set_relief(GTK_BUTTON(working), 
						  global_rb ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);

	working = get_widget(main_window, "volume_pix_frame");
	if (working) {
		pix = xpm_label_box(volume_icon_xpm, main_window);
		gtk_widget_show(pix);
		gtk_container_add(GTK_CONTAINER(working), pix);
	}
	working = get_widget(main_window, "vol_scale");
	if (working) {
#ifdef NEW_SCALE	
		adj = GTK_BSCALE(working)->adjustment;
#else
		adj = GTK_RANGE(working)->adjustment;
#endif	
		gtk_adjustment_set_value(adj, (float)(pl->GetCorePlayer())->GetVolume());
		gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
							GTK_SIGNAL_FUNC(volume_cb), playlist);
	}

	val_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(val_area), 204, 32);
	gtk_widget_show(val_area);

	global_ustr.vol_scale = working;
	global_ustr.drawing_area = val_area;
	global_ustr.data = playlist;
	if (working) {	
		gtk_signal_connect (GTK_OBJECT (working), "motion_notify_event",
							GTK_SIGNAL_FUNC(volume_move_event), &global_ustr);
		gtk_signal_connect (GTK_OBJECT (working), "button_press_event",
							GTK_SIGNAL_FUNC(volume_move_event), &global_ustr);
	}
	gtk_signal_connect(GTK_OBJECT(val_area), "configure_event",
					   (GtkSignalFunc) val_area_configure, NULL);
	gtk_signal_connect(GTK_OBJECT(val_area), "expose_event",
					   (GtkSignalFunc) pixmap_expose, NULL);


	working = get_widget(main_window, "balance_pic_frame");
	if (working) {
		pix = xpm_label_box(balance_icon_xpm, main_window);
		gtk_widget_show(pix);
		gtk_container_add(GTK_CONTAINER(working), pix);
	}
	working = get_widget(main_window,  "bal_scale");
	if (working) {		
		adj = GTK_RANGE(working)->adjustment;
		gtk_adjustment_set_value(adj, 100.0);
		gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
							GTK_SIGNAL_FUNC(balance_cb), playlist);
		global_ustr.bal_scale = working;
		gtk_signal_connect (GTK_OBJECT (working), "motion_notify_event",
							GTK_SIGNAL_FUNC(balance_move_event), &global_ustr);
		gtk_signal_connect (GTK_OBJECT (working), "button_press_event",
							GTK_SIGNAL_FUNC(balance_move_event), &global_ustr);
		gtk_signal_connect (GTK_OBJECT(working), "button_release_event",
							GTK_SIGNAL_FUNC(balance_release_event), &global_ustr);
	}

	working = get_widget(main_window, "playlist_button");
	pix = xpm_label_box(playlist_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	gtk_signal_connect(GTK_OBJECT(working), "clicked",
					   GTK_SIGNAL_FUNC(playlist_cb), playlist_window_gtk); 
	gtk_button_set_relief(GTK_BUTTON(working), 
						  global_rb ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);
	
	working = get_widget(main_window, "cd_button");
	pix = xpm_label_box(menu_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	//gtk_signal_connect(GTK_OBJECT(working), "clicked",
	//			GTK_SIGNAL_FUNC(cd_cb), p);
	gtk_button_set_relief(GTK_BUTTON(working),
						  global_rb ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);

	working = get_widget(main_window, "info_box"); 

	gtk_widget_set_style(val_area, style);

	gtk_box_pack_start (GTK_BOX (working), val_area, true, true, 0);

	global_ustr.data = playlist;
	working = get_widget(main_window, "pos_scale");
	global_ustr.pos_scale = working;

	working = get_widget(main_window, "pos_scale");
	gtk_signal_connect(GTK_OBJECT(working), "button_release_event",
					   GTK_SIGNAL_FUNC(release_event), &global_ustr);
	gtk_signal_connect (GTK_OBJECT (working), "button_press_event",
						GTK_SIGNAL_FUNC(press_event), NULL);
	gtk_signal_connect (GTK_OBJECT (working), "motion_notify_event",
						GTK_SIGNAL_FUNC(move_event), &global_ustr);


	global_ustr.speed_scale = speed_scale;
#if 1
	gtk_signal_connect (GTK_OBJECT (speed_scale), "motion_notify_event",
						GTK_SIGNAL_FUNC(speed_move_event), &global_ustr);
	gtk_signal_connect (GTK_OBJECT (speed_scale), "button_press_event",
						GTK_SIGNAL_FUNC(speed_move_event), &global_ustr);

	adj = GTK_RANGE(speed_scale)->adjustment;
	//gtk_adjustment_set_value(adj, 100.0);
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
						GTK_SIGNAL_FUNC(speed_cb), playlist);
	#endif
	gtk_signal_connect(GTK_OBJECT(main_window), "delete_event", GTK_SIGNAL_FUNC(main_window_delete), (void *)f);

	// Create root menu
	root_menu = gtk_menu_new();

	// Preferences
#if 0
	menu_item = gtk_menu_item_new_with_label("Preferences...");
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_widget_show(menu_item);
#endif
	// Scopes
	menu_item = gtk_menu_item_new_with_label("Scopes...");
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
					   GTK_SIGNAL_FUNC(scopes_cb), scopes_window);
	gtk_widget_show(menu_item);
	// Effects
	menu_item = gtk_menu_item_new_with_label("Effects...");
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
					   GTK_SIGNAL_FUNC(effects_cb), effects_window);
	gtk_widget_show(menu_item);
	gtk_widget_set_sensitive(menu_item, false);

	// About
	menu_item = gtk_menu_item_new_with_label("About...");
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_widget_show(menu_item);
	gtk_widget_set_sensitive(menu_item, false);

#if 1	
	// Separator
	menu_item = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_widget_show(menu_item);

	// CD playback
	menu_item = gtk_menu_item_new_with_label("CD Player (CDDA)");
	gtk_widget_show(menu_item);
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
					   GTK_SIGNAL_FUNC(cd_cb), playlist);
#endif


	// Separator
	menu_item = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_widget_show(menu_item);
	// Exit
	menu_item = gtk_menu_item_new_with_label("Exit");
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
					   GTK_SIGNAL_FUNC(exit_cb), (void *)f);
	gtk_widget_show(menu_item);
	
	working = get_widget(main_window, "cd_button");
	gtk_signal_connect_object (GTK_OBJECT (working), "event",
							   GTK_SIGNAL_FUNC(alsaplayer_button_press), GTK_OBJECT(root_menu)); // cd

	gdk_flush();

	//gdk_window_set_decorations(GTK_WIDGET(main_window)->window, (GdkWMDecoration)0);
	gtk_widget_show(GTK_WIDGET(main_window));

	// Check if we should open the playlist
	if (prefs_get_bool(ap_prefs, "gtk_interface", "playlist_active", 0)) {
		playlist_window_gtk->Show();
	}	

	// start indicator thread
	//alsaplayer_error("About to start indicator_looper()");
	pthread_create(&indicator_thread, NULL,
				   (void * (*)(void *))indicator_looper, playlist);
#ifdef DEBUG
	printf("THREAD-%d=main thread\n", getpid());
#endif
}
