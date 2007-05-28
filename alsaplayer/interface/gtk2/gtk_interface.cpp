/*  gtk2_interface.cpp - gtk+ callbacks, etc
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
#include <assert.h>
//#define NEW_SCALE
//#define SUBSECOND_DISPLAY 
#include <math.h>

#include <algorithm>
#include "utilities.h"

// Include things needed for gtk interface:
#include <gtk/gtk.h>
#include <glib.h>

#include "support.h"
#include "gladesrc.h"
#include "gtk_interface.h"
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
//#include "pixmaps/cd.xpm"
#include "pixmaps/menu.xpm"

#include "PlaylistWindow.h"

// Include other things: (ultimate aim is to wrap this up into one nice file)
#include "CorePlayer.h"
#include "Playlist.h"
#include "EffectsWindow.h"
//#include "Effects.h"
#include "ScopesWindow.h"
#include "control.h"

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
#define EQ_TEMP_STEP(freq, step)        freq * pow(2.0000, (float) step / 12.0)
#define FPS_HACK	32	// number of audio frames

// Global variables (get rid of these too... ;-) )
int global_update = 1;
/* These are used to contain the size of the window manager borders around
   our windows, and are used to show/hide windows in the same positions. */
gint global_effects_show = 0;
gint global_scopes_show = 0;


gint windows_x_offset = -1;
gint windows_y_offset = -1;

static int global_draw_volume = 1;
static GtkWidget *play_pix;
static GdkPixmap *val_ind = NULL;
static PlaylistWindowGTK *playlist_window_gtk = NULL;
static coreplayer_notifier notifier;

static gint main_window_x = 150;
static gint main_window_y = 175;

typedef struct  _update_struct {
	gpointer data;
	GtkWidget *drawing_area;
	GtkWidget *vol_scale;
	GtkWidget *bal_scale;
	GtkWidget *pos_scale;
	GtkWidget *speed_scale;
	float speed;
} update_struct;

static update_struct global_ustr;

#define LOOP_OFF	0
#define LOOP_START_SET	1
#define LOOP_ON		2

typedef struct  _loop_struct {
	int state;
	gfloat start;
	gfloat end;
	unsigned int track; // used to exit loop mode when a new song is played
} loop_struct;

static loop_struct global_loop;

// Static variables  (to be moved into a class, at some point)
static GtkWidget *play_dialog;
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
void draw_speed(float speed);
void draw_pan(float pan);
void draw_volume(float vol);
void position_notify(void *data, int pos);
void notifier_lock();
void notifier_unlock();
void play_cb(GtkWidget *widget, gpointer data);
void pause_cb(GtkWidget *, gpointer data);
void stop_cb(GtkWidget *, gpointer data);
void loop_cb(GtkWidget *, gpointer data);
void exit_cb(GtkWidget *, gpointer data);
void forward_skip_cb(GtkWidget *, gpointer data);
void reverse_skip_cb(GtkWidget *, gpointer data);
void forward_play_cb(GtkWidget *, gpointer data);
void reverse_play_cb(GtkWidget *, gpointer data);

void speed_changed(void *, float speed)
{
	notifier_lock();
	draw_speed(speed);
	notifier_unlock();
}

void pan_changed(void *, float pan)
{
	notifier_lock();
	draw_pan(pan);
	notifier_unlock();
}

void volume_changed(void *, float vol)
{
	notifier_lock();
	draw_volume(vol);
	notifier_unlock();
}

void position_notify(void *, int pos)
{
	notifier_lock();
	indicator_callback(NULL, 0);
	notifier_unlock();
}

void notifier_lock(void)
{
	GDK_THREADS_ENTER();
}


void notifier_unlock(void)
{
	gdk_flush();
	GDK_THREADS_LEAVE();
}


void stop_notify(void *data)
{
	//alsaplayer_error("Song was stopped");
}

void start_notify(void *data)
{
	//alsaplayer_error("Song was started");
}


gboolean main_window_delete(GtkWidget *, GdkEvent *event, gpointer data)
{
	global_update = -1;

	// Remove notifier
	
	gdk_flush();
	
	if (playlist_window_gtk) {
		Playlist *playlist = playlist_window_gtk->GetPlaylist();
		GDK_THREADS_LEAVE();
		playlist->UnRegisterNotifier(&notifier);
		GDK_THREADS_ENTER();
		delete playlist_window_gtk;
	}	
	gtk_main_quit();
	gdk_flush();	
	// Never reached
	return FALSE;
}


gboolean press_event(GtkWidget *, GdkEvent *, gpointer)
{
	global_update = 0;
	return FALSE;
}

gboolean volume_move_event(GtkWidget *, GdkEvent *, gpointer)
{
	//draw_volume();
	return FALSE;
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
			gdk_draw_string(val_ind, ustr->drawing_area->style->private_font,
							ustr->drawing_area->style->white_gc, update_rect.x+6,
							update_rect.y+14, title);
			// Do the drawing
			gtk_widget_draw (ustr->drawing_area, &update_rect);	
	}
	gdk_flush();
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
			gdk_draw_string(val_ind, ustr->drawing_area->style->private_font,
							ustr->drawing_area->style->white_gc, update_rect.x+6,
							update_rect.y+12, format);
			// Do the drawing
							
		gtk_widget_draw (ustr->drawing_area, &update_rect);
	}
}


void draw_volume(float the_vol)
{
	update_struct *ustr = &global_ustr;
	GtkAdjustment *adj;
	GdkRectangle update_rect;
	char str[60];

	int vol = (int) (the_vol * 100); // quick hack
	
	if (!ustr->vol_scale)
		return;
	adj = GTK_RANGE(ustr->vol_scale)->adjustment;
	
	int val = vol; //(int)GTK_ADJUSTMENT(adj)->value;

	val ? sprintf(str, "Volume: %d%%  ", val) : sprintf(str, "Volume: mute");

	update_rect.x = 0;
	update_rect.y = 16;
	update_rect.width = 82;
	update_rect.height = 16;
	if (val_ind) {	
			gdk_draw_rectangle(val_ind,
							ustr->drawing_area->style->black_gc,
							true, update_rect.x, update_rect.y, update_rect.width, update_rect.height);
			gdk_draw_string(val_ind, ustr->drawing_area->style->private_font,
							ustr->drawing_area->style->white_gc, update_rect.x+6, update_rect.y+12, str);
			gtk_widget_draw (ustr->drawing_area, &update_rect);
	}
	gdk_flush();
}



gboolean pan_move_event(GtkWidget *, GdkEvent *, gpointer)
{
	global_draw_volume = 0;
	//draw_balance();
	return FALSE;
}


gboolean pan_release_event(GtkWidget *, GdkEvent *, gpointer)
{
	global_draw_volume = 1;
	return FALSE;
}


void draw_pan(float the_val)
{
	update_struct *ustr = &global_ustr;
	GdkRectangle update_rect;
	char str[60];
	int pan = (int)(the_val * 100.0);

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
							ustr->drawing_area->style->private_font,
							ustr->drawing_area->style->white_gc,
							update_rect.x+6, update_rect.y+12,
							str);
			gtk_widget_draw (ustr->drawing_area, &update_rect);
	}
	gdk_flush();
}


gboolean speed_move_event(GtkWidget *, GdkEvent *, gpointer)
{
	//draw_speed();
	return FALSE;
}

void draw_speed(float speed)
{
	update_struct *ustr = &global_ustr;
	GtkAdjustment *adj;
	GdkRectangle update_rect;
	char str[60];
	int speed_val;
	
	adj = GTK_RANGE(ustr->speed_scale)->adjustment;

	//speed_val = (int)GTK_ADJUSTMENT(adj)->value;
	speed_val = (int)(speed * 100.0); // We need percentages
	if (speed_val < ZERO_PITCH_TRESH && speed_val > -ZERO_PITCH_TRESH) {
		sprintf(str, "Speed: pause");
	}
	else
		sprintf(str, "Speed: %d%%  ", speed_val);
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
							ustr->drawing_area->style->private_font,
							ustr->drawing_area->style->white_gc,
							update_rect.x+6, update_rect.y+14,
							str);
			gtk_widget_draw (ustr->drawing_area, &update_rect);		
	}
	gdk_flush();
}	


gboolean val_release_event(GtkWidget *widget, GdkEvent *, gpointer)
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
	return FALSE;
}


gboolean release_event(GtkWidget *widget, GdkEvent *, gpointer)
{
	GtkAdjustment *adj;
	update_struct *ustr = &global_ustr;
	Playlist *pl = (Playlist *)ustr->data;
	CorePlayer *p = pl->GetCorePlayer();

	adj = GTK_RANGE(widget)->adjustment;
	p->Seek((int)adj->value);
	global_update = 1;
	
	return FALSE;
}


gboolean move_event(GtkWidget *, GdkEvent *, gpointer data)
{
	indicator_callback(data, 0);
	return FALSE;
}

void speed_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();
	float val =  GTK_ADJUSTMENT(widget)->value;
	if (val < ZERO_PITCH_TRESH && val > -ZERO_PITCH_TRESH)
		val = 0;
	GDK_THREADS_LEAVE();	
	p->SetSpeed(  (float) val / 100.0);
	GDK_THREADS_ENTER();
	draw_speed(val / 100.0);
}

pthread_t smoother_thread;
pthread_mutex_t smoother_mutex = PTHREAD_MUTEX_INITIALIZER;
static float destination = 100.0;
static float speed_pan_position = 100.0;

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

pthread_t looper_thread;
pthread_mutex_t looper_mutex = PTHREAD_MUTEX_INITIALIZER;
 
void looper(void *data)
{
	// GtkAdjustment *adj = (GtkAdjustment *)data;
	loop_struct *loop = &global_loop;
	update_struct *ustr = &global_ustr;
	Playlist *pl = (Playlist *)ustr->data;
	unsigned int track = pl->GetCurrent();
	CorePlayer *p = pl->GetCorePlayer();

	if (pthread_mutex_trylock(&looper_mutex) != 0) {
		pthread_exit(NULL);
	}
	
	nice(5);
	
	while (loop->state == LOOP_ON && loop->track == track) {
		if (loop->track != track) {
			loop->state = LOOP_OFF;
		} else if(p->GetPosition() >= loop->end) {
			p->Seek(lroundf(loop->start));
			// global_update = 1;	
		} 

		dosleep(10000);
	}
	pthread_mutex_unlock(&looper_mutex);
	pthread_exit(NULL);
}

gboolean key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GtkAdjustment *adj;
	update_struct *ustr = &global_ustr;

	PlaylistWindowGTK *playlist_window_gtk = (PlaylistWindowGTK *) data;
	Playlist *playlist = NULL;
	GtkWidget *list = NULL;

	playlist = playlist_window_gtk->GetPlaylist();
	list = playlist_window_gtk->GetPlaylist_list();
	
	/* key definitions are from enum gtk_keymap */
  if (event->state & GDK_CONTROL_MASK) {
	switch(event->keyval) {
     case GDK_q:
      exit_cb(NULL,NULL);
      break; 
     default:
      break;
  }
  } else {
	switch(event->keyval) {
		case STOP_KEY:
			stop_cb(NULL, ustr->data);
			break;
		case PLAY_KEY:
			play_cb(NULL, ustr->data);
			break;
		case PAUSE_KEY:
			pause_cb(NULL, ustr->speed_scale);
			break;
		case NEXT_KEY:
			playlist_window_gtk_next(NULL, ustr->data);
			break;
		case PREV_KEY:
			playlist_window_gtk_prev(NULL, ustr->data);
			break;
		case FWD_KEY:
			forward_skip_cb(NULL, ustr->pos_scale);
			break;
		case BACK_KEY:
			reverse_skip_cb(NULL, ustr->pos_scale);
			break;
		case FWD_PLAY_KEY:
			forward_play_cb(NULL, ustr->speed_scale);
			break;
		case REV_PLAY_KEY:
			reverse_play_cb(NULL, ustr->speed_scale);
			break;
		case SPEED_UP_KEY:
			adj = GTK_RANGE(ustr->speed_scale)->adjustment;
			gtk_adjustment_set_value(adj, EQ_TEMP_STEP(adj->value, 1));
			break;
		case SPEED_DOWN_KEY:
			adj = GTK_RANGE(ustr->speed_scale)->adjustment;
			gtk_adjustment_set_value(adj, EQ_TEMP_STEP(adj->value, -1));
			break;
		case SPEED_COMMA_UP_KEY:
			adj = GTK_RANGE(ustr->speed_scale)->adjustment;
			gtk_adjustment_set_value(adj, EQ_TEMP_STEP(adj->value, 0.234600103846));
			break;
		case SPEED_COMMA_DOWN_KEY:
			adj = GTK_RANGE(ustr->speed_scale)->adjustment;
			gtk_adjustment_set_value(adj, EQ_TEMP_STEP(adj->value, -0.234600103846));
			break;
		case VOL_UP_KEY:
      adj = GTK_RANGE(ustr->vol_scale)->adjustment;
      gtk_adjustment_set_value(adj, adj->value + 0.5);
			break;
			break;
		case VOL_DOWN_KEY:
      adj = GTK_RANGE(ustr->vol_scale)->adjustment;
      gtk_adjustment_set_value(adj, adj->value - 0.5);
			break;
		case LOOP_KEY:
			loop_cb(NULL, ustr->pos_scale);
			break;
		
//old stuff		
		case GDK_Insert:
			dialog_popup(widget, (gpointer)
				playlist_window_gtk->add_file);
			break;	
		case GDK_Delete:
			playlist_remove(widget, data);
			break;
		case GDK_Return:
			playlist_play_current(playlist, list);
			break;
		case GDK_Right:
			// This is a hack, but quite legal
			ap_set_position_relative(global_session_id, 10);
			break;
		case GDK_Left:
			ap_set_position_relative(global_session_id, -10);
			break;
		default:
			//printf("Unknown key pressed: %c\n", event->keyval);
			break;
	}
	}
	return TRUE;
}

void loop_cb(GtkWidget *, gpointer data)
{
	GtkAdjustment *adj = GTK_RANGE(data)->adjustment;
	update_struct *ustr = &global_ustr;
	Playlist *pl = (Playlist *)ustr->data;
	loop_struct *loop = &global_loop;
	
	switch(loop->state) {
		case LOOP_OFF:
			loop->track = pl->GetCurrent();
			loop->start = adj->value;
			loop->state = LOOP_START_SET;
			break;
		case LOOP_START_SET:
			loop->end = adj->value;
			loop->state = LOOP_ON;
			pthread_create(&looper_thread, NULL,
					(void * (*)(void *))looper, adj);
			pthread_detach(looper_thread);
			break;
		case LOOP_ON:
			loop->state = LOOP_OFF;
			break;
		default:
			break;
	}

}

void forward_skip_cb(GtkWidget *, gpointer data)
{
	GtkAdjustment *adj;
	update_struct *ustr = &global_ustr;
	Playlist *pl = (Playlist *)ustr->data;
	CorePlayer *p = pl->GetCorePlayer();

	adj = GTK_RANGE(data)->adjustment;
	p->Seek((int)adj->value + 5 * FPS_HACK);
	global_update = 1;	
}

void reverse_skip_cb(GtkWidget *, gpointer data)
{
	GtkAdjustment *adj;
	update_struct *ustr = &global_ustr;
	Playlist *pl = (Playlist *)ustr->data;
	CorePlayer *p = pl->GetCorePlayer();

	adj = GTK_RANGE(data)->adjustment;
	p->Seek((int)adj->value - 5 * FPS_HACK);
	global_update = 1;	
}

void forward_play_cb(GtkWidget *, gpointer data)
{
	GtkAdjustment *adj;
	int smooth_trans;

	smooth_trans = prefs_get_bool(ap_prefs, "gtk2_interface", "smooth_transition", 0);
	adj = GTK_RANGE(data)->adjustment;

	if (smooth_trans) {
		speed_pan_position = 100.0;
		destination = speed_pan_position;
		pthread_create(&smoother_thread, NULL,
				(void * (*)(void *))smoother, adj);
		pthread_detach(smoother_thread);
	} else {
		speed_pan_position = 100.0;
		gtk_adjustment_set_value(adj, speed_pan_position);
	}	
}


void reverse_play_cb(GtkWidget *, gpointer data)
{
	GtkAdjustment *adj;
	int smooth_trans;

	smooth_trans = prefs_get_bool(ap_prefs, "gtk2_interface", "smooth_transition", 0);

	adj = GTK_RANGE(data)->adjustment;

	if (smooth_trans) {
		speed_pan_position = -100.0;
		destination = speed_pan_position;
		pthread_create(&smoother_thread, NULL,
				(void * (*)(void *))smoother, adj);
		pthread_detach(smoother_thread);
	} else {
		speed_pan_position = -100.0;	
		gtk_adjustment_set_value(adj, speed_pan_position);
	}	
}


void pause_cb(GtkWidget *, gpointer data)
{
	GtkAdjustment *adj;
	int smooth_trans;

	adj = GTK_RANGE(data)->adjustment;
			
	smooth_trans = prefs_get_bool(ap_prefs, "gtk2_interface", "smooth_transition", 0);
		
	if (smooth_trans) {
		//?? if (destination <= adj->value && destination != 0.0) {
		if (adj->value != 0.0) {
			speed_pan_position = gtk_adjustment_get_value(adj);
			destination = 0.0;
		} else {
			destination = speed_pan_position;
		}	
		pthread_create(&smoother_thread, NULL,
			(void * (*)(void *))smoother, adj);
		pthread_detach(smoother_thread);
	} else {
		if (adj->value != 0.0) {
			speed_pan_position = gtk_adjustment_get_value(adj);
			gtk_adjustment_set_value(adj, 0.0);
		} else {
			gtk_adjustment_set_value(adj, speed_pan_position);
		}
	}	
}


void stop_cb(GtkWidget *, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if (p && p->IsPlaying()) {
		pl->Pause();
		GDK_THREADS_LEAVE();
		p->Stop();
		p->Close();
		GDK_THREADS_ENTER();
		//clear_buffer();
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
			GDK_THREADS_LEAVE();
			pl->Play(pl->GetCurrent());
			GDK_THREADS_ENTER();
		} else if (!p->IsPlaying() && pl->Length()) {
			GDK_THREADS_LEAVE();
			pl->Play(pl->GetCurrent());
			GDK_THREADS_ENTER();
		}	
	}	
}


void eject_cb(GtkWidget *, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if ((p) && (!pl->Length())) {
		gtk_widget_show(play_dialog);
// what for ?		gdk_window_raise(play_dialog->window);
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
		GDK_THREADS_LEAVE();
		p->SetVolume(((float) vol_scale[idx]) / 100.0);
		GDK_THREADS_ENTER();
	}
}


void pan_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj = (GtkAdjustment *)widget;
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();
	int val;

	if (p) {
		val = (int)adj->value;
		if (val > MIN_BAL_TRESH && val < MAX_BAL_TRESH) val = BAL_CENTER;
		GDK_THREADS_LEAVE();
		p->SetPan((float)(val) / 100.0 - 1.0);
		GDK_THREADS_ENTER();
	}	
}


gint indicator_callback(gpointer, int locking)
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
	long slider_val=0, t_min=0, t_sec=0;
	long c_hsec=0, secs=0, c_min=0, c_sec=0;
	long sr=0;
	int nr_frames=0;
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
	nr_frames = p->GetFrames();
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
		if (nr_frames >= 0) {
			secs = p->GetCurrentTime(nr_frames);
			t_min = secs / 6000;
			t_sec = (secs % 6000) / 100;
		}
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
	if (nr_frames < 0 || strlen(info.status)) {
		sprintf(str, "%s", info.status);
		if (!strlen(info.status)) {
			alsaplayer_error("empty string");
		}	
	} else {
#ifdef SUBSECOND_DISPLAY
		sprintf(str, "%02ld:%02ld.%02d / %02d:%02d", c_min, c_sec, c_hsec, t_min, t_sec);
#else
		if (nr_frames >= 0) 
			sprintf(str, "%02ld:%02ld / %02ld:%02ld", c_min, c_sec, t_min, t_sec);
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
						ustr->drawing_area->style->private_font,
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
	if (strlen(info.artist)) {
		sprintf(title_string, "%s - %s", info.title, info.artist);
		draw_title(title_string);
	} else if (strlen(info.title)) {
		sprintf(title_string, "%s", info.title);
		draw_title(title_string);
	} else {
		char *p = strrchr(info.path, '/');
		if (p) {
			p++;
			draw_title(p);
		} else {
			draw_title(info.path);
		}	
	}
	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = ustr->drawing_area->allocation.width;
	update_rect.height = ustr->drawing_area->allocation.height;
	gdk_flush();
	if (locking)
		GDK_THREADS_LEAVE();
	return true;
}


void cd_cb(GtkWidget *, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if (p) {
		pl->Pause();
		GDK_THREADS_LEAVE();
		p->Stop();
		pl->Clear();
		if (p->Open("CD.cdda")) {
			p->Start();
		}	
		GDK_THREADS_ENTER();
		pl->UnPause();
	}
}


void exit_cb(GtkWidget *, gpointer data)
{
	GtkFunction f = (GtkFunction)data;
	global_update = -1;
	gdk_flush();
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

void scopes_cb(GtkWidget *, gpointer data)
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


void effects_cb(GtkWidget *, gpointer data)
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


void play_file_ok(GtkWidget *play_dialog, gpointer data)
{
	Playlist *playlist = (Playlist *)data;
	CorePlayer *p = playlist->GetCorePlayer();

	if (p) {

/* file_list = next*/		GSList *file_list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(play_dialog));
	
			std::vector<std::string> paths;
			gchar *current_dir = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(play_dialog)));
		char *path;
		
		// Write default_play_path
		prefs_set_string(ap_prefs, "gtk2_interface", "default_play_path", current_dir);
/* doesn't really work 
		if (!file_list) {
// to be fixed	gchar *sel = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_FILE_SELECTION(play_dialog)->selection_entry)));
			gchar *sel = NULL;			
			if (sel && strlen(sel)) {
				if (!strstr(sel, "http://"))
					paths.push_back(std::string(current_dir) + "/" + sel);
				else
					paths.push_back(sel);
				GDK_THREADS_LEAVE();
				playlist->AddAndPlay(paths);
				GDK_THREADS_ENTER();
//				gtk_entry_set_text(GTK_ENTRY(GTK_FILE_SELECTION(play_dialog)->selection_entry), "");
				g_free(sel);
			}	
			return;
		}
*/
		// Get the selections
		while (file_list) {
			path = (char *) file_list->data;
			
			if (path) {
				paths.push_back(path);
			}
			file_list = file_list->next;
		}

		// Sort them (they're sometimes returned in a slightly odd order)
		sort(paths.begin(), paths.end());

		// Add selections to the queue, and start playing them
		GDK_THREADS_LEAVE();
		playlist->AddAndPlay(paths);
		GDK_THREADS_ENTER();
		playlist->UnPause();
		
		gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER(play_dialog));
		g_slist_free(file_list);	
	}
	// Save path
	gtk_widget_hide(play_dialog);
}

void play_file_cancel(GtkWidget *play_dialog, gpointer data)
{
// do we really need placing window ?
//	gint x,y;

//	gdk_window_get_root_origin(play_dialog->window, &x, &y);
	gtk_widget_hide(play_dialog);
//	gtk_widget_set_uposition(play_dialog, x, y);
}

void playlist_cb(GtkWidget *, gpointer data)
{
	PlaylistWindowGTK *pl = (PlaylistWindowGTK *)data;
	pl->ToggleVisible();
}

gint alsaplayer_button_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *bevent = (GdkEventButton *) event;
		gtk_menu_popup (GTK_MENU (data), NULL, NULL, NULL, NULL,
						bevent->button, bevent->time);
		return true;
	}
	return false;
}



GtkWidget *xpm_label_box(gchar *xpm_data[], GtkWidget *to_win)
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


gboolean on_expose_event (GtkWidget * widget, GdkEvent *event, gpointer data)
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
    return FALSE;
}

gint pixmap_expose(GtkWidget *widget, GdkEventExpose *event, gpointer)
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


gint val_area_configure(GtkWidget *widget, GdkEventConfigure *, gpointer)
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
	g_signal_connect(G_OBJECT(widget), "expose_event",
                G_CALLBACK(pixmap_expose), val_ind);
	global_update = 1;	
	return true;
}

void play_dialog_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
		play_file_ok(GTK_WIDGET(dialog), (gpointer) user_data);
	else
		play_file_cancel(GTK_WIDGET(dialog), NULL);
}	

void init_main_window(Playlist *pl)
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
//my try	gtk_window_set_policy(GTK_WINDOW(main_window), false, false, false);
	gtk_window_set_title(GTK_WINDOW(main_window), global_session_name == NULL ?
		"AlsaPlayer" : global_session_name);
	gtk_window_set_wmclass(GTK_WINDOW(main_window), "AlsaPlayer", "alsaplayer");
	gtk_widget_realize(main_window);
	playlist_window_gtk = new PlaylistWindowGTK(playlist);

	effects_window = init_effects_window();	
	scopes_window = init_scopes_window();
//	play_dialog = gtk_file_selection_new("Play file or URL");
//	gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (play_dialog));
  play_dialog = gtk_file_chooser_dialog_new("Load Playlist", GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_OPEN, 
  																GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				     											GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      										NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(play_dialog), TRUE);
//	GtkTreeView *file_list = GTK_TREE_VIEW(GTK_FILE_SELECTION(play_dialog)->file_list);
//	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(file_list), GTK_SELECTION_MULTIPLE);

//	GSList *file_list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(play_dialog));
	
//	g_signal_connect(G_OBJECT(
//								  GTK_FILE_SELECTION(play_dialog)->cancel_button), "clicked",
//					   G_CALLBACK(play_file_cancel), play_dialog);
//	g_signal_connect(G_OBJECT(play_dialog), "delete_event",
//					   G_CALLBACK(play_file_delete_event), play_dialog);
//	g_signal_connect(G_OBJECT(
//								  GTK_FILE_SELECTION(play_dialog)->ok_button),
//					   "clicked", G_CALLBACK(play_file_ok), playlist);
//	gtk_file_selection_set_filename(GTK_FILE_SELECTION(play_dialog), 
//		prefs_get_string(ap_prefs, "gtk2_interface", "default_play_path", "~/")); 

	g_signal_connect(G_OBJECT(play_dialog), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(play_dialog), "response", G_CALLBACK(play_dialog_cb), playlist);

	g_signal_connect (G_OBJECT (main_window), "expose_event",
						G_CALLBACK(on_expose_event), NULL);

	gtk_signal_connect(GTK_OBJECT(main_window), "key_press_event",
		GTK_SIGNAL_FUNC(key_press_cb), (gpointer)playlist_window_gtk);

	speed_scale = get_widget(main_window, "pitch_scale"); 

	smallfont = gdk_font_load("-adobe-helvetica-medium-r-normal--10-*-*-*-*-*-*-*");

	if (!smallfont)
		assert((smallfont = gdk_fontset_load("fixed")) != NULL);

	style = gtk_style_new();
	style = gtk_style_copy(gtk_widget_get_style(main_window));
	
	if (style->private_font)
		gdk_font_unref(style->private_font);
	style->private_font = smallfont;
	gdk_font_ref(style->private_font); 	

	working = get_widget(main_window, "stop_button");
	pix = xpm_label_box(stop_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	g_signal_connect(G_OBJECT(working), "clicked",
					   G_CALLBACK(stop_cb), playlist);
	gtk_button_set_relief(GTK_BUTTON(working), GTK_RELIEF_NONE);

	working = get_widget(main_window, "reverse_button");
	pix = xpm_label_box(r_play_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	g_signal_connect(G_OBJECT(working), "clicked",
					   G_CALLBACK(reverse_play_cb), speed_scale);
	gtk_button_set_relief(GTK_BUTTON(working), GTK_RELIEF_NONE);

	working = get_widget(main_window, "forward_button");
	pix = xpm_label_box(f_play_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix); 
	g_signal_connect(G_OBJECT(working), "clicked",
					   G_CALLBACK(forward_play_cb), speed_scale);
	gtk_button_set_relief(GTK_BUTTON(working), GTK_RELIEF_NONE);

	working = get_widget(main_window, "pause_button");
	pix = xpm_label_box(pause_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	g_signal_connect(G_OBJECT(working), "clicked",
					   G_CALLBACK(pause_cb), speed_scale);
	gtk_button_set_relief(GTK_BUTTON(working), GTK_RELIEF_NONE);

	working = get_widget(main_window, "prev_button");
	pix = xpm_label_box(prev_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	g_signal_connect(G_OBJECT(working), "clicked",
					   G_CALLBACK(playlist_window_gtk_prev), playlist);
	gtk_button_set_relief(GTK_BUTTON(working), GTK_RELIEF_NONE);

	working = get_widget(main_window, "next_button");
	pix = xpm_label_box(next_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix); 
	g_signal_connect(G_OBJECT(working), "clicked",
					   G_CALLBACK(playlist_window_gtk_next), playlist);
	gtk_button_set_relief(GTK_BUTTON(working), GTK_RELIEF_NONE);

	working = get_widget(main_window, "play_button");
	play_pix = xpm_label_box(play_xpm, main_window);
	gtk_widget_show(play_pix);
	gtk_container_add(GTK_CONTAINER(working), play_pix);
	g_signal_connect(G_OBJECT(working), "clicked",
					   G_CALLBACK(play_cb), playlist);
	gtk_button_set_relief(GTK_BUTTON(working), GTK_RELIEF_NONE);

	working = get_widget(main_window, "volume_pix_frame");
	if (working) {
		pix = xpm_label_box(volume_icon_xpm, main_window);
		gtk_widget_show(pix);
		gtk_container_add(GTK_CONTAINER(working), pix);
	}
	working = get_widget(main_window, "vol_scale");
	if (working) {
		
		adj = GTK_RANGE(working)->adjustment;
		gtk_adjustment_set_value(adj, (pl->GetCorePlayer())->GetVolume() * 100.0);
		g_signal_connect (G_OBJECT (adj), "value_changed",
							G_CALLBACK(volume_cb), playlist);
	}

	val_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(val_area), 204, 32);
	gtk_widget_show(val_area);

	global_ustr.vol_scale = working;
	global_ustr.drawing_area = val_area;
	global_ustr.data = playlist;
	if (working) {	
		g_signal_connect (G_OBJECT (working), "motion_notify_event",
							G_CALLBACK(volume_move_event), &global_ustr);
		g_signal_connect (G_OBJECT (working), "button_press_event",
							G_CALLBACK(volume_move_event), &global_ustr);
	}
	g_signal_connect(G_OBJECT(val_area), "configure_event",
					   G_CALLBACK(val_area_configure), NULL);
	g_signal_connect(G_OBJECT(val_area), "expose_event",
					   G_CALLBACK(pixmap_expose), NULL);
					   
					   
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
		g_signal_connect (G_OBJECT (adj), "value_changed",
							G_CALLBACK(pan_cb), playlist);
		global_ustr.bal_scale = working;
		g_signal_connect (G_OBJECT (working), "motion_notify_event",
							G_CALLBACK(pan_move_event), &global_ustr);
		g_signal_connect (G_OBJECT (working), "button_press_event",
							G_CALLBACK(pan_move_event), &global_ustr);
		g_signal_connect (G_OBJECT(working), "button_release_event",
							G_CALLBACK(pan_release_event), &global_ustr);
	}

	working = get_widget(main_window, "playlist_button");
	pix = xpm_label_box(playlist_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	g_signal_connect(G_OBJECT(working), "clicked",
					   G_CALLBACK(playlist_cb), playlist_window_gtk); 
	gtk_button_set_relief(GTK_BUTTON(working), GTK_RELIEF_NONE);
	
	working = get_widget(main_window, "cd_button");
	pix = xpm_label_box(menu_xpm, main_window);
	gtk_widget_show(pix);
	gtk_container_add(GTK_CONTAINER(working), pix);
	//g_signal_connect(G_OBJECT(working), "clicked",
	//			G_CALLBACK(cd_cb), p);
	gtk_button_set_relief(GTK_BUTTON(working),GTK_RELIEF_NONE);

	working = get_widget(main_window, "info_box"); 

	gtk_widget_set_style(val_area, style);

	gtk_box_pack_start (GTK_BOX (working), val_area, true, true, 0);

	global_ustr.data = playlist;
	working = get_widget(main_window, "pos_scale");
	global_ustr.pos_scale = working;

	working = get_widget(main_window, "pos_scale");
	g_signal_connect(G_OBJECT(working), "button_release_event",
					   G_CALLBACK(release_event), &global_ustr);
	g_signal_connect (G_OBJECT (working), "button_press_event",
						G_CALLBACK(press_event), NULL);
	g_signal_connect (G_OBJECT (working), "motion_notify_event",
						G_CALLBACK(move_event), &global_ustr);


	global_ustr.speed_scale = speed_scale;
#if 1
	g_signal_connect (G_OBJECT (speed_scale), "motion_notify_event",
						G_CALLBACK(speed_move_event), &global_ustr);
	g_signal_connect (G_OBJECT (speed_scale), "button_press_event",
						G_CALLBACK(speed_move_event), &global_ustr);

	adj = GTK_RANGE(speed_scale)->adjustment;
	//gtk_adjustment_set_value(adj, 100.0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
						G_CALLBACK(speed_cb), playlist);
	#endif
	g_signal_connect(G_OBJECT(main_window), "delete_event", G_CALLBACK(main_window_delete), NULL);

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
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(scopes_cb), scopes_window);
	gtk_widget_show(menu_item);
	// Effects
	menu_item = gtk_menu_item_new_with_label("Effects...");
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(effects_cb), effects_window);
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
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(cd_cb), playlist);
#endif


	// Separator
	menu_item = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_widget_show(menu_item);
	// Exit
	menu_item = gtk_menu_item_new_with_label("Exit");
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(exit_cb), NULL);
	gtk_widget_show(menu_item);
	
	working = get_widget(main_window, "cd_button");
			   
	g_signal_connect(G_OBJECT(working), "event", G_CALLBACK(alsaplayer_button_press), (gpointer) root_menu);
	
	gdk_flush();

	//gdk_window_set_decorations(GTK_WIDGET(main_window)->window, (GdkWMDecoration)0);
	gtk_widget_show(GTK_WIDGET(main_window));

	// Check if we should open the playlist
	if (prefs_get_bool(ap_prefs, "gtk2_interface", "playlist_active", 0)) {
		playlist_window_gtk->Show();
	}	

	memset(&notifier, 0, sizeof(notifier));
	notifier.speed_changed = speed_changed;
	notifier.pan_changed = pan_changed;
	notifier.volume_changed = volume_changed;
	notifier.stop_notify = stop_notify;
	notifier.start_notify = start_notify;
	notifier.position_notify = position_notify;

	GDK_THREADS_LEAVE();
	playlist->RegisterNotifier(&notifier, NULL);
	GDK_THREADS_ENTER();
#if 0
	// Setup geometry stuff
	memset(&geom, 0, sizeof(geom));
	
	toplevel = gtk_widget_get_toplevel(main_window);

	geom.min_width = 408;
	geom.min_height = geom.max_height = 98;
	geom.max_width = 2048;	

	gtk_window_set_geometry_hints(GTK_WINDOW(toplevel),
		GTK_WIDGET(main_window), &geom,
		(GdkWindowHints)(GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE));
#endif

	gdk_flush();
}
