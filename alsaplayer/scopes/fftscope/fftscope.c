/*  fftscope.cpp
 *  Copyright (C) 1999 Richard Boulton <richard@tartarus.org>
 *  Based on code by Andy Lo A Foe <arloafoe@cs.vu.nl>
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

static int act_fft[512];
static GdkImage *image = NULL;
static GtkWidget *scope_win = NULL;
static int ready_state = 0;
static pthread_t fftscope_thread;
static pthread_mutex_t fftscope_mutex;
static int is_init = 0;
static int running = 0;

static void fftscope_hide();

static const int default_colors[] = {
    10, 20, 30,
    230, 230, 230
};


void fftscope_set_fft(void *fft_buffer, int samples, int channels)
{
	if (!fft_buffer || (samples * channels) > 512) {
			memset(act_fft, 0, sizeof(act_fft));
			return;
	}	
	memcpy(act_fft, fft_buffer, (sizeof(int)) * samples * channels);
}

#define FFTSCOPE_DOLOOP() \
while (running) { \
	int w;\
    guint val;\
\
    for (w=0; w < 256 * 128; w++) { \
	bits[w] = bg_color.pixel; \
    } \
\
    for (i=0; i < 256; i++) { \
	val = (act_fft[i] + act_fft[i+256]) / 2; \
	if(val > 127) val = 127; \
	loc = bits + i + 256 * 127; \
	for (h = val; h > 0; h--) { \
	    *loc = colEq[h]; \
	    loc-=256; \
	} \
    } \
    GDK_THREADS_ENTER(); \
    gdk_draw_image(win,gc,image,0,0,0,0,-1,-1); \
    gdk_flush(); \
    GDK_THREADS_LEAVE(); \
    dosleep(SCOPE_SLEEP); \
}

static void fftscope32(void *data)
{
	guint32 *loc;
	guint32 *bits;
	guint32 colEq[128];
	int i, h;
	GdkWindow *win;
	GdkColormap *c;
	GdkVisual *v;
	GdkGC *gc;
	GdkColor bg_color;
	
	win = (GdkWindow *)data;
	GDK_THREADS_ENTER();
	c = gdk_colormap_get_system();
	gc = gdk_gc_new(win);
	v = gdk_window_get_visual(win);

	for (i = 0; i < 64; i++) {
		GdkColor color;
		color.red = (i*4) << 8;
		color.green = 255 << 8;
		color.blue = 0;
	        gdk_color_alloc(c, &color);
		colEq[i] = color.pixel; 
		color.red = 255 << 8;
		color.green = ((63 - i) * 4) << 8;
		color.blue = 0;
		gdk_color_alloc(c, &color);
		colEq[i + 64] = color.pixel;
  	}

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

	FFTSCOPE_DOLOOP();

	GDK_THREADS_ENTER();
	fftscope_hide();
	GDK_THREADS_LEAVE();
}



static void fftscope16(void *data)
{
	guint16 *loc;
	guint16 *bits;
	guint16 colEq[128];
	int i, h;
	GdkWindow *win;
	GdkColormap *c;
	GdkVisual *v;
	GdkGC *gc;
	GdkColor bg_color;
	
	win = (GdkWindow *)data;
	GDK_THREADS_ENTER();
	c = gdk_colormap_get_system();
	gc = gdk_gc_new(win);
	v = gdk_window_get_visual(win);

	for (i = 0; i < 64; i++) {
		GdkColor color;
		color.red = (i*4) << 8;
		color.green = 255 << 8;
		color.blue = 0;
	        gdk_color_alloc(c, &color);
		colEq[i] = color.pixel; 
		color.red = 255 << 8;
		color.green = ((63 - i) * 4) << 8;
		color.blue = 0;
		gdk_color_alloc(c, &color);
		colEq[i + 64] = color.pixel;
  	}

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

	FFTSCOPE_DOLOOP();

	GDK_THREADS_ENTER();
	fftscope_hide();
	GDK_THREADS_LEAVE();
}


static void fftscope8(void *data)
{
	guint8 *loc;
	guint8 colEq[128];
	guint8 *bits;
	int i, h;
	GdkWindow *win;
	GdkColormap *c;
	GdkVisual *v;
	GdkGC *gc;
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
		colEq[i * 2] = color.pixel; 
		colEq[i * 2 + 1] = color.pixel; 
		color.red = 255 << 8;
		color.green = ((31 - i) * 8) << 8;
		color.blue = 0;
		gdk_color_alloc(c, &color);
		colEq[i * 2 + 64] = color.pixel;
		colEq[i * 2 + 65] = color.pixel;
  	}

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

	FFTSCOPE_DOLOOP();

	GDK_THREADS_ENTER();
	fftscope_hide();
	GDK_THREADS_LEAVE();
}

static GdkVisual *visual;
static GdkWindow *win;

static void test_cb(GtkWidget *widget, gpointer data)
{
	printf("Woah!\n");
}


static void stop_fftscope();

static gboolean close_fftscope_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GDK_THREADS_LEAVE();
	stop_fftscope();
	GDK_THREADS_ENTER();

	return TRUE;
}


static GtkWidget *init_fftscope_window()
{
	GtkWidget *fftscope_win;
	GtkStyle *style;
	GdkColor *color;
	
	pthread_mutex_init(&fftscope_mutex, NULL);

	style = gtk_style_new();
	fftscope_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(fftscope_win), "FFTscope");
	gtk_widget_set_usize(fftscope_win, 256,128);
	gtk_window_set_wmclass (GTK_WINDOW(fftscope_win), "FFTscope", "AlsaPlayer");
	gtk_window_set_policy (GTK_WINDOW (fftscope_win), FALSE, FALSE, FALSE);
	style = gtk_style_copy(gtk_widget_get_style(GTK_WIDGET(fftscope_win)));
	
	color = &style->bg[GTK_STATE_NORMAL];
	color->red = SCOPE_BG_RED << 8;
	color->blue = SCOPE_BG_BLUE << 8;
	color->green = SCOPE_BG_GREEN << 8;
	gdk_color_alloc(gdk_colormap_get_system(), color);
	gtk_widget_set_style(GTK_WIDGET(fftscope_win), style);
	
	gtk_widget_show(fftscope_win);
	
	win = fftscope_win->window;

	// Signals
		
	gtk_signal_connect(GTK_OBJECT(fftscope_win), "delete_event",
                GTK_SIGNAL_FUNC(close_fftscope_window), fftscope_win);


	// Clear and show the window
	gdk_window_clear(win);
	gdk_window_show(win);
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


static void stop_fftscope()
{
	running = 0;
	pthread_join(fftscope_thread, NULL);
}


static void run_fftscope(void *data)
{
	nice(SCOPE_NICE); // Be nice to most processes

	GDK_THREADS_ENTER();	
	visual = gdk_window_get_visual(win); 
	GDK_THREADS_LEAVE();

	switch (visual->depth) {
	 case 8:
		fftscope8(win);
		break;
	 case 16:
		fftscope16(win);
		break;
	 case 24:
	 case 32:
		fftscope32(win);
		break;
		
	}
	pthread_mutex_unlock(&fftscope_mutex);
	pthread_exit(NULL);
}


static void start_fftscope(void *data)
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
	pthread_create(&fftscope_thread, NULL, (void * (*)(void *))run_fftscope, data);
}


static int open_fftscope()
{
	return 1;
}

static int init_fftscope()
{
	int i;

	memset(act_fft, 0, sizeof(act_fft));

	return 1;
}

static void close_fftscope()
{
}

static int fftscope_running()
{
	return running;
}

scope_plugin fftscope_plugin = {
	SCOPE_PLUGIN_VERSION,
	{ "FFTscope" },
	{ "Richard Boulton"},
	NULL,
	init_fftscope,
	open_fftscope,
	start_fftscope,
	fftscope_running,
	stop_fftscope,
	close_fftscope,
	NULL,
	fftscope_set_fft
};


scope_plugin *scope_plugin_info()
{
	return &fftscope_plugin;
}


