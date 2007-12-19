/*  blurscope.c
 *  Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
 *  Ported from XMMS:
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
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
 *  $Id: blurscope.c 1017 2003-11-09 13:28:30Z adnans $
*/ 
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
#include "scope_plugin.h"
#include "utilities.h"
#include "prefs.h"

static GtkWidget *window = NULL,*area;
static GdkPixmap *bg_pixmap = NULL;
static pthread_t bscope_thread;
static pthread_mutex_t bscope_mutex;
static pthread_mutex_t edit_mutex;
static gint running = 0;
static gint16 audio_data[2][256];

#define WIDTH 256 
#define HEIGHT 128
#define min(x,y) ((x)<(y)?(x):(y))
#define BPL	((WIDTH + 2))

static guchar rgb_buf[(WIDTH + 2) * (HEIGHT + 2)];
static GdkRgbCmap *cmap = NULL; 

static int bscope_running(void);

static inline void draw_pixel_8(guchar *buffer,gint x, gint y, guchar c)
{
	buffer[((y + 1) * BPL) + (x + 1)] = c;
}


#ifndef I386_ASSEM
void bscope_blur_8(guchar *ptr,gint w, gint h, gint bpl)
{
	register guint i,sum;
	register guchar *iptr;
	
	iptr = ptr + bpl + 1;
	i = bpl * h;
	while(i--)
	{
		sum = (iptr[-bpl] + iptr[-1] + iptr[1] + iptr[bpl]) >> 2;
		if(sum > 2)
			sum -= 2;
		*(iptr++) = sum;
	}
	
	
}
#else
extern void bscope_blur_8(guchar *ptr,gint w, gint h, gint bpl);
#endif

void generate_cmap(void)
{
	guint32 colors[256],i,red,blue,green;
	if(window)
	{
		red = (guint32)(0xFF3F7F / 0x10000);
		green = (guint32)((0xFF3F7F % 0x10000)/0x100);
		blue = (guint32)(0xFF3F7F % 0x100);
		for(i = 255; i > 0; i--)
		{
			colors[i] = (((guint32)(i*red/256) << 16) | ((guint32)(i*green/256) << 8) | ((guint32)(i*blue/256)));
		}
		colors[0]=0;
		if(cmap)
		{
			gdk_rgb_cmap_free(cmap);
		}
		cmap = gdk_rgb_cmap_new(colors,256);
	}
}

static void stop_bscope(void)
{
	if (running) {
		running = 0;
		pthread_join(bscope_thread, NULL);
	}	
}


static gboolean close_bscope_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GDK_THREADS_LEAVE();            
	stop_bscope();
	GDK_THREADS_ENTER();
	return TRUE;
}


static void bscope_init(void)
{
	GdkColor color;
	
	if(window)
		return;

	pthread_mutex_init(&bscope_mutex, NULL);
	pthread_mutex_init(&edit_mutex, NULL);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window),"Blurscope");
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
	gtk_widget_realize(window);
	color.red = SCOPE_BG_RED << 8;
	color.blue = SCOPE_BG_BLUE << 8;
	color.green = SCOPE_BG_GREEN << 8;
	gdk_color_alloc(gdk_colormap_get_system(), &color);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			GTK_SIGNAL_FUNC(close_bscope_window), window);	
	gtk_widget_set_usize(window, WIDTH, HEIGHT);
	
	area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window),area);
	gtk_widget_realize(area);
	gdk_window_set_background(area->window, &color);
	generate_cmap();
	memset(rgb_buf,0,(WIDTH + 2) * (HEIGHT + 2));
		
	gtk_widget_show(area);
	gdk_window_clear(window->window);
	gdk_window_clear(area->window);
}

static void shutdown_bscope(void)
{
	prefs_set_bool(ap_prefs, "blurscope", "active", bscope_running());

	stop_bscope();

	if(window)
		gtk_widget_destroy(window);
	if(bg_pixmap)
	{
		gdk_pixmap_unref(bg_pixmap);
		bg_pixmap = NULL;
	}
	if(cmap)
	{
		gdk_rgb_cmap_free(cmap);
		cmap = NULL;
	}
}

static inline void draw_vert_line(guchar *buffer, gint x, gint y1, gint y2)
{
	int y;
	if(y1 < y2)
	{
		for(y = y1; y <= y2; y++)
			draw_pixel_8(buffer,x,y,0xFF);
	}
	else if(y2 < y1)
	{
		for(y = y2; y <= y1; y++)
			draw_pixel_8(buffer,x,y,0xFF);
	}
	else
		draw_pixel_8(buffer,x,y1,0xFF);
}


static void bscope_hide();

static void bscope_set_data(void *audio_buffer, int size)
{
#ifndef FREQ_BLUR 	
	short i;
#endif	
	short *sound = (short *)audio_buffer;
#ifdef FREQ_BLUR	
	static gint i,y, prev_y;
#endif
	if (pthread_mutex_trylock(&edit_mutex) != 0) {
			return;
	}		
	if (running && sound && size >= 1024) {
     	for(i=0;i<256;i++) {
           	audio_data[0][i] = *(sound++);
            audio_data[1][i] = *(sound++);
     	}
	}
#ifdef FREQ_BLUR	
		bscope_blur_8(rgb_buf, WIDTH, HEIGHT, BPL);

		prev_y = y = (HEIGHT / 2) + (audio_data[0][0] >> 9) + (audio_data[1][0] >> 9) / 2;
		if (prev_y < 0)
			y = prev_y = 0;
		if (y >= HEIGHT)
			y = prev_y = HEIGHT - 1;

		for(i = 0; i < WIDTH; i++)
		{
			y = (HEIGHT / 2) + (audio_data[0][i >> 1] >> 9) + 
												 (audio_data[1][i >> 1] >> 9) / 2; /* Take half of other */
			if(y < 0)
				y = 0;
			if(y >= HEIGHT)
				y = HEIGHT - 1;
			draw_vert_line(rgb_buf,i,prev_y,y);
			prev_y = y;
		}
#endif
	pthread_mutex_unlock(&edit_mutex);
}


static void the_bscope(void)
{
	running = 1;
	
	while (running) {
		gint i,y, prev_y;
#ifndef FREQ_BLUR
		pthread_mutex_lock(&edit_mutex);
		
		bscope_blur_8(rgb_buf, WIDTH, HEIGHT, BPL);

		prev_y = y = (HEIGHT / 2) + (audio_data[0][0] >> 9) + (audio_data[1][0] >> 9) / 2;
		if (prev_y < 0)
			y = prev_y = 0;
		if (y >= HEIGHT)
			y = prev_y = HEIGHT - 1;

		for(i = 0; i < WIDTH; i++)
		{
			y = (HEIGHT / 2) + (audio_data[0][i >> 1] >> 9) + 
												 (audio_data[1][i >> 1] >> 9) / 2; /* Take half of other */
			if(y < 0)
				y = 0;
			if(y >= HEIGHT)
				y = HEIGHT - 1;
			draw_vert_line(rgb_buf,i,prev_y,y);
			prev_y = y;
		}
		pthread_mutex_unlock(&edit_mutex);
#endif	
		
		GDK_THREADS_ENTER();
		gdk_draw_indexed_image(area->window,area->style->white_gc,0,0,WIDTH,HEIGHT,GDK_RGB_DITHER_NONE,rgb_buf + BPL + 1,(WIDTH + 2),cmap);
		gdk_flush();
		GDK_THREADS_LEAVE();
	
		dosleep(SCOPE_SLEEP);
	}
	GDK_THREADS_ENTER();
	bscope_hide();
	gdk_flush();
	GDK_THREADS_LEAVE();
}


void bscope_hide(void)
{
	gint x, y;	
	if (window) {
		gdk_window_get_root_origin(window->window, &x, &y);	
		gtk_widget_hide(window);
		gtk_widget_set_uposition(window, x, y);
	}
}


static void run_bscope(void *data)
{
	nice(SCOPE_NICE);	
	the_bscope();	
	pthread_mutex_unlock(&bscope_mutex);
	pthread_exit(NULL);
}



static void start_bscope(void)
{
	if (pthread_mutex_trylock(&bscope_mutex) != 0) {
			printf("blurscope already running\n");
			return;
	}

	gtk_widget_show(window);
	pthread_create(&bscope_thread, NULL,
		(void * (*)(void *))run_bscope, NULL);
}


static int init_bscope(void *arg)
{
	bscope_init();

	if (prefs_get_bool(ap_prefs, "blurscope", "active", 0)) {
		start_bscope();
	}	
	return 1;
}


static int bscope_running()
{
	return running;
}


static scope_plugin bscope_plugin;

scope_plugin *scope_plugin_info(void)
{
	memset(&bscope_plugin, 0, sizeof(scope_plugin));
	bscope_plugin.version = SCOPE_PLUGIN_VERSION;
	bscope_plugin.name = "Blurscope";
	bscope_plugin.author = "Andy Lo A Foe";
	bscope_plugin.init = init_bscope;
	bscope_plugin.start = start_bscope;
	bscope_plugin.running = bscope_running;
	bscope_plugin.stop = stop_bscope;
	bscope_plugin.shutdown = shutdown_bscope;
	bscope_plugin.set_data = bscope_set_data;
	return &bscope_plugin;
}

