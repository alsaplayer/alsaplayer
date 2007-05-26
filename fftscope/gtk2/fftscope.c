/*  fftscope.c
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
 *  Copyright (C) 1999 Richard Boulton <richard@tartarus.org>
 *  Original code by Tinic Uro
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
 *	2002-03-30: converted to use indexed pixmap
 * 
 *  $Id: fftscope.c 1119 2007-05-21 19:30:33Z dominique_libre $
 *  
*/
#include <pthread.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include "prefs.h"
#include "alsaplayer/scope_plugin.h"
#include "alsaplayer/utilities.h"

#define SCOPE_HEIGHT	128 /* Only tested for 64 and 128 */

static int act_fft[512];
static GdkImage *image = NULL;
static GtkWidget *scope_win = NULL;
static GdkRgbCmap *color_map = NULL;
static GtkWidget *area = NULL;
static int ready_state = 0;
static pthread_t fftscope_thread;
static pthread_mutex_t fftscope_mutex;
static int is_init = 0;
static int running = 0;

static void fftscope_hide();
static void stop_fftscope();

static const int default_colors[] = {
	10, 20, 30,
	230, 230, 230
};

/*
 * Important note: the set_fft and set_data calls should NEVER contain
 * calls that can block. These functions are called from the main audio
 * thread so they should ideally not do any heavy computation and definitely
 * not contain blocking calls.
 * 
 * fft_buffer is an int array with samples * channels integers
 * All fft samples are grouped per channel so the data is NON-interleaved
 *
 */
void fftscope_set_fft(void *fft_buffer, int samples, int channels)
{
	if (!fft_buffer || (samples * channels) > 512) {
		memset(act_fft, 0, sizeof(act_fft));
		return;
	}
	/* Just store data for later use inside the render thread */
	memcpy(act_fft, fft_buffer, (sizeof(int)) * samples * channels);
}


/* The actual FFTscope renderer function. */
static void the_fftscope()
{
	guint8 *loc;
	guint8 bits[256 * 129];
	int i, h;

	running = 1;

	while (running) {
		int w;
		guint val;

		memset(bits, 128, 256 * SCOPE_HEIGHT);

		for (i = 0; i < 256; i++) {
			val = (act_fft[i] + act_fft[i + 256]) / (64 * (128 / SCOPE_HEIGHT));
			if (val > (SCOPE_HEIGHT-1)) {
				val = (SCOPE_HEIGHT-1);
			}
			loc = bits + i + 256 * (SCOPE_HEIGHT-1);
			for (h = val; h > 0; h--) {
				*loc = h;
				loc -= 256;
			}
		}
		GDK_THREADS_ENTER();
		gdk_draw_indexed_image(area->window, area->style->white_gc,
				       0, 0, 256, SCOPE_HEIGHT, GDK_RGB_DITHER_NONE,
				       bits, 256, color_map);
		gdk_flush();
		GDK_THREADS_LEAVE();
		dosleep(SCOPE_SLEEP);
	}
	GDK_THREADS_ENTER();
	fftscope_hide();
	GDK_THREADS_LEAVE();
}


static gboolean
close_fftscope_window(GtkWidget * widget, GdkEvent * event, gpointer data)
{
	GDK_THREADS_LEAVE();
	stop_fftscope();
	GDK_THREADS_ENTER();

	return TRUE;
}


/* Init function. Here we setup all the gtk stuff */
static GtkWidget *init_fftscope_window()
{
	GtkWidget *fftscope_win;
	GtkStyle *style;
	GdkColor color;
	guint32 colors[129];
	int i;

	pthread_mutex_init(&fftscope_mutex, NULL);

	style = gtk_style_new();
	fftscope_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(fftscope_win), "FFTscope");
	gtk_widget_set_usize(fftscope_win, 256, SCOPE_HEIGHT);
	gtk_window_set_wmclass(GTK_WINDOW(fftscope_win), "FFTscope",
			       "AlsaPlayer");
	gtk_window_set_policy(GTK_WINDOW(fftscope_win), FALSE, FALSE,
			      FALSE);
	style =
	    gtk_style_copy(gtk_widget_get_style(GTK_WIDGET(fftscope_win)));

	color.red = SCOPE_BG_RED << 8;
	color.blue = SCOPE_BG_BLUE << 8;
	color.green = SCOPE_BG_GREEN << 8;
	gdk_color_alloc(gdk_colormap_get_system(), &color);
	gtk_widget_set_style(GTK_WIDGET(fftscope_win), style);

	for (i = 0; i < 32; i++) {
		colors[i * 2] = colors[i * 2 + 1] =
		    ((i * 8) << 16) + (255 << 8);
		colors[i * 2 + 64] = colors[i * 2 + 65] =
		    (255 << 16) + (((31 - i) * 8) << 8);
	}
	colors[128] = 0;
	color_map = gdk_rgb_cmap_new(colors, 129);
	area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(fftscope_win), area);
	gtk_widget_realize(area);
	gdk_window_set_background(area->window, &color);

	gtk_widget_show(area);
	gtk_widget_show(fftscope_win);

	/* Signals */

	gtk_signal_connect(GTK_OBJECT(fftscope_win), "delete_event",
			   GTK_SIGNAL_FUNC(close_fftscope_window),
			   fftscope_win);


	/* Clear and show the window */
	gdk_window_clear(fftscope_win->window);
	gdk_flush();

	ready_state = 1;

	return fftscope_win;
}


static void fftscope_hide()
{
	gint x, y;

	if (scope_win) {
		gdk_window_get_root_origin(scope_win->window, &x, &y);
		gtk_widget_hide(scope_win);
		gtk_widget_set_uposition(scope_win, x, y);
	}
}

/* Called to stop the FFTscope window */
static void stop_fftscope()
{
	running = 0;

	pthread_join(fftscope_thread, NULL);
}


/* This is our main looper */
static void run_fftscope(void *data)
{
	nice(SCOPE_NICE);	// Be nice to most processes

	the_fftscope();
	pthread_mutex_unlock(&fftscope_mutex);
	pthread_exit(NULL);
}


/* Called to startup the FFTscope */
static void start_fftscope()
{
	if (!is_init) {
		scope_win = init_fftscope_window();
		is_init = 1;
	}
	if (pthread_mutex_trylock(&fftscope_mutex) != 0) {
		printf("fftscope already running\n");
		return;
	}
	gtk_widget_show(scope_win);
	pthread_create(&fftscope_thread, NULL,
		       (void *(*)(void *)) run_fftscope, NULL);
}


/* Init function */
static int init_fftscope()
{
	int i;

	memset(act_fft, 0, sizeof(act_fft));

	if (prefs_get_bool(ap_prefs, "fftscope", "active", 0))
		start_fftscope();
	
	return 1;
}

/* Are we running? */
static int fftscope_running()
{
	return running;
}


/* Shutdown function. Should deallocate stuff here too */
static void shutdown_fftscope(void)
{
	prefs_set_bool(ap_prefs, "fftscope", "active", fftscope_running());

	if (fftscope_running()) {
		stop_fftscope();
	}
}


/* The C struct that contains all our functions */
static scope_plugin fftscope_plugin;


/* Our entry point so the HOST can get to the above struct */
scope_plugin *scope_plugin_info()
{
	memset(&fftscope_plugin, 0, sizeof(scope_plugin));
	fftscope_plugin.version = SCOPE_PLUGIN_VERSION;
	fftscope_plugin.name = "FFTscope";
	fftscope_plugin.author = "Andy Lo-A-Foe, Richard Boulton & Dominique Michel";
	fftscope_plugin.init = init_fftscope;
	fftscope_plugin.start = start_fftscope;
	fftscope_plugin.running = fftscope_running;
	fftscope_plugin.stop = stop_fftscope;
	fftscope_plugin.shutdown = shutdown_fftscope;
	fftscope_plugin.set_fft = fftscope_set_fft;
	/* We don't assign set_data since it's not needed for this plugin */
	return &fftscope_plugin;
}
