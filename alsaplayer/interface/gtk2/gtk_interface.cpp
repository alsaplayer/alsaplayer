/*  gtk2_interface.cpp - gtk+ callbacks, etc
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#if defined(HAVE_SYSTRAY) && HAVE_SYSTRAY == yes
#define COMP_SYSTRAY
#else
#undef COMP_SYSTRAY
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
/*
#include "pixmaps/f_play.xpm"
#include "pixmaps/r_play.xpm"
#include "pixmaps/pause.xpm"
#include "pixmaps/next.xpm"
#include "pixmaps/prev.xpm"
#include "pixmaps/stop.xpm"
#if 0
#include "pixmaps/eject.xpm"
#endif
#include "pixmaps/play.xpm"
#include "pixmaps/playlist.xpm"
#include "pixmaps/cd.xpm"
#include "pixmaps/menu.xpm"
#include "pixmaps/loop.xpm"
#include "pixmaps/looper.xpm"
*/
#include "pixmaps/note.xpm"
#include "pixmaps/volume_icon.xpm"
#include "pixmaps/balance_icon.xpm"

#include "PlaylistWindow.h"

// Include other things: (ultimate aim is to wrap this up into one nice file)
#include "CorePlayer.h"
#include "Playlist.h"
#include "AboutWindow.h"
#include "PreferencesWindow.h"
#include "ScopesWindow.h"
#include "control.h"
#ifdef COMP_SYSTRAY
#include "StatusIcon.h"
#endif

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

//static int vol_scales[] = {
//				0,1,2,4,7,12,18,26,35,45,56,69,83,100 };

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

static GdkPixbuf*
reverse_pic(GdkPixbuf *p)
{
	GdkPixbuf *n;
	
	n = gdk_pixbuf_flip(p, TRUE);
	return n;	
}

static void
ap_message_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	gtk_widget_destroy(widget);
}

#if 0
static void
ap_message_response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
	if (arg1 == GTK_RESPONSE_CLOSE)
		gtk_widget_destroy(GTK_WIDGET(dialog));	
} 
#endif
                                                        
void ap_message_error(GtkWidget *parent, const gchar *message)
{
	GtkWidget *md;

	md = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Error !"));
		
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(md), "%s", message);
	
	g_signal_connect(G_OBJECT(md), "delete-event", G_CALLBACK(ap_message_delete), NULL);
	g_signal_connect(G_OBJECT(md), "response", G_CALLBACK(ap_message_delete), NULL);
	
	gtk_widget_show_all(md);
}

void ap_message_warning(GtkWidget *parent, const gchar *message)
{
	GtkWidget *md;
	
	md = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, _("Warning !"));
	
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(md), "%s", message);

	g_signal_connect(G_OBJECT(md), "delete-event", G_CALLBACK(ap_message_delete), NULL);	
	g_signal_connect(G_OBJECT(md), "response", G_CALLBACK(ap_message_delete), NULL);
	gtk_widget_show_all(md);
}

gboolean ap_message_question(GtkWidget *parent, const gchar *message)
{
	GtkWidget *md;
	gboolean res = FALSE;
	
	md = gtk_message_dialog_new(GTK_WINDOW(parent), (GtkDialogFlags) (GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT), GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, _("Excuse me !"));
	
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(md), "%s", message);

	g_signal_connect(G_OBJECT(md), "delete-event", G_CALLBACK(ap_message_delete), NULL);
	
	gint response = gtk_dialog_run(GTK_DIALOG(md));
	
	if (response == GTK_RESPONSE_YES)
		res = TRUE;
	else
		res = FALSE;
	
	gtk_widget_destroy(md);
	
	return res;	
}

static gboolean
main_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	global_update = -1;

	PlaylistWindow *playlist_window = (PlaylistWindow *) g_object_get_data(G_OBJECT(widget), "playlist_window");
	
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

	vol ? str = g_strdup_printf(_("Volume: %d%%"), vol) : str = g_strdup_printf(_("Volume: mute"));
	
	infowindow->set_volume(str);
	
	g_free(str);
}

void draw_pan(float the_val)
{
	gchar *str;
	int pan = (int)(the_val * 100.0);

	if (pan < 0) {
		str = g_strdup_printf(_("Pan: left %d%%"), - pan);
	} else if (pan > 0) {
		str = g_strdup_printf(_("Pan: right %d%%"), pan);
	} else {
		str = g_strdup_printf(_("Pan: center"));
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
		str = g_strdup_printf(_("Speed: pause"));
	}
	else
		str = g_strdup_printf(_("Speed: %d%%  "), speed_val);
	
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

gboolean button_release_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if (event->type != GDK_SCROLL)
		return FALSE;
		
	GdkEventScroll *sevent = (GdkEventScroll *) event;
	GtkAdjustment *adj = GTK_RANGE(widget)->adjustment;
	
	gdouble value = gtk_adjustment_get_value(adj);
	if ((sevent->direction == GDK_SCROLL_UP) || (sevent->direction == GDK_SCROLL_RIGHT)) {
		gtk_adjustment_set_value(adj,  value+1.0); 
	} else if ((sevent->direction == GDK_SCROLL_DOWN) || (sevent->direction == GDK_SCROLL_LEFT)) {
		gtk_adjustment_set_value(adj,  value-1.0);
	}
	
	return TRUE;
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
//		draw_speed(val / 100.0);
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
	
	if (nice(5)) {
		/* no compilerwarning */
	}
	
	if (adj) {
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
	
	if (nice(5)) {
		/* no compiler warning */
	}
	
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
	GtkWidget *button = NULL;
	
	playlist = playlist_window->GetPlaylist();
	list = playlist_window->GetList();
	
	/* key definitions are from enum gtk_keymap */
  if (event->state & GDK_CONTROL_MASK) {
	switch(event->keyval) {
     case GDK_q:
      exit_cb(NULL,(gpointer) gtk_widget_get_toplevel(widget));
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
			play_cb(NULL, playlist_window);
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
			button = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "looper_button"));
			loop_cb(button, scale);
			break;
		
//old stuff		
		case GDK_Insert:
			playlist_window->AddFile();
			break;	
		case GDK_Delete:
			playlist_remove(NULL, user_data);
			break;
		case GDK_Return:
			playlist_play_current(list, playlist_window);
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
//		if (destination <= adj->value && destination != 0.0) {
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
	PlaylistWindow *playlist_window = (PlaylistWindow *)data;
	Playlist *pl = playlist_window->GetPlaylist();
	CorePlayer *p = pl->GetCorePlayer();
	if (p) {
		pl->UnPause();
		if (!pl->Length()) {
			eject_cb(widget, data);
		} else if (pl->Length()) {
			GDK_THREADS_LEAVE();
			pl->Play(pl->GetCurrent());
			GDK_THREADS_ENTER();
		}	
	}	
}


void eject_cb(GtkWidget *, gpointer data)
{
	PlaylistWindow *playlist_window = (PlaylistWindow *)data;
	Playlist *pl = playlist_window->GetPlaylist();
	CorePlayer *p = pl->GetCorePlayer();

	if ((p) && (!pl->Length())) {
		playlist_window->AddFile();
	}
}	


void volume_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj = (GtkAdjustment *)widget;
	Playlist *pl = (Playlist *)data;
	CorePlayer *p = pl->GetCorePlayer();

	if (p) {

		
	double volume = (double) p->GetVolume() * 100.0;
	if ((int)volume != (int)adj->value) {
		GDK_THREADS_LEAVE();	
		p->SetVolume(  (float) adj->value / 100.0);
		GDK_THREADS_ENTER();
	}
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
	adj = GTK_RANGE(ustr->vol_scale)->adjustment;
	double volume = (double) p->GetVolume() * 100.0;
	if ((int)volume != (int)gtk_adjustment_get_value(adj))
	{
		if (locking)
			GDK_THREADS_ENTER();
		gtk_adjustment_set_value(adj, volume);
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
		sprintf(info.title, _("No stream"));
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
		sprintf(title_string, "%s - %s", info.artist, info.title);
		infowindow->set_title(title_string);
		if (prefs_get_bool(ap_prefs, "gtk2_interface", "play_on_title", 0))
			gtk_window_set_title(GTK_WINDOW(gtk_widget_get_toplevel(playlist_window->GetWindow())), title_string);
	} else if (strlen(info.title)) {
		sprintf(title_string, "%s", info.title);
		infowindow->set_title(title_string);
		if (prefs_get_bool(ap_prefs, "gtk2_interface", "play_on_title", 0))
			gtk_window_set_title(GTK_WINDOW(gtk_widget_get_toplevel(playlist_window->GetWindow())), title_string);
	} else {
		char *p = strrchr(info.path, '/');
		if (p) {
			p++;
			infowindow->set_title(p);
			if (prefs_get_bool(ap_prefs, "gtk2_interface", "play_on_title", 0))
				gtk_window_set_title(GTK_WINDOW(gtk_widget_get_toplevel(playlist_window->GetWindow())), p);
		} else {
			infowindow->set_title(info.path);
			if (prefs_get_bool(ap_prefs, "gtk2_interface", "play_on_title", 0))
				gtk_window_set_title(GTK_WINDOW(gtk_widget_get_toplevel(playlist_window->GetWindow())), info.path);
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


void exit_cb(GtkWidget *, gpointer user_data)
{
	main_window_delete(GTK_WIDGET(user_data), NULL, NULL);
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

GtkWidget *get_image_from_xpm(const gchar *data[])
{
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_xpm_data(data);
	
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

static void
preferences_cb(GtkMenuItem *item, gpointer user_data)
{
	if (!GTK_WIDGET_VISIBLE(GTK_WIDGET(user_data)))
		gtk_widget_show_all(GTK_WIDGET(user_data));
	else
		gtk_widget_hide_all(GTK_WIDGET(user_data));
}

static void
about_cb(GtkMenuItem *item, gpointer user_data)
{
	if (!GTK_WIDGET_VISIBLE(GTK_WIDGET(user_data)))
		about_dialog_show(GTK_WIDGET(user_data));
	else
		gtk_widget_hide(GTK_WIDGET(user_data));
}

static GtkWidget*
create_main_menu(GtkWidget *main_window)
{
	GtkWidget *root_menu;
	GtkWidget *menu_item;
	
	GtkWidget *scopes_window = GTK_WIDGET(g_object_get_data(G_OBJECT(main_window), "scopes_window"));
	GtkWidget *about_window = GTK_WIDGET(g_object_get_data(G_OBJECT(main_window), "about_window"));
	GtkWidget *preferences_window = GTK_WIDGET(g_object_get_data(G_OBJECT(main_window), "preferences_window"));
	
	root_menu = gtk_menu_new();

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(preferences_cb), preferences_window);
	
	menu_item = gtk_menu_item_new_with_label(_("Scopes..."));
	gtk_menu_append(GTK_MENU(root_menu), menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate",
					   G_CALLBACK(scopes_cb), scopes_window);

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
					   G_CALLBACK(exit_cb), (gpointer)main_window);

	gtk_widget_show_all(GTK_WIDGET(root_menu));					   	
	
	return root_menu;	
}

void
loop_cb(GtkWidget *widget, gpointer data)
{
	GtkAdjustment *adj = GTK_RANGE(data)->adjustment;
	update_struct *ustr = &global_ustr;
	Playlist *pl = (Playlist *)ustr->data;
	loop_struct *loop = &global_loop;
	
	switch(loop->state) {
		case LOOP_OFF:
		{
			GtkWidget *pic = gtk_button_get_image(GTK_BUTTON(widget));
			GdkPixbuf *pb = gtk_widget_render_icon(pic, GTK_STOCK_GOTO_LAST, GTK_ICON_SIZE_MENU, NULL);
			GdkPixbuf *npb = reverse_pic(pb);
			g_object_unref(pb);
			pic = gtk_image_new_from_pixbuf(npb);
			g_object_unref(npb);
			gtk_button_set_image(GTK_BUTTON(widget), pic);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
			gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Set end of the looper"), NULL);
		}
			loop->track = pl->GetCurrent();
			loop->start = adj->value;
			loop->state = LOOP_START_SET;
			break;
		case LOOP_START_SET:
		{
			GtkWidget *pic = gtk_button_get_image(GTK_BUTTON(widget));
			GdkPixbuf *npb = gtk_widget_render_icon(pic, GTK_STOCK_GOTO_LAST, GTK_ICON_SIZE_MENU, NULL);
			pic = gtk_image_new_from_pixbuf(npb);
			g_object_unref(npb);
			gtk_button_set_image(GTK_BUTTON(widget), pic);
			gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Switch off looper"), NULL);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
		}			
			loop->end = adj->value;
			loop->state = LOOP_ON;
			pthread_create(&looper_thread, NULL,
					(void * (*)(void *))looper, adj);
			pthread_detach(looper_thread);
			break;
		case LOOP_ON:
			gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Set start of the looper"), NULL);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
		
			loop->state = LOOP_OFF;
			break;
		default:
			break;
	}

}

static void
loop_button_clicked(GtkWidget *widget, gpointer user_data)
{
	Playlist *pl = (Playlist *)user_data;

	if (pl->LoopingPlaylist()) {
		GtkWidget *pic = gtk_button_get_image(GTK_BUTTON(widget));
		GdkPixbuf *npb = gtk_widget_render_icon(pic, GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU, NULL);
		pic = gtk_image_new_from_pixbuf(npb);
		g_object_unref(npb);
		gtk_button_set_image(GTK_BUTTON(widget), pic);
		
		gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Switch off loop"), NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
		
		pl->UnLoopPlaylist();
		pl->LoopSong();
		
		prefs_set_int(ap_prefs, "gtk2_interface", "loop", 2);
	}
	else if (pl->LoopingSong()) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
		gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Play playlist in loop"), NULL);
		
		pl->UnLoopSong();
		prefs_set_int(ap_prefs, "gtk2_interface", "loop", 0);
	}
	else {
		GtkWidget *pic = gtk_button_get_image(GTK_BUTTON(widget));
		GdkPixbuf *pb = gtk_widget_render_icon(pic, GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU, NULL);
		GdkPixbuf *npb = reverse_pic(pb);
		g_object_unref(pb);
		pic = gtk_image_new_from_pixbuf(npb);
		g_object_unref(npb);
		gtk_button_set_image(GTK_BUTTON(widget), pic);
		
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
		gtk_tooltips_set_tip(GTK_TOOLTIPS(g_object_get_data(G_OBJECT(widget), "tooltips")), widget, _("Play song in loop"), NULL);
		pl->LoopPlaylist();
		prefs_set_int(ap_prefs, "gtk2_interface", "loop", 1);
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

static void
volume_button_cb(GtkButton *, gpointer user_data)
{
	static gdouble volume = 0.0;
	
	gdouble vol = gtk_adjustment_get_value(gtk_range_get_adjustment(GTK_RANGE(user_data)));
	
	if (vol != 0.0) {
		gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(user_data)), 0.0);
		volume = vol;
	}
	else
		gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(user_data)), volume);
}

static void
balance_button_cb(GtkButton *, gpointer user_data)
{
	gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(user_data)), 100.0);
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
	GtkWidget *balance_button;
	GtkWidget *bal_scale;
	GtkWidget *volume_box;
	GtkWidget *volume_button;
	GtkAdjustment *vol_adj;
	GtkWidget *vol_scale;
	GtkWidget *pic;
	GtkWidget *scopes_window;
	GtkWidget *about_window;
	GtkWidget *info_window;
	GtkWidget *menu;
	GtkTooltips *tooltips;
	GdkPixbuf *window_icon = NULL;
	GtkWidget *mini_button_box;
	GtkWidget *loop_button;
	GtkWidget *looper_button;
	GtkWidget *preferences_window;
	
	// Dirty trick
	playlist = pl;
	
	tooltips = gtk_tooltips_new();
			
	main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//	gtk_widget_set_size_request(main_window, 408, -1);
//	gtk_window_set_resizable(GTK_WINDOW (main_window), FALSE);

	window_icon = gdk_pixbuf_new_from_xpm_data((const char **)note_xpm);
	gtk_window_set_default_icon (window_icon);
	g_object_unref(G_OBJECT(window_icon));

	gtk_window_set_title(GTK_WINDOW(main_window), "AlsaPlayer");
	
	main_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (main_frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (main_window), main_frame);

	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (main_frame), main_box);

	info_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_box), info_box, FALSE, FALSE, 0);

	infowindow = new InfoWindow();
	info_window = infowindow->GetWindow();
	
	g_object_set_data(G_OBJECT(main_window), "info_window", infowindow);
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

	loop_button = gtk_toggle_button_new();
	g_object_set_data(G_OBJECT(main_window), "loop_button", loop_button);
	g_object_set_data(G_OBJECT(loop_button), "tooltips", tooltips);
//	pic = get_image_from_xpm(loop_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU);
	gtk_button_set_image(GTK_BUTTON(loop_button), pic);
	gtk_button_set_relief(GTK_BUTTON(loop_button),GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (mini_button_box), loop_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), loop_button, _("Play playlist in loop"), NULL);
	
	looper_button = gtk_toggle_button_new();
	g_object_set_data(G_OBJECT(main_window), "looper_button", looper_button);
	g_object_set_data(G_OBJECT(looper_button), "tooltips", tooltips);
//	pic = get_image_from_xpm(looper_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_GOTO_LAST, GTK_ICON_SIZE_MENU);
	gtk_button_set_image(GTK_BUTTON(looper_button), pic);
	gtk_button_set_relief(GTK_BUTTON(looper_button),GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (mini_button_box), looper_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), looper_button, _("Set start of the looper"), NULL);
	
	cd_button = gtk_button_new ();
//	pic = get_image_from_xpm(cd_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_CDROM, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(cd_button), pic);
	gtk_button_set_relief(GTK_BUTTON(cd_button),GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), cd_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), cd_button, _("Play CD"), NULL);
	
	prev_button = gtk_button_new();
//	pic = get_image_from_xpm(prev_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(prev_button), pic);
	gtk_button_set_relief(GTK_BUTTON(prev_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), prev_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), prev_button, _("Previous track"), _("Go to track before the current one on the list"));
	
	play_button = gtk_button_new ();
//	pic = get_image_from_xpm(play_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(play_button), pic);
	gtk_button_set_relief(GTK_BUTTON(play_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), play_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), play_button, _("Play"), _("Play current track on the list or open filechooser if list is empty"));

	stop_button = gtk_button_new ();
	g_object_set_data(G_OBJECT(main_window), "stop_button", stop_button);
//	pic = get_image_from_xpm(stop_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_MEDIA_STOP, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(stop_button), pic);
	gtk_button_set_relief(GTK_BUTTON(stop_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), stop_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), stop_button, _("Stop"), NULL);
		
	next_button = gtk_button_new ();
//	pic = get_image_from_xpm(next_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_MEDIA_NEXT, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(next_button), pic);
	gtk_button_set_relief(GTK_BUTTON(next_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), next_button, FALSE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), next_button, _("Next track"), _("Play the track after the current one on the list"));

	playlist_button = gtk_button_new ();
//	pic = get_image_from_xpm(playlist_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(playlist_button), pic);
	gtk_button_set_relief(GTK_BUTTON(playlist_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (button_box), playlist_button, FALSE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), playlist_button, _("Playlist window"), _("Manage playlist"));
	
	audio_control_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_scale_box), audio_control_box, TRUE, TRUE, 0);

	pitch_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (audio_control_box), pitch_box, FALSE, FALSE, 0);

	reverse_button = gtk_button_new ();
//	pic = get_image_from_xpm(r_play_xpm);
	pic = gtk_image_new();
	GdkPixbuf *pb = gtk_widget_render_icon(pic, GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_MENU, NULL);
	GdkPixbuf *npb = reverse_pic(pb);
	g_object_unref(pb);
	pic = gtk_image_new_from_pixbuf(npb);
	g_object_unref(npb);
	gtk_container_add(GTK_CONTAINER(reverse_button), pic);
	gtk_button_set_relief(GTK_BUTTON(reverse_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (pitch_box), reverse_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), reverse_button, _("Normal speed backwards"), _("Play track backwards with normal speed"));

	pause_button = gtk_button_new ();
//	pic = get_image_from_xpm(pause_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_MENU);
	gtk_container_add(GTK_CONTAINER(pause_button), pic);
	gtk_button_set_relief(GTK_BUTTON(pause_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (pitch_box), pause_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), pause_button, _("Pause/Unpause"), NULL);

	forward_button = gtk_button_new ();
//	pic = get_image_from_xpm(f_play_xpm);
	pic = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_MENU);
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
	
	balance_button = gtk_button_new();
	pic = get_image_from_xpm(balance_icon_xpm);
	gtk_container_add(GTK_CONTAINER(balance_button), pic); 
	gtk_button_set_relief(GTK_BUTTON(balance_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (bal_box), balance_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), balance_button, _("Center balance"), NULL);
	
	bal_scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (100, 0, 201, 1, 1, 1)));
	g_object_set_data(G_OBJECT(main_window), "bal_scale", bal_scale);
	gtk_adjustment_set_value(GTK_RANGE(bal_scale)->adjustment, 100.0);
	gtk_box_pack_start (GTK_BOX (bal_box), bal_scale, TRUE, TRUE, 0);
	gtk_scale_set_draw_value (GTK_SCALE (bal_scale), FALSE);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), bal_scale, _("Balance"), _("Change balance"));
	
	volume_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (bal_vol_box), volume_box, TRUE, TRUE, 0);

	volume_button = gtk_button_new();
	pic = get_image_from_xpm(volume_icon_xpm);
	gtk_container_add(GTK_CONTAINER(volume_button), pic); 
	gtk_button_set_relief(GTK_BUTTON(volume_button), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (volume_box), volume_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), volume_button, _("Mute/Unmute"), NULL);
	
	vol_adj = GTK_ADJUSTMENT(gtk_adjustment_new (100, 0, 101, 1, 1, 1));
	gtk_adjustment_set_value(vol_adj, (pl->GetCorePlayer())->GetVolume() * 100.0);
	vol_scale = gtk_hscale_new (vol_adj);
	g_object_set_data(G_OBJECT(main_window), "vol_scale", vol_scale);
	gtk_scale_set_draw_value (GTK_SCALE (vol_scale), FALSE);
	gtk_box_pack_start (GTK_BOX (volume_box), vol_scale, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), vol_scale, _("Volume"), _("Change volume"));
	
	playlist_window = new PlaylistWindow(playlist);
	g_object_set_data(G_OBJECT(main_window), "playlist_window", playlist_window);

	gtk_box_pack_start (GTK_BOX (main_box), playlist_window->GetWindow(), TRUE, TRUE, 0);
	
	scopes_window = init_scopes_window(main_window);
	g_object_set_data(G_OBJECT(main_window), "scopes_window", scopes_window);
	about_window = init_about_window(main_window);	
	g_object_set_data(G_OBJECT(main_window), "about_window", about_window);
	preferences_window = init_preferences_window(main_window);
	g_object_set_data(G_OBJECT(main_window), "preferences_window", preferences_window);
	
	menu = create_main_menu(main_window);
			
	global_ustr.vol_scale = vol_scale;
	global_ustr.drawing_area = info_window;
	global_ustr.data = playlist;
	global_ustr.pos_scale = pos_scale;
	global_ustr.speed_scale = speed_scale;
	global_ustr.bal_scale = bal_scale;

#ifdef COMP_SYSTRAY
		/* temp? init of staticon */
	gboolean staticon = status_icon_create(main_window);
#endif
	
	g_signal_connect(G_OBJECT(main_window), "expose-event", G_CALLBACK(configure_window), (gpointer)infowindow);

#ifdef COMP_SYSTRAY	
	if (staticon)	
		g_signal_connect(G_OBJECT(main_window), "delete_event", G_CALLBACK(main_window_delete), NULL);
	else
		g_signal_connect(G_OBJECT(main_window), "delete_event", G_CALLBACK(gtk_widget_hide), NULL);
#else
	g_signal_connect(G_OBJECT(main_window), "delete_event", G_CALLBACK(main_window_delete), NULL);
#endif
	g_signal_connect(G_OBJECT(main_window), "key_press_event", G_CALLBACK(key_press_cb), (gpointer)playlist_window);	
	g_signal_connect(G_OBJECT(main_window), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	
	g_signal_connect(G_OBJECT(volume_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(volume_button), "clicked", G_CALLBACK(volume_button_cb), (gpointer)vol_scale);
	g_signal_connect(G_OBJECT(balance_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);	
	g_signal_connect(G_OBJECT(balance_button), "clicked", G_CALLBACK(balance_button_cb), (gpointer) bal_scale);
	
	g_signal_connect(G_OBJECT(playlist_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
  	g_signal_connect(G_OBJECT(playlist_button), "clicked", G_CALLBACK(playlist_button_cb), playlist_window); 
	g_signal_connect(G_OBJECT(cd_button), "clicked", G_CALLBACK(cd_cb), pl);
	g_signal_connect(G_OBJECT(play_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(play_button), "clicked", G_CALLBACK(play_cb), (gpointer) playlist_window);
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
	g_signal_connect(G_OBJECT(vol_scale), "event", G_CALLBACK(button_release_event), NULL);
	g_signal_connect(G_OBJECT(pos_scale), "button_release_event", G_CALLBACK(release_event), playlist);
	g_signal_connect(G_OBJECT(pos_scale), "button_press_event", G_CALLBACK(press_event), NULL);
	g_signal_connect(G_OBJECT(pos_scale), "motion_notify_event", G_CALLBACK(move_event), NULL);
	g_signal_connect(G_OBJECT(GTK_RANGE(speed_scale)->adjustment), "value_changed", G_CALLBACK(speed_cb), playlist);
	g_signal_connect(G_OBJECT(speed_scale), "event", G_CALLBACK(button_release_event), NULL);
	g_signal_connect(G_OBJECT(speed_scale), "event", G_CALLBACK(button_release_event), NULL);
	g_signal_connect(G_OBJECT(cd_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(GTK_RANGE(bal_scale)->adjustment), "value_changed", G_CALLBACK(pan_cb), playlist);
	g_signal_connect(G_OBJECT(bal_scale), "event", G_CALLBACK(button_release_event), NULL);
	g_signal_connect(G_OBJECT(loop_button), "clicked", G_CALLBACK(loop_button_clicked), (gpointer)playlist);
	g_signal_connect(G_OBJECT(loop_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	g_signal_connect(G_OBJECT(looper_button), "clicked", G_CALLBACK(loop_cb), (gpointer)pos_scale);
	g_signal_connect(G_OBJECT(looper_button), "button_press_event", G_CALLBACK(alsaplayer_button_press), (gpointer) menu);
	
	return main_window;
}

void init_main_window(Playlist *pl)
{
	GtkWidget *main_window;
	gint width, height, plheight;
	
	main_window = create_main_window(pl);
	gtk_widget_show_all(main_window);
	
	PlaylistWindow *playlist_window = (PlaylistWindow *) g_object_get_data(G_OBJECT(main_window), "playlist_window");

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
	
	
	width = prefs_get_int(ap_prefs, "gtk2_interface", "width", 0);
	height = prefs_get_int(ap_prefs, "gtk2_interface", "height", 0);
	plheight = prefs_get_int(ap_prefs, "gtk2_interface", "playlist_height", 0);
	
	if (!prefs_get_bool(ap_prefs, "gtk2_interface", "playlist_active", 0)) {
		playlist_button_cb(main_window, playlist_window);
		playlist_window->SetHeight(plheight);
	}	
	
	if (width && height)	
		gtk_window_resize(GTK_WINDOW(main_window), width, height);
	
	int loop = prefs_get_int(ap_prefs, "gtk2_interface", "loop", 0);
	if (loop == 1) {
		gtk_button_clicked(GTK_BUTTON(g_object_get_data(G_OBJECT(main_window), "loop_button"))); 
	} else if (loop == 2) {
		gtk_button_clicked(GTK_BUTTON(g_object_get_data(G_OBJECT(main_window), "loop_button")));
		gtk_button_clicked(GTK_BUTTON(g_object_get_data(G_OBJECT(main_window), "loop_button")));
	}
	
	if (pl->Length() && pl->IsPaused()) {
		GDK_THREADS_LEAVE();
		playlist_window->CbSetCurrent(playlist_window, 1);	// hmmm should do
		GDK_THREADS_ENTER();
	} 
}
