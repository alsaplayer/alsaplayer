/*  barfft.cpp
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org>
 *  Based on code by Richard Boulton <richard@tartarus.org>
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
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <sys/time.h>
#include <time.h>   
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "scope_config.h"
#include "prefs.h"

#define BARS 16 

static GtkWidget *area = NULL;
static GdkRgbCmap *color_map = NULL;
static int fft_buf[512];
static int maxbar[BARS];
static GtkWidget *scope_win = NULL;
static int ready_state = 0;
static pthread_t fftscope_thread;
static pthread_mutex_t fftscope_mutex;
static int is_init = 0;
static int running = 0;

#if 0
static int xranges[] = {1, 2, 3, 4, 6, 8, 11, 15, 21,
			29, 40, 54, 74, 101, 137, 187, 255};
#endif
#if 0
static int xranges[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 13, 15, 17, 19, 21, 23, 25, 29, 33, 37, 40, 44, 48, 54, 61, 67, 74, 90, 101, 137, 158, 187, 255};
#endif
static int xranges[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};

static void fftscope_hide(void);
static int fftscope_running(void);

static const int default_colors[] = {
    10, 20, 30,
    230, 230, 230
};


void logscope_set_fft(void *fft_data, int samples, int channels)
{
	if (!fft_data) {
			memset(fft_buf, 0, sizeof(fft_buf));
			return;
	}
	memcpy(fft_buf, fft_data, sizeof(int) * samples * channels);
}

static void the_fftscope(void)
{
	guchar *loc;
	guchar bits [256 * 129];
	int i, h;

	running = 1;

	while (running) {
		guint val;
		gint j;
		gint k;

		memset(bits, 0, 256 * 128);

		for (i=0; i < BARS; i++) { 
			val = 0;
			for (j = xranges[i]; j < xranges[i + 1]; j++) {
				/* k = (guint)(sqrt(fftout[j]) * fftmult); */
				k = (fft_buf[j] + fft_buf[256+j]) / 256;
				val += k;
			}
			if(val > 127) val = 127;
			if (val > (guint)maxbar[ i ]) 
				maxbar[ i ] = val;
			else {
				k = maxbar[ i ] - (4 + (8 / (128 - maxbar[ i ])));
				val = k > 0 ? k : 0;
				maxbar[ i ] = val;
			}
			loc = bits + 256 * 128;
			for (h = val; h > 0; h--) {
				for (j = (256 / BARS) * i + 0; j < (256 / BARS) * i + ((256 / BARS) - 1); j++) {
					*(loc + j) = val-h;
				}
				loc -=256;
			}
		}
		GDK_THREADS_ENTER();
		gdk_draw_indexed_image(area->window, area->style->white_gc,
				0,0,256,128, GDK_RGB_DITHER_NONE, bits, 256, color_map);
		gdk_flush();
		GDK_THREADS_LEAVE();
		dosleep(SCOPE_SLEEP);
	}	
	GDK_THREADS_ENTER();
	fftscope_hide();
	GDK_THREADS_LEAVE();
}


static void stop_fftscope(void);

static gboolean close_fftscope_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GDK_THREADS_LEAVE();
	stop_fftscope();
	GDK_THREADS_ENTER();

	return TRUE;
}


static GtkWidget *init_fftscope_window(void)
{
	GtkWidget *fftscope_win;
	GdkColor color;
	guint32 colors[129];
	int i;
	
	pthread_mutex_init(&fftscope_mutex, NULL);

	fftscope_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(fftscope_win), "logFFTscope");
	gtk_widget_set_usize(fftscope_win, 256,128);
	gtk_window_set_policy (GTK_WINDOW (fftscope_win), FALSE, FALSE, FALSE);

	gtk_widget_realize(fftscope_win);
	
	color.red = SCOPE_BG_RED << 8;
	color.blue = SCOPE_BG_BLUE << 8;
	color.green = SCOPE_BG_GREEN << 8;
	gdk_color_alloc(gdk_colormap_get_system(), &color);
	
	colors[0] = 0;
	for (i = 1; i < 64; i++) {
		colors[i] = ((i*4) << 16) + (255 << 8);
		colors[i + 63] = (255 << 16) + (((63 - i) * 4) << 8);
  	}
	color_map = gdk_rgb_cmap_new(colors, 128);
	area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(fftscope_win), area);
	gtk_widget_realize(area);
	gdk_window_set_background(area->window, &color);
	
	gtk_widget_show(area);
	gtk_widget_show(fftscope_win);
	
	gtk_signal_connect(GTK_OBJECT(fftscope_win), "delete_event",
                GTK_SIGNAL_FUNC(close_fftscope_window), fftscope_win);


	ready_state = 1;

	return fftscope_win;
}


static void fftscope_hide(void)
{
	gint x, y;
	
	if (scope_win) {
		gdk_window_get_root_origin(scope_win->window, &x, &y);
		gtk_widget_hide(scope_win);
		gtk_widget_set_uposition(scope_win, x, y);
	}
}


static void stop_fftscope(void)
{
	if (running) {
		running = 0;
		pthread_join(fftscope_thread, NULL);
	}	
}


static void run_fftscope(void *data)
{
	nice(SCOPE_NICE); /* Be nice to most processes */

	the_fftscope();

	pthread_mutex_unlock(&fftscope_mutex);
	pthread_exit(NULL);
}


static void start_fftscope(void)
{
	if (!is_init) {
		is_init = 1;
		scope_win = init_fftscope_window();
	}
	if (pthread_mutex_trylock(&fftscope_mutex) != 0) {
		printf("fftscope already running\n");
		return;
	}
	gtk_widget_show(scope_win);
	pthread_create(&fftscope_thread, NULL, (void * (*)(void *))run_fftscope, NULL);
}


static int init_fftscope(void *arg)
{
	int i;
	
	for (i = 0; i < BARS; i++) {
		maxbar[ i ] = 0;
	}	
	
	if (prefs_get_bool(ap_prefs, "logbarfft", "active", 0))
		start_fftscope();
	
	return 1;
}

static void shutdown_fftscope(void)
{
	prefs_set_bool(ap_prefs, "logbarfft", "active", fftscope_running());

	if (fftscope_running())
		stop_fftscope();
}

static int fftscope_running(void)
{
	return running;
}

scope_plugin logscope_plugin = {
	SCOPE_PLUGIN_VERSION,
	"logFFTscope",
	"Andy Lo A Foe",
	NULL,
	init_fftscope,
	start_fftscope,
	fftscope_running,
	stop_fftscope,
	shutdown_fftscope,
	NULL,
	logscope_set_fft,
};


scope_plugin *scope_plugin_info(void)
{
	return &logscope_plugin;
}

