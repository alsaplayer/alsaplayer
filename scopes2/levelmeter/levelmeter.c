/*  levelmeter.c
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
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include "scope_config.h"
#include "prefs.h"

#define DECAY	3

static char actlEq[257];
static char oldlEq[257];       
static char actrEq[257];
static char oldrEq[257];

static char scX[256];
static char scY[256];     

static GtkWidget *scope_win = NULL;
static GtkWidget *area = NULL;
static GdkPixmap *draw_pixmap = NULL, *disp = NULL;
static GdkGC *gc = NULL;
static pthread_t levelmeter_thread;
static pthread_mutex_t levelmeter_mutex;
static gint is_init = 0;
static gint running = 0;/* this global variable determines when
			   and if we should exit the thread loop.
			   we should always exit gracefully from
			   the looper thread or else we might hold
		           the global GDK thread lock (a very shitty hack)
                           and lock our whole app */


static void levelmeter_hide(void);
static int levelmeter_running(void);

static void levelmeter_set_data(void *audio_buffer, int size)
{
	short i;
	short *sound = (short *)audio_buffer;
	
	if (running && sound) {
		char *newsetl = actlEq;
	        char *newsetr = actrEq;
		int bufsize=size/512;
        	for(i=0;i<256;i++) {
                	*newsetl++=(char)((int)(*(sound  ))>>8);
                	*newsetr++=(char)((int)(*(sound+1))>>8);
                	sound+=bufsize;
        	}
        }
}


static void the_levelmeter(GtkWidget *win)
{
	static int oldl = 0;
	static int oldr = 0;
	char *oldsetl = oldlEq;
	char *newsetl = actlEq;
	char *oldsetr = oldrEq;
	char *newsetr = actrEq;
	int levell, levelr; 
	int maxl = 0, maxr = 0, count = 0; 
	int i;

	running = 1;
	
	while (running) {
		memcpy(oldsetl, newsetl, 256);
		memcpy(oldsetr, newsetr, 256);

		count++;

		if (count > 30) {
			count = 0;
			maxl = 0;
			maxr = 0;
		}
		levell = 0;
		for (i = 0; i < 256; i++) {
			if (oldsetl[i] > 0) {
				levell=MAX(levell,oldsetl[i]);
			} else {
				levell=MAX(levell,-oldsetl[i]);
			}
		}

		levelr = 0;
		for (i = 0; i < 256; i++) {
			if (oldsetr[i] > 0) {
				levelr=MAX(levelr, oldsetr[i]);
			} else {
				levelr=MAX(levelr,-oldsetr[i]);		
			}
		}
		levelr >>= 1;
		levell >>= 1;

		if (oldr > 0 && (oldr-=2) > levelr)
				levelr = oldr;
		else oldr = levelr;

		if (oldl > 0 && (oldl-=2) > levell)
				levell = oldl;
		else oldl = levell;
		
		if (maxl < levell<<2) {
			maxl = (levell<<2)-4;
			count = 0;
		}
		if (maxr < levelr<<2) {
			maxr = (levelr<<2)-4;
			count = 0;
		}
		GDK_THREADS_ENTER();
		gdk_draw_rectangle(draw_pixmap,gc,TRUE,0,0,256,40);
		gdk_draw_pixmap(draw_pixmap,gc,disp,0,0,0,1,levell<<2,18);
		gdk_draw_pixmap(draw_pixmap,gc,disp,maxl,0,maxl,1,4,18);
		gdk_draw_pixmap(draw_pixmap,gc,disp,0,0,0,21,levelr<<2,18);
		gdk_draw_pixmap(draw_pixmap,gc,disp,maxr,0,maxr,21,4,18);
		gdk_draw_pixmap(area->window,gc,draw_pixmap,0,0,0,0,256,40);
		gdk_flush();
		GDK_THREADS_LEAVE();
		dosleep(SCOPE_SLEEP);
	}
	GDK_THREADS_ENTER();
	levelmeter_hide();
	gdk_flush();
	GDK_THREADS_LEAVE();
}


static void stop_levelmeter(void);

static gboolean close_levelmeter_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GDK_THREADS_LEAVE();			
	stop_levelmeter();
	GDK_THREADS_ENTER();
	
	return TRUE;
}


static void popup(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	//printf("Bla\n");
}


static GtkWidget *init_levelmeter_window(void)
{
	GtkWidget *levelmeter_win;
	GdkColor color, col;
	int i;
	
	pthread_mutex_init(&levelmeter_mutex, NULL);

	levelmeter_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(levelmeter_win), "Levelmeter");
	gtk_widget_set_usize(levelmeter_win, 255, 40);	
	gtk_window_set_policy (GTK_WINDOW (levelmeter_win), FALSE, FALSE, FALSE);  
	gtk_widget_set_events(levelmeter_win, GDK_BUTTON_PRESS_MASK);	
	gtk_widget_realize(levelmeter_win);

	gc = gdk_gc_new(levelmeter_win->window);
	
	if (!gc)
		return NULL;
	color.red = SCOPE_BG_RED << 8;
	color.blue = SCOPE_BG_BLUE << 8;
	color.green = SCOPE_BG_GREEN << 8;
	gdk_color_alloc(gdk_colormap_get_system(), &color);

	gint depth = gdk_visual_get_system()->depth; 
//	draw_pixmap = gdk_pixmap_new(levelmeter_win->window, 256,40, gdk_visual_get_best_depth());			
//	disp = gdk_pixmap_new(levelmeter_win->window, 256, 18, gdk_visual_get_best_depth());
	draw_pixmap = gdk_pixmap_new(levelmeter_win->window, 256,40, depth);
	disp = gdk_pixmap_new(levelmeter_win->window, 256, 18, depth);
	for (i = 0; i < 256; i+=4) {
		if (i < 128) {	
			col.red = (i<<1) << 8;
			col.green = 255  << 8;
			col.blue = 0;
		} else {
			col.red = 255 << 8;
			col.green = (255 - (i << 1)) << 8;
			col.blue = 0;
		}	
		gdk_color_alloc(gdk_colormap_get_system(), &col);
		gdk_gc_set_foreground(gc,&col);
		gdk_draw_line(disp,gc,i,0,i,18);
		gdk_draw_line(disp,gc,i+1,0,i+1,18);
		gdk_draw_line(disp,gc,i+2,0,i+2,18);
		gdk_gc_set_foreground(gc,&color);
		gdk_draw_line(disp,gc,i+3,0,i+3,18);
	}

	gdk_color_black(gdk_colormap_get_system(),&col);

	gdk_gc_set_foreground(gc,&col);
	

	area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(levelmeter_win), area);
	gtk_widget_realize(area);
	gdk_window_set_background(area->window, &color);

	gdk_window_clear(area->window);
	gtk_widget_show(area);
	
	// Signals

	gtk_signal_connect(GTK_OBJECT(levelmeter_win), "delete_event",
		GTK_SIGNAL_FUNC(close_levelmeter_window), levelmeter_win);
	gtk_signal_connect(GTK_OBJECT(levelmeter_win), "button_press_event",
		GTK_SIGNAL_FUNC(popup), levelmeter_win);

	// Create sin/cos tables
	
	for (i = 0; i < 256; i++) {
                scX[i] = (char) (sin(((2*M_PI)/255)*i)*128);
                scY[i] = (char) (-cos(((2*M_PI)/255)*i)*128);
        } 

	return levelmeter_win;
}  



void levelmeter_hide(void)
{
	gint x, y;	
	if (scope_win) {
		gdk_window_get_root_origin(scope_win->window, &x, &y);	
		gtk_widget_hide(scope_win);
		gtk_widget_set_uposition(scope_win, x, y);
	}
}


static void stop_levelmeter(void)
{
	if (running) {
		running = 0;
		pthread_join(levelmeter_thread, NULL);
	}	
}


static void run_levelmeter(void *data)
{
	nice(SCOPE_NICE);	
	the_levelmeter(scope_win);
	pthread_mutex_unlock(&levelmeter_mutex);
	pthread_exit(NULL);
}



static void start_levelmeter(void)
{
    if (!is_init) {
		scope_win = init_levelmeter_window();
		if (!scope_win)
			return;
		is_init = 1;
	}
	if (pthread_mutex_trylock(&levelmeter_mutex) != 0) {
			printf("levelmeter already running\n");
			return;
	}	
	
	gtk_widget_show(scope_win);

	pthread_create(&levelmeter_thread, NULL,
		(void * (*)(void *))run_levelmeter, NULL);
}

static int init_levelmeter(void *arg)
{
	if (prefs_get_bool(ap_prefs, "levelmeter", "active", 0))
		start_levelmeter();
	return 1;
}


static void shutdown_levelmeter(void)
{
	prefs_set_bool(ap_prefs, "levelmeter", "active", levelmeter_running());
	
	stop_levelmeter();
	if (disp) {
					gdk_pixmap_unref(disp);
	}
	if (draw_pixmap) {
					gdk_pixmap_unref(draw_pixmap);
	}	
	if (area) {
					gtk_widget_destroy(area);
					area = NULL;
	}
	if (gc) {
					gdk_gc_destroy(gc);
					gc = NULL;
	}	
	if (scope_win) {
					gtk_widget_destroy(scope_win);
					scope_win = NULL;
	}					
}


static int levelmeter_running()
{
	return running;
}

scope_plugin levelmeter_plugin = {
	SCOPE_PLUGIN_VERSION,
	"Levelmeter",
	"Andy Lo A Foe",
	NULL,
	init_levelmeter,
	start_levelmeter,
	levelmeter_running,
	stop_levelmeter,
	shutdown_levelmeter,
	levelmeter_set_data,
	NULL
};


scope_plugin *scope_plugin_info(void)
{
	return &levelmeter_plugin;
}

