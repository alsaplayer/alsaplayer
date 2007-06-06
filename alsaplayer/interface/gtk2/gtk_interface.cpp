/*  gtk2_interface.cpp - gtk+ callbacks, etc
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#ifdef ENABLE_NLS
#define _(String) gettext(String)
#define N_(String) noop_gettext(String)
#else
#define _(String) (String)
#define N_(String) String
#endif

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
#include "pixmaps/cd.xpm"
#include "pixmaps/menu.xpm"
#include "pixmaps/note.xpm"
#include "pixmaps/loop.xpm"
#include "pixmaps/looper.xpm"

#include "PlaylistWindow.h"

// Include other things: (ultimate aim is to wrap this up into one nice file)
#include "CorePlayer.h"
#include "Playlist.h"
#include "EffectsWindow.h"
#include "AboutWindow.h"
//#include "Effects.h"
#include "ScopesWindow.h"
#include "control.h"

#include "info_window.h"

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

static PlaylistWindow *playlist_window = NULL;
static coreplayer_notifier notifier;

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

static int vol_scales[] = {
				0,1,2,4,7,12,18,26,35,45,56,69,83,100 };

#ifdef SUBSECOND_DISPLAY
#define INDICATOR_WIDTH 85
#else
#define INDICATOR_WIDTH 69
#endif

InfoWindow *infowindow;

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
	PlaylistWindow *playlist_window = (PlaylistWindow *) data;
	playlist_window->SetStop();
}

void start_notify(void *data)
{
//	PlaylistWindow::SetCurrentCb does it so this is useless
//	PlaylistWindow *playlist_window = (PlaylistWindow *) data;
//	playlist_window->SetPlay();
}


gboolean main_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	global_update = -1;

	PlaylistWindow *playlist_window = (PlaylistWindow *) data;
	
	prefs_set_int(ap_prefs, "gtk2_interface", "width", widget->allocation.width);
	prefs_set_int(ap_prefs, "gtk2_interface", "height", widget->allocation.height);
	
	// Remove notifier
	
	gdk_flush();
	
	if (playlist_window) {
		Playlist *playlist = playlist_window->GetPlaylist();
		GDK_THREADS_LEAVE();
		playlist->UnRegisterNotifier(&notifier);
		GDK_THREADS_ENTER();
		delete playlist_window;
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

void draw_volume(float the_vol)
{
	gchar *str;

	int vol = (int) (the_vol * 100); // quick hack

	vol ? str = g_strdup_printf("Volume: %d%%", vol) : str = g_strdup_printf("Volume: mute");
	
	infowindow->set_volume(str);
	
	g_free(str);
}

void draw_pan(float the_val)
{
	gchar *str;
	int pan = (int)(the_val * 100.0);

	if (pan < 0) {
		str = g_strdup_printf("Pan: left %d%%", - pan);
	} else if (pan > 0) {
		str = g_strdup_printf("Pan: right %d%%", pan);
	} else {
		str = g_strdup_printf("Pan: center");
	} 
	
	infowindow->set_balance(str);
	
	g_free(str);
}


void draw_speed(float speed)
{
	gchar *str;
	int speed_val;

	speed_val = (int)(speed * 100.0); // We need percentages
	if (speed_val < ZERO_PITCH_TRESH && speed_val > -ZERO_PITCH_TRESH) {
		str = g_strdup_printf("Speed: pause");
	}
	else
		str = g_strdup_printf("Speed: %d%%  ", speed_val);
	
	infowindow->set_speed(str);
	
	g_free(str);
}	


gboolean release_event(GtkWidget *widget, GdkEvent *, gpointer data)
{
	GtkAdjustment *adj;
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	adj = GTK_RANGE(widget)->adjustment;
	p->Seek((int)adj->value);
	global_update = 1;
	
	return FALSE;
}


gboolean move_event(GtkWidget *, GdkEvent *, gpointer)
{
	indicator_callback(NULL, 0);
	return FALSE;
}

void speed_cb(GtkWidget *widget, gpointer data)
{
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();
	double val =  GTK_ADJUSTMENT(widget)->value;
	if (val < ZERO_PITCH_TRESH && val > -ZERO_PITCH_TRESH)
		val = 0;
	double speed = (double) p->GetSpeed() * 100.0;
	if ((int)speed != (int)val) {
		GDK_THREADS_LEAVE();	
		p->SetSpeed(  (float) val / 100.0);
		GDK_THREADS_ENTER();
	}
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
			 global_update = 1;	
		} 

		dosleep(10000);
	}
	pthread_mutex_unlock(&looper_mutex);
	pthread_exit(NULL);
}

static gboolean
key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GtkAdjustment *adj;

	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	Playlist *playlist = NULL;
	GtkWidget *list = NULL;
	GtkWidget *scale = NULL;

	playlist = playlist_window->GetPlaylist();
	list = playlist_window->GetList();
	
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
			stop_cb(NULL, playlist);
			break;
		case PLAY_KEY:
			play_cb(NULL, playlist);
			break;
		case PAUSE_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "speed_scale"));
			pause_cb(NULL, scale);
			break;
		case NEXT_KEY:
			playlist_window->PlayNext();
			break;
		case PREV_KEY:
			playlist_window->PlayPrev();
			break;
		case FWD_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "pos_scale"));
			forward_skip_cb(NULL, scale);
			break;
		case BACK_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "pos_scale"));
			reverse_skip_cb(NULL, scale);
			break;
		case FWD_PLAY_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "speed_scale"));
			forward_play_cb(NULL, scale);
			break;
		case REV_PLAY_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "speed_scale"));
			reverse_play_cb(NULL, scale);
			break;
		case SPEED_UP_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "speed_scale"));
			adj = GTK_RANGE(scale)->adjustment;
			gtk_adjustment_set_value(adj, EQ_TEMP_STEP(adj->value, 1));
			break;
		case SPEED_DOWN_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "speed_scale"));
			adj = GTK_RANGE(scale)->adjustment;
			gtk_adjustment_set_value(adj, EQ_TEMP_STEP(adj->value, -1));
			break;
		case SPEED_COMMA_UP_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "speed_scale"));
			adj = GTK_RANGE(scale)->adjustment;
			gtk_adjustment_set_value(adj, EQ_TEMP_STEP(adj->value, 0.234600103846));
			break;
		case SPEED_COMMA_DOWN_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "speed_scale"));
			adj = GTK_RANGE(scale)->adjustment;
			gtk_adjustment_set_value(adj, EQ_TEMP_STEP(adj->value, -0.234600103846));
			break;
		case VOL_UP_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "vol_scale"));
      		adj = GTK_RANGE(scale)->adjustment;
      		gtk_adjustment_set_value(adj, adj->value + 0.5);
			break;
		case VOL_DOWN_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "vol_scale"));
      		adj = GTK_RANGE(scale)->adjustment;
      		gtk_adjustment_set_value(adj, adj->value - 0.5);
			break;
		case LOOP_KEY:
			scale = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "pos_scale"));
			loop_cb(NULL, scale);
			break;
		
//old stuff		
/*		case GDK_Insert:
			dialog_popup(widget, (gpointer)
				playlist_window_gtk->add_file);
			break;	
*/		case GDK_Delete:
			playlist_remove(NULL, user_data);
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
		destination = 100;
		pthread_create(&smoother_thread, NULL,
				(void * (*)(void *))smoother, adj);
		pthread_detach(smoother_thread);
	} else {
		gtk_adjustment_set_value(adj, 100);
	}	
}


void reverse_play_cb(GtkWidget *, gpointer data)
{
	GtkAdjustment *adj;
	int smooth_trans;

	smooth_trans = prefs_get_bool(ap_prefs, "gtk2_interface", "smooth_transition", 0);

	adj = GTK_RANGE(data)->adjustment;

	if (smooth_trans) {
		destination = -100.0;
		pthread_create(&smoother_thread, NULL,
				(void * (*)(void *))smoother, adj);
		pthread_detach(smoother_thread);
	} else {
		gtk_adjustment_set_value(adj, -100);
	}	
}


void pause_cb(GtkWidget *, gpointer data)
{
	GtkAdjustment *adj;
	int smooth_trans;

	adj = GTK_RANGE(data)->adjustment;
			
	smooth_trans = prefs_get_bool(ap_prefs, "gtk2_interface", "smooth_transition", 0);
		
	if (smooth_trans) {
		if (destination <= adj->value && destination != 0.0) {
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
		gtk_widget_show_all(play_dialog);
// what for ?		gdk_window_raise(play_dialog->window);
	}
}	


void volume_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj = (GtkAdjustment *)widget;
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if (p) {
		GDK_THREADS_LEAVE();
		p->SetVolume(((float) adj->value) / 100.0);
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
	GdkColor color;
	stream_info info;
	char title_string[256];
	char str[60];
	long slider_val=0, t_min=0, t_sec=0;
	long c_hsec=0, secs=0, c_min=0, c_sec=0;
	long sr=0;
	int nr_frames=0;
//	static char old_str[60] = "";

	ustr = &global_ustr;
	pl = (Playlist *)ustr->data;
	p = pl->GetCorePlayer();
	drawable = ustr->drawing_area->window;

	adj = GTK_RANGE(ustr->speed_scale)->adjustment;
	double speed = (double) p->GetSpeed() * 100.0;
	if ((int)speed != (int)gtk_adjustment_get_value(adj))
	{
		if (locking)
			GDK_THREADS_ENTER();
		gtk_adjustment_set_value(adj, speed);
		if (locking)
				GDK_THREADS_LEAVE();
	}
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
	if (locking)
			GDK_THREADS_ENTER();
	infowindow->set_position(str);
	if (locking)
			GDK_THREADS_LEAVE();
			
	if (locking)
		GDK_THREADS_ENTER();
	infowindow->set_format(info.stream_type);
	if (strlen(info.artist)) {
		sprintf(title_string, "%s - %s", info.title, info.artist);
		infowindow->set_title(title_string);
	} else if (strlen(info.title)) {
		sprintf(title_string, "%s", info.title);
		infowindow->set_title(title_string);
	} else {
		char *p = strrchr(info.path, '/');
		if (p) {
			p++;
			infowindow->set_title(p);
		} else {
			infowindow->set_title(info.path);
		}	
	}

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

void scopes_cb(GtkWidget *, gpointer user_data)
{
	GtkWidget *win = (GtkWidget *) user_data;

		if (GTK_WIDGET_VISIBLE(win)){
			gtk_widget_hide(win);
		}
	else 
		gtk_widget_show_all(win);

}

/*
void effects_cb(GtkWidget *, gpointer user_data)
{
	GtkWidget *win = (GtkWidget *) user_data;

		if (GTK_WIDGET_VISIBLE(win)){
			gtk_widget_hide(win);
		}
	else 
		gtk_widget_show_all(win);
}
*/

void play_file_ok(GtkWidget *play_dialog, gpointer data)
{
	Playlist *playlist = (Playlist *)data;
	CorePlayer *p = playlist->GetCorePlayer();

	if (p) {

		GSList *file_list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(play_dialog));
	
			std::vector<std::string> paths;
		char *path;
		
		// Write default_play_path
//		prefs_set_string(ap_prefs, "gtk2_interface", "default_play_path", current_dir);
	
		// Get the selections
		if (!file_list) {
			path = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(play_dialog));
			if (path) {
				paths.push_back(path);
				g_free(path);
			}
		}
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
}

void dialog_cancel_response(GtkWidget *dialog, gpointer data)
{
// do we really need placing window ?
//	gint x,y;

//	gdk_window_get_root_origin(play_dialog->window, &x, &y);
	gtk_widget_hide(dialog);
//	gtk_widget_set_uposition(play_dialog, x, y);
}

static void
playlist_button_cb(GtkWidget *button, gpointer user_data)
{
	PlaylistWindow *pl = (PlaylistWindow *) user_data;
	GtkWidget *window = gtk_widget_get_toplevel(button);
	GdkGeometry geometry;
	
	if (pl->IsHidden()) {
		pl->Show();
		gtk_window_resize(GTK_WINDOW(window), window->allocation.width, window->allocation.height + pl->GetHeight());
		
		geometry.max_width =  65535;
		geometry.max_height = 65535;
		
		gtk_window_set_geometry_hints(GTK_WINDOW(window),
			GTK_WIDGET(window), &geometry, GDK_HINT_MAX_SIZE);
			
	}
	else {
		pl->Hide();
		gtk_window_resize(GTK_WINDOW(window), window->allocation.width, 1);
		
		geometry.max_width =  65535;
		geometry.max_height = -1;
		
		gtk_window_set_geometry_hints(GTK_WINDOW(window),
			GTK_WIDGET(window), &geometry, GDK_HINT_MAX_SIZE);
	}
		
}

gboolean alsaplayer_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button == 3) {
		gtk_menu_popup (GTK_MENU (data), NULL, NULL, NULL, NULL,
						event->button, event->time);
		return true;
	}
	return false;
}

GtkWidget *get_image_from_xpm(gchar *data[])
{
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)data);
	
	image = gtk_image_new_from_pixbuf(pixbuf);
	
	return image;
}

void
update_info_window(GtkWidget *main_window)
{
	GtkWidget *speed = GTK_WIDGET(g_object_get_data(G_OBJECT(main_window), "speed_scale"));
	gdouble speed_val = gtk_adjustment_get_value(gtk_range_get_adjustment(GTK_RANGE(speed)));

	GtkWidget *vol = GTK_WIDGET(g_object_get_data(G_OBJECT(main_window), "vol_scale"));
	gdouble vol_val = gtk_adjustment_get_value(gtk_range_get_adjustment(GTK_RANGE(vol)));
		
	draw_speed(speed_val/100.0);
	draw_volume(vol_val/100.0);
	
	indicator_callback(NULL, 0);
}

void play_dialog_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
		play_file_ok(GTK_WIDGET(dialog), (gpointer) user_data);
	
	dialog_cancel_response(GTK_WIDGET(dialog), NULL);
}	

void about_cb(GtkMenuItem *item, gpointer user_data)
{
	if (!GTK_WIDGET_VISIBLE(GTK_WIDGET(user_data)))
		about_dialog_show(GTK_WIDGET(user_data));
	else
		gtk_widget_hide(GTK_WIDGET(user_data));
}

GtkWidget*
create_main_menu(GtkWidget *main_window)
{
	GtkWidget *root_menu;
	GtkWidget *menu_item;
	
	GtkWidget *scopes_window = GTK_WIDGET(g_object_get_data(G_OBJECT(main_window), "scopes_window"));
//	GtkWidget *effects_window = GTK_WIDGET(g_object_get_data(G_OBJECT(main_window), "effects_window"));
	GtkWidget *about_window = GTK_WIDGET(g_object_get_data(G_OBJECT(main_window), "about_window"));
	
	root_menu = gtk_menu_new();

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	gtk_widget_set_sensitive(menu_item, FALSE);
	
	menu_item = gtk_menu_item_new_with_label(_("Scopes..."));
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(scopes_cb), scopes_window);

	menu_item = gtk_menu_item_new_with_label(_("Effects..."));
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
//	g_signal_connect(G_OBJECT(menu_item), "activate",
//					   G_CALLBACK(effects_cb), effects_window);
	gtk_widget_set_sensitive(menu_item, FALSE);

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(about_cb), about_window);

	menu_item = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(root_menu), menu_item);

	menu_item = gtk_menu_item_new_with_label(_("CD Player (CDDA)"));
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(cd_cb), playlist);

	menu_item = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(root_menu), menu_item);

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(exit_cb), NULL);

	gtk_widget_show_all(GTK_WIDGET(root_menu));					   	
	
	return root_menu;	
}

void loop_button_clicked(GtkWidget *widget, gpointer user_data)
{
	Playlist *pl = (Playlist *)user_data;

	if (pl->LoopingPlaylist()) {
		gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Switch off loop"), NULL);
		pl->UnLoopPlaylist();
		pl->LoopSong();
	}
	else if (pl->LoopingSong()) {
		gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Play playlist in loop"), NULL);
		pl->UnLoopSong();
	}
	else {
		gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Play song in loop"), NULL);
		pl->LoopPlaylist();
	}
}

static gboolean 
configure_window (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	InfoWindow* info = (InfoWindow *)user_data;
	
	info->set_positions();
			
	return FALSE;	
} 

static void
prev_button_clicked(GtkButton *, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	
	if(playlist_window)
		playlist_window->PlayPrev();
}

static void
next_button_clicked(GtkButton *, gpointer user_data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *) user_data;
	
	if(playlist_window)
		playlist_window->PlayNext();
}

GtkWidget*
create_main_window (Playlist *pl)
{
	GtkWidget *main_window;
	GtkWidget *main_frame;
	GtkWidget *main_box;
	GtkWidget *info_box;
	GtkWidget *pos_scale;
	GtkWidget *button_scale_box;
	GtkWidget *control_box;
	GtkWidget *button_box;
	GtkWidget *cd_button;
	GtkWidget *prev_button;
	GtkWidget *play_button;
	GtkWidget *stop_button;
	GtkWidget *next_button;
	GtkWidget *playlist_button;
	GtkWidget *audio_control_box;
	GtkWidget *pitch_box;
	GtkWidget *reverse_button;
	GtkWidget *pause_button;
	GtkWidget *forward_button;
	GtkWidget *speed_scale;
	GtkWidget *bal_vol_box;
	GtkWidget *bal_box;
	GtkWidget *balance_pic;
	GtkWidget *bal_scale;
	GtkWidget *volume_box;
	GtkWidget *volume_pic;
	GtkAdjustment *vol_adj;
	GtkWidget *vol_scale;
	GtkWidget *pic;
	GtkWidget *effects_window;
	GtkWidget *scopes_window;
	GtkWidget *about_window;
	GtkWidget *info_window;
	GtkWidget *menu;
	GtkTooltips *tooltips;
	GdkPixbuf *window_icon = NULL;
	GtkWidget *mini_button_box;
	GtkWidget *loop_button;
	GtkWidget *looper_button;

	// Dirty trick
	playlist = pl;
	
	tooltips = gtk_tooltips_new();
			
	main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//	gtk_widget_set_size_request(main_window, 408, -1);
//	gtk_window_set_resizable(GTK_WINDOW (main_window), FALSE);

	window_icon = gdk_pixbuf_new_from_xpm_data((const char **)note_xpm);
	gtk_window_set_default_icon (window_icon);
	g_object_unref(G_OBJECT(window_icon));
		
	gtk_window_set_title(GTK_WINDOW(main_window), global_session_name == NULL ? "AlsaPlayer" : global_session_name);
	gtk_window_set_wmclass(GTK_WINDOW(main_window), "AlsaPlayer", "alsaplayer");

	main_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (main_frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (main_window), main_frame);

	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (main_frame), main_box);

	info_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_box), info_box, FALSE, FALSE, 0);

	infowindow = new InfoWindow();
	info_window = infowindow->GetWindow();
	
	g_object_set_data(G_OBJECT(main_window), "info_window", info_window);
	gtk_box_pack_start (GTK_BOX (info_box), info_window, TRUE, TRUE, 0);
	
	pos_scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 0, 0, 0, 0)));
	g_object_set_data(G_OBJECT(main_window), "pos_scale", pos_scale);
	gtk_box_pack_start (GTK_BOX (main_box), pos_scale, FALSE, FALSE, 0);
	gtk_scale_set_draw_value (GTK_SCALE (pos_scale), FALSE);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), pos_scale, _("Position control"), _("Set position of the song")); 
	
	button_scale_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_box), button_scale_box, FALSE, FALSE, 0);

	control_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_scale_box), control_box, FALSE, FALSE, 0);

	button_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (control_box), button_box, FALSE, FALSE, 0);

	mini_button_box = gtk_vbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (button_box), mini_button_box, FALSE, FALSE, 0);

	loop_button = gtk_button_new();
	g_object_set_data(G_OBJECT(loop_button), "tooltips", tooltips);
	pic = get_image_from_xpm(loop_xpm);
	gtk_container_add(GTK_CONTAINER(loop_button), pic);
	gtk_button_set_relief(GTK_BUTTON(loop_button),GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (mini_button_box), loop_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), loop_button, _("Play playlist in loop"), NULL);
	
	looper_button = gtk_button_new();
	pic = get_image_from_xpm(looper_xpm);
	gtk_container_add(GTK_CONTAINER(looper_button), pic);
	gtk_button_set_relief(GTK_BUTTON(looper_button),GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (mini_button_box), looper_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), looper_button, _("Use looper"), NULL);
	
	cd_button = gtk_button_new ();
	pic = get_image_from_xpm(cd_xpm);
	gtk_container_add(GTK_CONTAINER(cd_button), pic);
	gtk_button_set_relief(GTK_BUTTON(cd_button),GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), cd_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), cd_button, _("Play CD"), NULL);
	
	prev_button = gtk_button_new();
	pic = get_image_from_xpm(prev_xpm);
	gtk_container_add(GTK_CONTAINER(prev_button), pic);
	gtk_button_set_relief(GTK_BUTTON(prev_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), prev_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), prev_button, _("Previous track"), _("Go to track before the current one on the list"));
	
	play_button = gtk_button_new ();
	pic = get_image_from_xpm(play_xpm);
	gtk_container_add(GTK_CONTAINER(play_button), pic);
	gtk_button_set_relief(GTK_BUTTON(play_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), play_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), play_button, _("Play"), _("Play current track on the list or open filechooser if list is empty"));

	stop_button = gtk_button_new ();
	pic = get_image_from_xpm(stop_xpm);
	gtk_container_add(GTK_CONTAINER(stop_button), pic);
	gtk_button_set_relief(GTK_BUTTON(stop_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), stop_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), stop_button, _("Stop"), NULL);
		
	next_button = gtk_button_new ();
	pic = get_image_from_xpm(next_xpm);
	gtk_container_add(GTK_CONTAINER(next_button), pic);
	gtk_button_set_relief(GTK_BUTTON(next_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), next_button, FALSE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), next_button, _("Next track"), _("Play the track after the current one on the list"));

	playlist_button = gtk_button_new ();
	pic = get_image_from_xpm(playlist_xpm);
	gtk_container_add(GTK_CONTAINER(playlist_button), pic);
	gtk_button_set_relief(GTK_BUTTON(playlist_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), playlist_button, FALSE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), playlist_button, _("Playlist window"), _("Manage playlist"));
	
	audio_control_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_scale_box), audio_control_box, TRUE, TRUE, 0);

	pitch_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (audio_control_box), pitch_box, FALSE, FALSE, 0);

	reverse_button = gtk_button_new ();
	pic = get_image_from_xpm(r_play_xpm);
	gtk_container_add(GTK_CONTAINER(reverse_button), pic);
	gtk_button_set_relief(GTK_BUTTON(reverse_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (pitch_box), reverse_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), reverse_button, _("Normal speed backwards"), _("Play track backwards with normal speed"));

	pause_button = gtk_button_new ();
	pic = get_image_from_xpm(pause_xpm);
	gtk_container_add(GTK_CONTAINER(pause_button), pic);
	gtk_button_set_relief(GTK_BUTTON(pause_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (pitch_box), pause_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), pause_button, _("Pause/Unpause"), NULL);

	forward_button = gtk_button_new ();
	pic = get_image_from_xpm(f_play_xpm);
	gtk_container_add(GTK_CONTAINER(forward_button), pic); 
	gtk_button_set_relief(GTK_BUTTON(forward_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (pitch_box), forward_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), forward_button, _("Normal speed"), _("Play track normally"));

	speed_scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (100, -400, 401, 1, 1, 1)));
	g_object_set_data(G_OBJECT(main_window), "speed_scale", speed_scale);
	gtk_box_pack_start (GTK_BOX (pitch_box), speed_scale, TRUE, TRUE, 0);
	gtk_scale_set_draw_value (GTK_SCALE (speed_scale), FALSE);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), speed_scale, _("Speed control"), _("Change playback speed"));

	bal_vol_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (audio_control_box), bal_vol_box, TRUE, FALSE, 0);

	bal_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (bal_vol_box), bal_box, TRUE, TRUE, 0);

	balance_pic = get_image_from_xpm(balance_icon_xpm);
	gtk_box_pack_start (GTK_BOX (bal_box), balance_pic, FALSE, FALSE, 0);

	bal_scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (100, 0, 201, 1, 1, 1)));
	g_object_set_data(G_OBJECT(main_window), "bal_scale", bal_scale);
	gtk_adjustment_set_value(GTK_RANGE(bal_scale)->adjustment, 100.0);
	gtk_box_pack_start (GTK_BOX (bal_box), bal_scale, TRUE, TRUE, 0);
	gtk_scale_set_draw_value (GTK_SCALE (bal_scale), FALSE);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), bal_scale, _("Balance"), _("Change balance"));
	
	volume_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (bal_vol_box), volume_box, TRUE, TRUE, 0);

	volume_pic = get_image_from_xpm(volume_icon_xpm);
	gtk_box_pack_start (GTK_BOX (volume_box), volume_pic, FALSE, FALSE, 0);

	vol_adj = GTK_ADJUSTMENT(gtk_adjustment_new (100, 0, 101, 1, 1, 1));
	gtk_adjustment_set_value(vol_adj, (pl->GetCorePlayer())->GetVolume() * 100.0);
	vol_scale = gtk_hscale_new (vol_adj);
	g_object_set_data(G_OBJECT(main_window), "vol_scale", vol_scale);
	gtk_scale_set_draw_value (GTK_SCALE (vol_scale), FALSE);
	gtk_box_pack_start (GTK_BOX (volume_box), vol_scale, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), vol_scale, _("Volume"), _("Change volume"));
	
	playlist_window = new PlaylistWindow(playlist);
	g_object_set_data(G_OBJECT(main_window), "playlist_window", playlist_window);
	play_dialog = NULL;//create_filechooser(GTK_WINDOW(main_window), playlist);
	
	gtk_box_pack_start (GTK_BOX (main_box), playlist_window->GetWindow(), TRUE, TRUE, 0);
	
//	effects_window = init_effects_window();	
//	g_object_set_data(G_OBJECT(main_window), "effects_window", effects_window);
	effects_window = NULL;
	scopes_window = init_scopes_window(main_window);
	g_object_set_data(G_OBJECT(main_window), "scopes_window", scopes_window);
	about_window = init_about_window(main_window);	
	g_object_set_data(G_OBJECT(main_window), "about_window", about_window);

	menu = create_main_menu(main_window);
			
	global_ustr.vol_scale = vol_scale;
	global_ustr.drawing_area = info_window;
	global_ustr.data = playlist;
	global_ustr.pos_scale = pos_scale;
	global_ustr.speed_scale = speed_scale;
	global_ustr.bal_scale = bal_scale;

	g_signal_connect(G_OBJECT(main_window), "expose-event", G_CALLBACK(configure_window), (gpointer)infowindow);
		
	g_signal_connect(G_OBJECT(main_window), "delete_event", G_CALLBACK(main_window_delete), (gpointer)playlist_window);
	g_signal_connect(G_OBJECT(main_window), "key_press_event", G_CALLBACK(key_press_cb), (gpointer)playlist_window);	
	g_signal_connect(G_OBJECT(main_window), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	
	g_signal_connect(G_OBJECT(playlist_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
  	g_signal_connect(G_OBJECT(playlist_button), "clicked", G_CALLBACK(playlist_button_cb), playlist_window); 
	g_signal_connect(G_OBJECT(cd_button), "clicked", G_CALLBACK(cd_cb), pl);
	g_signal_connect(G_OBJECT(play_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(play_button), "clicked", G_CALLBACK(play_cb), playlist);
	g_signal_connect(G_OBJECT(stop_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(stop_button), "clicked", G_CALLBACK(stop_cb), playlist);
	g_signal_connect(G_OBJECT(next_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(next_button), "clicked", G_CALLBACK(next_button_clicked), playlist_window);
	g_signal_connect(G_OBJECT(prev_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(prev_button), "clicked", G_CALLBACK(prev_button_clicked), playlist_window);
	g_signal_connect(G_OBJECT(reverse_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(reverse_button), "clicked", G_CALLBACK(reverse_play_cb), speed_scale);
	g_signal_connect(G_OBJECT(pause_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(pause_button), "clicked", G_CALLBACK(pause_cb), speed_scale);
	g_signal_connect(G_OBJECT(forward_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(forward_button), "clicked", G_CALLBACK(forward_play_cb), speed_scale);
	g_signal_connect(G_OBJECT(vol_adj), "value_changed", G_CALLBACK(volume_cb), playlist);
	g_signal_connect(G_OBJECT(pos_scale), "button_release_event", G_CALLBACK(release_event), playlist);
	g_signal_connect(G_OBJECT(pos_scale), "button_press_event", G_CALLBACK(press_event), NULL);
	g_signal_connect(G_OBJECT(pos_scale), "motion_notify_event", G_CALLBACK(move_event), NULL);
	g_signal_connect(G_OBJECT(GTK_RANGE(speed_scale)->adjustment), "value_changed", G_CALLBACK(speed_cb), playlist);	
	g_signal_connect(G_OBJECT(cd_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(GTK_RANGE(bal_scale)->adjustment), "value_changed", G_CALLBACK(pan_cb), playlist);
	g_signal_connect(G_OBJECT(loop_button), "clicked", G_CALLBACK(loop_button_clicked), (gpointer)playlist);
	g_signal_connect(G_OBJECT(loop_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(looper_button), "clicked", G_CALLBACK(loop_cb), (gpointer)pos_scale);
	g_signal_connect(G_OBJECT(looper_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);

	infowindow->set_background_color("#000000");
	infowindow->set_font_color("#ffffff");
	
	return main_window;
}

void init_main_window(Playlist *pl)
{
	GtkWidget *main_window;
	gint width, height;
	
	main_window = create_main_window(pl);
	gtk_widget_show_all(main_window);
	
	PlaylistWindow *playlist_window = (PlaylistWindow *) g_object_get_data(G_OBJECT(main_window), "playlist_window");
	
	if (!prefs_get_bool(ap_prefs, "gtk2_interface", "playlist_active", 0)) {
		playlist_button_cb(main_window, playlist_window);
	}	

	width = prefs_get_int(ap_prefs, "gtk2_interface", "width", 0);
	height = prefs_get_int(ap_prefs, "gtk2_interface", "height", 0);
	
	if (width && height)	
		gtk_window_resize(GTK_WINDOW(main_window), width, height);

	memset(&notifier, 0, sizeof(notifier));
	notifier.speed_changed = speed_changed;
	notifier.pan_changed = pan_changed;
	notifier.volume_changed = volume_changed;
	notifier.stop_notify = stop_notify;
	notifier.start_notify = start_notify;
	notifier.position_notify = position_notify;

	GDK_THREADS_LEAVE();
	playlist->RegisterNotifier(&notifier, (void *)playlist_window);
	GDK_THREADS_ENTER();
}
