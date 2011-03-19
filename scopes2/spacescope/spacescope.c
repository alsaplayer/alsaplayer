/*  spacescope.c
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
 */ 
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
#include <pthread.h>
#include "scope_config.h"
#include "prefs.h"

#define SPACE_WH	128	/* Leave this at 128 for now */

static char actEq[257];
static char oldEq[257];       

static char scX[257];
static char scY[257];     

static GtkWidget *area = NULL;
static GtkWidget *scope_win = NULL;
static GdkRgbCmap *color_map = NULL;
static pthread_t spacescope_thread;
static pthread_mutex_t spacescope_mutex;
static gint is_init = 0;
static gint running = 0;


static void spacescope_hide(void);
static int spacescope_running(void);

void spacescope_set_data(void *audio_buffer, int size)
{
	int i;
	short *sound = (short *)audio_buffer;

	if (!sound) {
		memset(&actEq, 0, sizeof(actEq));
		return;
	}		
	if (running && sound) {
		char *newset = actEq;
		int bufsize = size / (size >= 512 ? 512 : size);
		for (i=0; i < 256; i++) {
			*newset++=(char)(((int)(*sound)+(int)(*(sound+1)))>>10);
			sound += bufsize;
		}
	}
}


void the_spacescope(void)
{
	gint foo, bar;
	char *oldset = oldEq;
	char *newset = actEq;
	guchar bits[(SPACE_WH+1) * (SPACE_WH+1)], *loc;
	int i;

	while (running) {
		memset(bits, 0, SPACE_WH * SPACE_WH);
		memcpy(oldset,newset,256);

		for (i=0; i < 256; i++) {
			foo = (1+(oldset[i]+64))>>1;
			bar = ((( scX[i]*foo)>>7 )+(64)) +
				((((scY[i]*foo) ) + (64 * SPACE_WH)) & 0xffffff80);
			if ((bar > 0) && (bar < (SPACE_WH * SPACE_WH))) {
				loc = bits + bar;
				*loc = foo;
			}
		}
		GDK_THREADS_ENTER();
		gdk_draw_indexed_image(area->window,area->style->white_gc,
				0, 0, SPACE_WH, SPACE_WH, GDK_RGB_DITHER_NONE, bits, SPACE_WH, color_map);
		GDK_THREADS_LEAVE();
		dosleep(SCOPE_SLEEP);
	}
	GDK_THREADS_ENTER();
	spacescope_hide();
	GDK_THREADS_LEAVE();	 
}

void stop_spacescope(void);

static gboolean close_spacescope_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GDK_THREADS_LEAVE();				
	stop_spacescope();
	GDK_THREADS_ENTER();

	return TRUE;
}



GtkWidget *init_spacescope_window(void)
{
	GtkWidget *spacescope_win;
	GdkColor color;
	guint32 colors[65];
	int i;

	pthread_mutex_init(&spacescope_mutex, NULL);

	spacescope_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(spacescope_win), "Spacescope");
	gtk_widget_set_usize(spacescope_win, SPACE_WH, SPACE_WH);	
	gtk_window_set_policy (GTK_WINDOW (spacescope_win), FALSE, FALSE, TRUE);  

	gtk_widget_realize(spacescope_win);
	color.red = SCOPE_BG_RED << 8;
	color.blue = SCOPE_BG_BLUE << 8;
	color.green = SCOPE_BG_GREEN << 8;
	gdk_color_alloc(gdk_colormap_get_system(), &color);
	colors[0] = 0;
	for (i = 1; i < 32; i++) {
		colors[i] = (i*8 << 16) + (255*8 << 8);
		colors[i+31] = (255*8 << 16) + ((31 - i)*8 << 8);
	}
	colors[63] = (255*8 << 16);
	color_map = gdk_rgb_cmap_new(colors, 64);
	area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(spacescope_win), area);
	gtk_widget_realize(area);
	gdk_window_set_background(area->window, &color);

	gtk_widget_show(area);
	gtk_widget_show(spacescope_win);

	/* Signals */

	gtk_signal_connect(GTK_OBJECT(spacescope_win), "delete_event",
			GTK_SIGNAL_FUNC(close_spacescope_window), spacescope_win);


	/* Create sin/cos tables */

	for (i = 0; i < 256; i++) {
		scX[i] = (char) (sin(((2*M_PI)/255)*i)*128);
		scY[i] = (char) (-cos(((2*M_PI)/255)*i)*128);
	} 
	return spacescope_win;
}  

void spacescope_hide(void)
{
	gint x, y;

	if (scope_win) {
		gdk_window_get_root_origin(scope_win->window, &x, &y);	
		gtk_widget_hide(scope_win);
		gtk_widget_set_uposition(scope_win, x, y);	
	}
}

void stop_spacescope(void)
{
	if (running) {
		running = 0;
		pthread_join(spacescope_thread, NULL);
	}	
}

void run_spacescope(void *data)
{
	nice(SCOPE_NICE);
	the_spacescope();
	pthread_mutex_unlock(&spacescope_mutex);
	pthread_exit(NULL);
}


void start_spacescope(void)
{
	if (!is_init) {
		is_init = 1;
		scope_win = init_spacescope_window();
	}
	if (pthread_mutex_trylock(&spacescope_mutex) != 0) {
		printf("spacescope already running\n");
		return;
	}		 
	running = 1;
	gtk_widget_show(scope_win);
	pthread_create(&spacescope_thread, NULL, (void * (*)(void *))run_spacescope, NULL);
}


static int init_spacescope(void *arg)
{
	if (prefs_get_bool(ap_prefs, "spacescope", "active", 0))
		start_spacescope();
	return 1;
}

static void shutdown_spacescope(void)
{
	prefs_set_bool(ap_prefs, "spacescope", "active", spacescope_running());

	if (spacescope_running()) {
		stop_spacescope();
	}
	if (area) {
		gtk_widget_destroy(area);
		area = NULL;
	}
	if (scope_win) {
		gtk_widget_destroy(scope_win);
		scope_win = NULL;
	}				
}


static int spacescope_running(void)
{
	return running;
}


scope_plugin spacescope_plugin = {
	SCOPE_PLUGIN_VERSION,
	"Spacescope",
	"Andy Lo A Foe",
	NULL,
	init_spacescope,
	start_spacescope,
	spacescope_running,
	stop_spacescope,
	shutdown_spacescope,
	spacescope_set_data,
	NULL
};


scope_plugin *scope_plugin_info(void)
{
	return &spacescope_plugin;
}		
