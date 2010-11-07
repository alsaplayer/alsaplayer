/*  monoscope.h
 *  Copyright (C) 1998-2002 Andy Lo A Foe <andy@alsaplayer.org>
 *  Original code by Tinic Uro
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
 *  $Id: monoscope.c 1017 2003-11-09 13:28:30Z adnans $
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
#include "alsaplayer_convolve.h"
#include "alsaplayer_error.h"

short newEq[CONVOLVE_BIG];	/* latest block of 512 samples. */
static short copyEq[CONVOLVE_BIG];
int avgEq[CONVOLVE_SMALL];	/* a running average of the last few. */
int avgMax;			/* running average of max sample. */

static GtkWidget *area = NULL;
static GtkWidget *scope_win = NULL;
static GdkRgbCmap *color_map = NULL;
static int ready_state = 0;
static pthread_t monoscope_thread;
static pthread_mutex_t monoscope_mutex;
static pthread_mutex_t update_mutex;
static int is_init = 0;
static int running = 0;
static convolve_state *state = NULL;

static void monoscope_hide(void);
static int monoscope_running(void);

static const int default_colors[] = {
	10, 20, 30,
	230, 230, 230
};

void monoscope_set_data(void *audio_buffer, int size)
{
	int i;	
	short *sound = (short *)audio_buffer;

	if (pthread_mutex_trylock(&update_mutex) != 0) {
		/* alsaplayer_error("missing an update"); */
		return;
	}	
	if (!sound) {
		memset(&newEq, 0, sizeof(newEq));
		pthread_mutex_unlock(&update_mutex);
		return;
	}	
	if (running && size >= CONVOLVE_BIG) {
		short * newset = newEq;
		int skip = (size / (CONVOLVE_BIG * 2)) * 2;
		for (i = 0; i < CONVOLVE_BIG; i++) {
			*newset++ = (((int) sound[0]) + (int) sound[1]) >> 1;
			sound += skip;
		}
	}
	pthread_mutex_unlock(&update_mutex);
}


void the_monoscope()
{
	int foo;
	int bar;  
	int i, h;
	guchar bits[ 257 * 129];
	guchar *loc;

	running = 1;

	while (running) {
		int factor;
		int val;
		int max = 1;
		short * thisEq;
		pthread_mutex_lock(&update_mutex);
		memcpy (copyEq, newEq, sizeof (short) * CONVOLVE_BIG);
		thisEq = copyEq;
#if 1					
		val = convolve_match (avgEq, copyEq, state);
		thisEq += val;
#endif	
		pthread_mutex_unlock(&update_mutex);
		memset(bits, 0, 256 * 128);
		for (i=0; i < 256; i++) {
			foo = thisEq[i] + (avgEq[i] >> 1);
			avgEq[i] = foo;
			if (foo < 0)
				foo = -foo;
			if (foo > max)
				max = foo;
		}
		avgMax += max - (avgMax >> 8);
		if (avgMax < max)
			avgMax = max; /* Avoid overflow */
		factor = 0x7fffffff / avgMax;
		/* Keep the scaling sensible. */
		if (factor > (1 << 18))
			factor = 1 << 18;
		if (factor < (1 << 8))
			factor = 1 << 8;
		for (i=0; i < 256; i++) {
			foo = avgEq[i] * factor;
			foo >>= 18;
			if (foo > 63)
				foo = 63;
			if (foo < -64)
				foo = -64;
			val = (i + ((foo+64) << 8));
			bar = val;
			if ((bar > 0) && (bar < (256 * 128))) {
				loc = bits + bar;
				if (foo < 0) {
					for (h = 0; h <= (-foo); h++) {
						*loc = h;
						loc+=256; 
					}
				} else {
					for (h = 0; h <= foo; h++) {
						*loc = h;
						loc-=256;
					}
				}
			}
		}
		for (i=16;i < 128; i+=16) {
			for (h = 0; h < 256; h+=2) {
				bits[(i << 8) + h] = 63;
				if (i == 64)
					bits[(i << 8) + h + 1] = 63;
			}
		}
		for (i = 16; i < 256; i+=16) {
			for (h = 0; h < 128; h+=2) {
				bits[i + (h << 8)] = 63;
			}
		}
		GDK_THREADS_ENTER();
		gdk_draw_indexed_image(area->window,area->style->white_gc,
				0, 0, 256, 128, GDK_RGB_DITHER_NONE, bits, 256, color_map);
		gdk_flush();
		GDK_THREADS_LEAVE();
		dosleep(SCOPE_SLEEP);
	}

	GDK_THREADS_ENTER();
	monoscope_hide();
	gdk_flush();
	GDK_THREADS_LEAVE();
}



void stop_monoscope(void);

static gboolean close_monoscope_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GDK_THREADS_LEAVE();		
	stop_monoscope();
	GDK_THREADS_ENTER();

	return TRUE;
}


GtkWidget *init_monoscope_window(void)
{
	GtkWidget *monoscope_win;
	GdkColor color;
	guint32 colors[65];
	int i;

	pthread_mutex_init(&monoscope_mutex, NULL);
	pthread_mutex_init(&update_mutex, NULL);

	monoscope_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(monoscope_win), "Monoscope");
	gtk_widget_set_usize(monoscope_win, 256,128);
	gtk_window_set_policy (GTK_WINDOW (monoscope_win), FALSE, FALSE, FALSE);

	gtk_widget_realize(monoscope_win);

	color.red = SCOPE_BG_RED << 8;
	color.blue = SCOPE_BG_BLUE << 8;
	color.green = SCOPE_BG_GREEN << 8;
	gdk_color_alloc(gdk_colormap_get_system(), &color);

	colors[0] = 0;
	for (i = 1; i < 32; i++) {
		colors[i] = (i*8 << 16) +(255 << 8);
		colors[i+31] = (255 << 16) + (((31 - i) * 8) << 8);
	}
	colors[63] = (40 << 16) + (75 << 8);
	color_map = gdk_rgb_cmap_new(colors, 64);
	area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(monoscope_win), area);
	gtk_widget_realize(area);
	gdk_window_set_background(area->window, &color);

	gtk_widget_show(area);
	gtk_widget_show(monoscope_win);

	/* Signals */

	gtk_signal_connect(GTK_OBJECT(monoscope_win), "delete_event",
			GTK_SIGNAL_FUNC(close_monoscope_window), monoscope_win);


	ready_state = 1;

	return monoscope_win;
}


void monoscope_hide(void)
{
	gint x, y;

	if (scope_win) {
		gdk_window_get_root_origin(scope_win->window, &x, &y);
		gtk_widget_hide(scope_win);
		gtk_widget_set_uposition(scope_win, x, y);
	} else {
		printf("Tried to hide destroyed widget!\n");
	}	
}


void stop_monoscope(void)
{
	if (running) {
		running = 0;
		pthread_join(monoscope_thread, NULL);
	}	
}


void run_monoscope(void *data)
{
	nice(SCOPE_NICE); /* Be nice to most processes */

	the_monoscope();

	pthread_mutex_unlock(&monoscope_mutex);
	pthread_exit(NULL);
}


void start_monoscope(void)
{
	if (!is_init) {
		is_init = 1;
		scope_win = init_monoscope_window();
	}
	if (pthread_mutex_trylock(&monoscope_mutex) != 0) {
		printf("monoscope already running\n");
		return;
	}
	gtk_widget_show(scope_win);
	pthread_create(&monoscope_thread, NULL, (void * (*)(void *))run_monoscope, NULL);
}


static int init_monoscope(void *arg)
{
	state = convolve_init();
	if(!state) return 0;

	/* FIXME - Need to call convolve_close(state); at some point
	 * if this is going to become a proper plugin. */

	if (prefs_get_bool(ap_prefs, "monoscope", "active", 0))
		start_monoscope();

	return 1;
}

static void shutdown_monoscope(void)
{
	prefs_set_bool(ap_prefs, "monoscope", "active", monoscope_running());

	if (monoscope_running())
		stop_monoscope();
	/*	
		if (state)
		*/						
}

static int monoscope_running(void)
{
	return running;
}

scope_plugin monoscope_plugin = {
	SCOPE_PLUGIN_VERSION,
	"Monoscope",
	"Andy Lo A Foe",
	NULL,
	init_monoscope,
	start_monoscope,
	monoscope_running,
	stop_monoscope,
	shutdown_monoscope,
	monoscope_set_data,
	NULL
};


scope_plugin *scope_plugin_info(void)
{
	return &monoscope_plugin;
}	
