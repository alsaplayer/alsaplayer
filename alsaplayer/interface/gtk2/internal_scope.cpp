#include <stdio.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include "alsaplayer_error.h"
#include "internal_scope.h"
#include "scope_plugin.h"
#include "utilities.h"
#include "scopes.h"

#define BARS 20

static int running = 0;
static GdkRgbCmap *color_map = NULL;
static GtkDrawingArea *area = NULL;
static int fft_buf[512];
static int maxbar[BARS];
static pthread_t scope_thread;
static pthread_mutex_t scope_mutex;

static int xranges[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 15, 20, 27, 36, 47, 62, 82, 107, 141, 184, 255};

int init_internal_scope(void *arg)
{
	GdkColor color;
	guint32 colors[129];
	int i;
	
	area = (GtkDrawingArea *)arg;

	if (!area)
		return 0;
	pthread_mutex_init(&scope_mutex, NULL);

	/* Allocate all colors */
	color.red = 222 << 8;
	color.blue = 222 << 8;
	color.green = 222 << 8;
	gdk_color_alloc(gdk_colormap_get_system(), &color);
	colors[0] = 23;
	for (i = 1; i < 64; i++) {
		colors[i] = ((i*4) << 16) + (255 << 8) + 128;
		colors[i + 63] = (255 << 16) + (((63 - i) * 4) << 8) + 130;
	}
	color_map = gdk_rgb_cmap_new(colors, 128);
	gdk_window_set_background(area->widget.window, &color);
	
	return 1;
}

int internal_scope_running(void)
{
	return running;
}


void scope_run(void *arg)
{
	guchar *loc;
	guchar bits [MINI_W * MINI_H];
	int val, i, j, k, h;
	
	nice(SCOPE_NICE);

	while (running) {
		const double y_scale = 3.60673760222; /* 20.0 / log(256) */
		
		memset(bits, 0, MINI_W * MINI_H);
		for (i=0; i < BARS; i++) {
			val = 0;
			for (j = xranges[i]; j < xranges[i + 1]; j++) {
				k = (fft_buf[j] + fft_buf[256+j]) / 2;
				if (k > val)
					val = k;
			}
			val >>= 8;
			
			if (val > 0) {
				val = (int)(log(val) * y_scale * y_scale);
			} else {
				val = 0;
			}
			if (val > MINI_H)
				val = MINI_H;

			if (val > (int)maxbar[i])
				maxbar[i] = val;
			else {
				maxbar[i] --;
				val = maxbar[i];
			}
			loc = bits + (MINI_W * MINI_H);
			for (h = val; h > 0; h--) {
				for (j = (MINI_W / BARS) * i + 0; j < (MINI_W / BARS) * i + ((MINI_W / BARS) - 1); j++) {
					*(loc + j) = val-h;
				}
				loc -= MINI_W;
			}
		}	
		GDK_THREADS_ENTER();
		gdk_draw_indexed_image(area->widget.window,
					area->widget.style->white_gc,
					0, 0, MINI_W, MINI_H,
					GDK_RGB_DITHER_NONE,
					bits, MINI_W, color_map);
		gdk_flush();
		GDK_THREADS_LEAVE();
		dosleep(30000);
			
	}	
	pthread_mutex_unlock(&scope_mutex);
	pthread_exit(NULL);
}



void start_internal_scope(void)
{
if (pthread_mutex_trylock(&scope_mutex) != 0) {
	alsaplayer_error("Internal scope already running");
	return;
}	
running = 1;
pthread_create(&scope_thread, NULL,
		(void *(*)(void *))scope_run, NULL);
return;
}

void stop_internal_scope(void)
{
running = 0;
pthread_join(scope_thread, NULL);
}


void shutdown_internal_scope(void)
{
stop_internal_scope();
return;
}


void internal_scope_set_data(void *buffer, int size)
{
}

void internal_scope_set_fft(void *fft_data, int samples, int channels)
{
	if (!fft_data) {
		memset(fft_buf, 0, sizeof(fft_buf));
		return;
	}
	memcpy(fft_buf, fft_data, sizeof(int) * samples * channels);
}

	
scope_plugin internal_scope = {
	SCOPE_PLUGIN_VERSION,
	"Internal Scope v0.0",
	"Andy Lo A Foe",
	NULL,
	init_internal_scope,
	start_internal_scope,
	internal_scope_running,
	stop_internal_scope,
	shutdown_internal_scope,
	internal_scope_set_data,
	internal_scope_set_fft
};

