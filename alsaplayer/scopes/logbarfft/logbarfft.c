/*  barfft.cpp
 *  Copyright (C) 1999 Andy Lo A Foe <andy@alsa-project.org>
 *  Based on code by Richard Boulton <richard@tartarus.org>
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
#include "fft.h"

#define BARS 32

static GtkWidget *area = NULL;
static GdkRgbCmap *color_map = NULL;
static sound_sample actEq[FFT_BUFFER_SIZE];
static sound_sample oldEq[FFT_BUFFER_SIZE];
static int maxbar[BARS];
static double fftout[FFT_BUFFER_SIZE / 2 + 1];
static double fftmult;
static fft_state *fftstate;
static GdkImage *image = NULL;
static GtkWidget *scope_win = NULL;
static int ready_state = 0;
static pthread_t fftscope_thread;
static pthread_mutex_t fftscope_mutex;
static int is_init = 0;
static int running = 0;

//static int xranges[] = {1, 2, 3, 4, 6, 8, 11, 15, 21,
//			29, 40, 54, 74, 101, 137, 187, 255};
static int xranges[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 13, 15, 17, 19, 21, 23, 25, 29, 33, 37, 40, 44, 48, 54, 61, 67, 74, 90, 101, 137, 158, 187, 255};



static void fftscope_hide();

static const int default_colors[] = {
    10, 20, 30,
    230, 230, 230
};


void logscope_set_data(void *audio_buffer, int size)
{
	int i;
	short *sound = (short *)audio_buffer;
	
	if (!sound) {
			memset(&actEq, 0, sizeof(actEq));
			return;
	}
	if (running && size > FFT_BUFFER_SIZE * 2) {
		sound_sample *newset = actEq;
		sound += (size / 2 - FFT_BUFFER_SIZE) * 2; // Use the very latest data
		for (i=0; i < FFT_BUFFER_SIZE; i++) {
			*newset++=(sound_sample)((
			          (int)(*sound) + (int)*(sound + 1)
				  ) >> 1);
			sound += 2;
		}
	}
}

static void the_fftscope()
{
	guchar *loc;
	guchar bits [256 * 129];
	int i, h;
	
	running = 1;

	while (running) {
    guint val;
    gint j;
    gint k;
		gint w;
   
		memset(bits, 0, 256 * 128);
    
		memcpy(&oldEq, &actEq, sizeof(actEq));

    fft_perform(oldEq, fftout, fftstate);
    for (i=0; i < BARS; i++) { 
				val = 0;
				for (j = xranges[i]; j < xranges[i + 1]; j++) {
					k = (guint)(sqrt(fftout[j]) * fftmult);
					val += k;
				}
				if(val > 127) val = 127;
				if (val > maxbar[ i ]) 
					maxbar[ i ] = val;
				else {
					k = maxbar[ i ] - 6;
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
	GdkColor color;
	guint32 colors[129];
	int i;
	
	pthread_mutex_init(&fftscope_mutex, NULL);

	fftscope_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(fftscope_win), "logFFTscope");
	gtk_widget_set_usize(fftscope_win, 256,128);
	gtk_window_set_wmclass (GTK_WINDOW(fftscope_win), "logFFTscope", "AlsaPlayer");
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
	
	// Signals
		
	gtk_signal_connect(GTK_OBJECT(fftscope_win), "delete_event",
                GTK_SIGNAL_FUNC(close_fftscope_window), fftscope_win);


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

	the_fftscope();

	pthread_mutex_unlock(&fftscope_mutex);
	pthread_exit(NULL);
}


static void start_fftscope(void *data)
{
	fftstate = fft_init();
	if (!fftstate)
		return;
	if (!is_init) {
		is_init = 1;
		scope_win = init_fftscope_window();
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

        fft_init();
	
	fftmult = (double)128 / ((FFT_BUFFER_SIZE * 16384) ^ 2);
		// Result now guaranteed (well, almost) to be in range 0..128

		// Low values represent more frequencies, and thus get more
		// intensity - this helps correct for that.

	fftmult *= 3; // Adhoc parameter, looks about right for me.

	for (i = 0; i < BARS; i++) {
		maxbar[ i ] = 0;
	}	
	return 1;
}

static void close_fftscope()
{
}

static int fftscope_running()
{
	return running;
}

scope_plugin logscope_plugin = {
	SCOPE_PLUGIN_VERSION,
	{ "logFFTscope" },
	{ "Andy Lo A Foe"},
	NULL,
	init_fftscope,
	open_fftscope,
	start_fftscope,
	fftscope_running,
	stop_fftscope,
	close_fftscope,
	logscope_set_data
};


scope_plugin *scope_plugin_info()
{
	return &logscope_plugin;
}

