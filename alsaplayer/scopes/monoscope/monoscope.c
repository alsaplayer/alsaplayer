/*  monoscope.h
 *  Copyright (C) 1998 Andy Lo A Foe <arloafoe@cs.vu.nl>
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
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include "scope_config.h"

#include "convolve.h"

short newEq[CONVOLVE_BIG];	// latest block of 512 samples.
int avgEq[CONVOLVE_SMALL];	// a running average of the last few.
int avgMax;			// running average of max sample.

static GdkImage *image = NULL;
static GtkWidget *scope_win = NULL;
static int ready_state = 0;
static pthread_t monoscope_thread;
static pthread_mutex_t monoscope_mutex;
static int is_init = 0;
static int running = 0;
static convolve_state *state;

static void monoscope_hide();

static const int default_colors[] = {
    10, 20, 30,
    230, 230, 230
};

void monoscope_set_data(void *audio_buffer, int size)
{
	int i;	
	short *sound = (short *)audio_buffer;
	if (!sound) {
			memset(&newEq, 0, sizeof(newEq));
			return;
	}	
	if (running && size > CONVOLVE_BIG * 2) {
		short * newset = newEq;
		int skip = (size / (CONVOLVE_BIG * 2)) * 2;
		for (i = 0; i < CONVOLVE_BIG; i++) {
			*newset++ = (((int) sound[0]) + (int) sound[1]) >> 1;
			sound += skip;
		}
	}
}

static	short copyEq[512];

#define MONOSCOPE_DOLOOP() \
while (running) { \
	int w, factor;\
	int max = 1;\
	short * thisEq; \
	memcpy (copyEq, newEq, sizeof (short) * CONVOLVE_BIG); \
	thisEq = copyEq + convolve_match (avgEq, copyEq, state); \
	for (w=0; w < 256 * 128; w++) { \
		bits[w] = bg_color.pixel; \
	} \
\
	for (i=0; i < 256; i++) { \
		foo = thisEq[i] + (avgEq[i] >> 1); \
		avgEq[i] = foo; \
		if (foo < 0) \
			foo = -foo; \
		if (foo > max) \
			max = foo; \
	} \
        avgMax += max - (avgMax >> 8); \
	if (avgMax < max) \
		avgMax = max; /* Avoid overflow */ \
    factor = 0x7fffffff / avgMax; \
	/* Keep the scaling sensible. */ \
	if (factor > (1 << 18)) \
		factor = 1 << 18; \
	if (factor < (1 << 8)) \
		factor = 1 << 8; \
	for (i=0; i < 256; i++) { \
		foo = avgEq[i] * factor; \
                foo >>= 18; \
		if (foo > 63) \
			foo = 63; \
		if (foo < -64) \
			foo = -64; \
		bar = (i + ((foo+64) << 8)); \
		if ((bar > 0) && (bar < (256 * 128))) { \
			loc = bits + bar; \
			if (foo < 0) { \
				for (h = 0; h <= (-foo); h++) { \
					*loc = colEq[h]; \
					loc+=256; \
				} \
			} else { \
				for (h = 0; h <= foo; h++) { \
					*loc = colEq[h]; \
					loc-=256; \
				} \
			} \
		} \
	} \
	for (i=16;i < 128; i+=16) { \
		for (h = 0; h < 256; h+=2) { \
			bits[(i << 8) + h] = raster_color.pixel; \
		} \
	} \
	for (i = 16; i < 256; i+=16) { \
		for (h = 0; h < 128; h+=2) { \
			bits[i + (h << 8)] = raster_color.pixel; \
		} \
	} \
	GDK_THREADS_ENTER(); \
	gdk_draw_image(win,gc,image,0,0,0,0,-1,-1); \
	gdk_flush(); \
	GDK_THREADS_LEAVE(); \
	dosleep(SCOPE_SLEEP); \
}

void monoscope32(void *data)
{
	guint32 *loc;
	gint foo;
	guint32 *bits;
	int bar;  
	guint32 colEq[65];
	int i, h;
	GdkWindow *win;
	GdkColormap *c;
	GdkVisual *v;
	GdkGC *gc;
	GdkColor raster_color;
	GdkColor bg_color;
	
	win = (GdkWindow *)data;
	GDK_THREADS_ENTER();
	c = gdk_colormap_get_system();
	gc = gdk_gc_new(win);
	v = gdk_window_get_visual(win);

	for (i = 0; i < 32; i++) {
		GdkColor color;
		color.red = (i*8) << 8;
		color.green = 255 << 8;
		color.blue = 0;
	        gdk_color_alloc(c, &color);
		colEq[i] = color.pixel; 
		color.red = 255 << 8;
		color.green = ((31 - i) * 8) << 8;
		color.blue = 0;
		gdk_color_alloc(c, &color);
		colEq[i + 32] = color.pixel;
  	}
	raster_color.red = 40 << 8;
	raster_color.green = 75 << 8;
	raster_color.blue = 0;
	gdk_color_alloc(c, &raster_color);

	// Create render image
	if (image) {
		gdk_image_destroy(image);
		image = NULL;
	}
	image = gdk_image_new(GDK_IMAGE_FASTEST, v, 256, 128);
	bg_color.red = SCOPE_BG_RED << 8;
	bg_color.green = SCOPE_BG_GREEN << 8;
	bg_color.blue = SCOPE_BG_BLUE << 8;
        gdk_color_alloc(c, &bg_color); 
	GDK_THREADS_LEAVE();
	
	assert(image);
	assert(image->bpp > 2);
	
	bits = (guint32 *)image->mem;	

	running = 1;

	MONOSCOPE_DOLOOP();
	
	GDK_THREADS_ENTER();
	monoscope_hide();
	GDK_THREADS_LEAVE();
}



void monoscope16(void *data)
{
	guint16 *loc;
	gint foo;
	guint16 *bits;
	int bar;  
	guint16 colEq[65];
	int i, h;
	GdkWindow *win;
	GdkColormap *c;
	GdkVisual *v;
	GdkGC *gc;
	GdkColor raster_color;
	GdkColor bg_color;
	
	win = (GdkWindow *)data;
	GDK_THREADS_ENTER();
	c = gdk_colormap_get_system();
	gc = gdk_gc_new(win);
	v = gdk_window_get_visual(win);

	for (i = 0; i < 32; i++) {
		GdkColor color;
		color.red = (i*8) << 8;
		color.green = 255 << 8;
		color.blue = 0;
	        gdk_color_alloc(c, &color);
		colEq[i] = color.pixel; 
		color.red = 255 << 8;
		color.green = ((31 - i) * 8) << 8;
		color.blue = 0;
		gdk_color_alloc(c, &color);
		colEq[i + 32] = color.pixel;
  	}
	raster_color.red = 40 << 8;
	raster_color.green = 75 << 8;
	raster_color.blue = 0;
	gdk_color_alloc(c, &raster_color);

	// Create render image
	if (image) {
		gdk_image_destroy(image);	
		image = NULL;
	}
	image = gdk_image_new(GDK_IMAGE_FASTEST, v, 256, 128);
	bg_color.red = SCOPE_BG_RED << 8;
	bg_color.green = SCOPE_BG_GREEN << 8;
	bg_color.blue = SCOPE_BG_BLUE << 8;        
	gdk_color_alloc(c, &bg_color); 
	GDK_THREADS_LEAVE();
	
	assert(image);
	assert(image->bpp == 2);
	
	bits = (guint16 *)image->mem;	
	
	running = 1;

	MONOSCOPE_DOLOOP();

	GDK_THREADS_ENTER();
	monoscope_hide();
	GDK_THREADS_LEAVE();
}


void monoscope8(void *data)
{
	guint8 *loc;
	gint8 foo;
	guint8 colEq[65];
	guint8 *bits;
	int bar;  
	int i, h;
	GdkWindow *win;
	GdkColormap *c;
	GdkVisual *v;
	GdkGC *gc;
	GdkColor raster_color;
	GdkColor bg_color;
	
	win = (GdkWindow *)data;
	GDK_THREADS_ENTER();
	c = gdk_colormap_get_system();
	gc = gdk_gc_new(win);
	v = gdk_window_get_visual(win);

	for (i = 0; i < 32; i++) {
		GdkColor color;
		color.red = (i*8) << 8;
		color.green = 255 << 8;
		color.blue = 0;
	        gdk_color_alloc(c, &color);
		colEq[i] = color.pixel; 
		color.red = 255 << 8;
		color.green = ((31 - i) * 8) << 8;
		color.blue = 0;
		gdk_color_alloc(c, &color);
		colEq[i + 32] = color.pixel;
  	}
	
	raster_color.red = 40 << 8;
	raster_color.green = 75 << 8;
	raster_color.blue = 0;
	gdk_color_alloc(c, &raster_color);
	
	// Create render image
	if (image) {
		gdk_image_destroy(image);
		image = NULL;
	}	
	image = gdk_image_new(GDK_IMAGE_FASTEST, v, 256, 128);
	bg_color.red = SCOPE_BG_RED << 8;
	bg_color.green = SCOPE_BG_GREEN << 8;
	bg_color.blue = SCOPE_BG_BLUE << 8;        
	gdk_color_alloc(c, &bg_color); 
	GDK_THREADS_LEAVE();
	
	assert(image);
	assert(image->bpp == 1);
	
	bits = (guint8 *)image->mem;	

	running = 1;

	MONOSCOPE_DOLOOP();

	GDK_THREADS_ENTER();
	monoscope_hide();
	GDK_THREADS_LEAVE();
}

static GdkVisual *visual;
static GdkWindow *win;

static void test_cb(GtkWidget *widget, gpointer data)
{
	printf("Woah!\n");
}


void stop_monoscope();

static gboolean close_monoscope_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
        stop_monoscope();

		return TRUE;
}


GtkWidget *init_monoscope_window()
{
	GtkWidget *monoscope_win;
	GtkStyle *style;
	GdkColor *color;
	
	pthread_mutex_init(&monoscope_mutex, NULL);

	style = gtk_style_new();
	monoscope_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(monoscope_win), "AlsaPlayer: Monoscope");
	gtk_widget_set_usize(monoscope_win, 256,128);
	gtk_window_set_wmclass (GTK_WINDOW(monoscope_win), "Monoscope", "AlsaPlayer");
	gtk_window_set_policy (GTK_WINDOW (monoscope_win), FALSE, FALSE, FALSE);
	style = gtk_style_copy(gtk_widget_get_style(GTK_WIDGET(monoscope_win)));
	
	color = &style->bg[GTK_STATE_NORMAL];
	color->red = SCOPE_BG_RED << 8;
	color->blue = SCOPE_BG_BLUE << 8;
	color->green = SCOPE_BG_GREEN << 8;
	gdk_color_alloc(gdk_colormap_get_system(), color);
	gtk_widget_set_style(GTK_WIDGET(monoscope_win), style);
	
	gtk_widget_show(monoscope_win);
	
	win = monoscope_win->window;

	// Signals
		
	gtk_signal_connect(GTK_OBJECT(monoscope_win), "delete_event",
                GTK_SIGNAL_FUNC(close_monoscope_window), monoscope_win);


	// Clear and show the window
	gdk_window_clear(win);
	gdk_window_show(win);
	gdk_flush(); 

	ready_state = 1;

	return monoscope_win;
}


void monoscope_hide()
{
	gint x, y;
	
	if (scope_win) {
		gdk_window_get_root_origin(scope_win->window, &x, &y);
		gtk_widget_hide(scope_win);
		gtk_widget_set_uposition(scope_win, x, y);
	}
}


void stop_monoscope()
{
	running = 0;
}


void run_monoscope(void *data)
{
	nice(SCOPE_NICE); // Be nice to most processes

	GDK_THREADS_ENTER();	
	visual = gdk_window_get_visual(win); 
	GDK_THREADS_LEAVE();

	switch (visual->depth) {
	 case 8:
		monoscope8(win);
		break;
	 case 16:
		monoscope16(win);
		break;
	 case 24:
	 case 32:
		monoscope32(win);
		break;
		
	}
	//delete sub;
	pthread_mutex_unlock(&monoscope_mutex);
}


void start_monoscope(void *data)
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
	pthread_create(&monoscope_thread, NULL, (void * (*)(void *))run_monoscope, data);
	pthread_detach(monoscope_thread);
}


static int open_monoscope()
{
	return 1;
}

static int init_monoscope()
{
	state = convolve_init();
	if(!state) return 0;

	/* FIXME - Need to call convolve_close(state); at some point
	 * if this is going to become a proper plugin. */

	return 1;
}

static void close_monoscope()
{
}

static int monoscope_running()
{
	return running;
}

scope_plugin monoscope_plugin = {
	SCOPE_PLUGIN_VERSION,
	{ "Monoscope" },
	{ "Andy Lo A Foe"},
	init_monoscope,
	open_monoscope,
	start_monoscope,
	monoscope_running,
	stop_monoscope,
	close_monoscope,
	monoscope_set_data
};


scope_plugin *scope_plugin_info()
{
	return &monoscope_plugin;
}	
