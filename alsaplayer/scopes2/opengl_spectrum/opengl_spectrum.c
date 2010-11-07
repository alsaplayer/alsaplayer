/*  opengl_spectrum.c (C) 2002 by Andy Lo A Foe <andy@alsaplayer.org>
 
 *  Based on code found in xmms:
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
 */

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "scope_plugin.h"
#include "alsaplayer_error.h"
#include "utilities.h"
#include "prefs.h"

#define NUM_BANDS 16

//#define NVIDIA_SYNC

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

static Display *dpy = NULL;
static Colormap colormap = 0;
static GLXContext glxcontext = NULL;
static Window window = 0;
static GLfloat y_angle = 45.0, y_speed = 0.5;
static GLfloat x_angle = 20.0, x_speed = 0.0;
static GLfloat z_angle = 0.0, z_speed = 0.0;
static GLfloat heights[16][16], scale;
static int going = FALSE, grabbed_pointer = FALSE;
static Atom wm_delete_window_atom;
static pthread_t draw_thread;
static pthread_mutex_t scope_mutex;

static int window_w;
static int window_h;

#ifdef NVIDIA_SYNC
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

static void stop_display(int);
static void oglspectrum_start(void);

static void wait_for_vsync()
{
#ifdef NVIDIA_SYNC	
	static int init = 0;
	static int fd = -1;
	static struct pollfd pollfds;
	if (!init) {
		fd = open("/dev/nvidia0", O_RDONLY);
		if (fd == -1) {
			alsaplayer_error("Error opening NVIDIA device /dev/nvidia0");
		} else {
			pollfds.fd = fd;
			pollfds.events = 0xffff;
			pollfds.revents = 0xffff;
			alsaplayer_error("Using NVIDIA poll method for vsync");
		}	 
		init = 1;	
	}
	poll (&pollfds, 1, -1);
#else
	dosleep(10000);
#endif	
}


static Window create_window(int width, int height)
{
	int attr_list[] = {
		GLX_RGBA,
		GLX_DEPTH_SIZE, 16,
		GLX_DOUBLEBUFFER,
		None
	};
	int scrnum;
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root, win;
	XVisualInfo *visinfo;
	Atom wm_protocols[1];

	if ((dpy = XOpenDisplay(NULL)) == NULL)
		return 0;

	scrnum = DefaultScreen(dpy);
	root = RootWindow(dpy, scrnum);

	if ((visinfo = glXChooseVisual(dpy, scrnum, attr_list)) == NULL)
		return 0;

	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = colormap = XCreateColormap(dpy, root,
						   visinfo->visual, AllocNone);
	attr.event_mask = StructureNotifyMask | KeyPressMask;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	win = XCreateWindow(dpy, root, 0, 0, width, height,
			    0, visinfo->depth, InputOutput,
			    visinfo->visual, mask, &attr);
	XmbSetWMProperties(dpy, win, "OpenGL Spectrum analyzer",
			   "OpenGL Spectrum analyzer", NULL, 0, NULL, NULL,
			   NULL);
	wm_delete_window_atom = wm_protocols[0] =
		XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, wm_protocols, 1);

	glxcontext = glXCreateContext(dpy, visinfo, NULL, True);

	XFree(visinfo);

	glXMakeCurrent(dpy, win, glxcontext);

	return win;
}


static void draw_rectangle(GLfloat x1, GLfloat y1, GLfloat z1, GLfloat x2, GLfloat y2, GLfloat z2)
{
	if(y1 == y2)
	{
	
		glVertex3f(x1, y1, z1);
		glVertex3f(x2, y1, z1);
		glVertex3f(x2, y2, z2);
		
		glVertex3f(x2, y2, z2);
		glVertex3f(x1, y2, z2);
		glVertex3f(x1, y1, z1);
	}
	else
	{
		glVertex3f(x1, y1, z1);
		glVertex3f(x2, y1, z2);
		glVertex3f(x2, y2, z2);
		
		glVertex3f(x2, y2, z2);
		glVertex3f(x1, y2, z1);
		glVertex3f(x1, y1, z1);
	}
}

static void draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue )
{
	GLfloat width = 0.1;

	glColor3f(red,green,blue);
	draw_rectangle(x_offset, height, z_offset, x_offset + width, height, z_offset + 0.1);
	draw_rectangle(x_offset, 0, z_offset, x_offset + width, 0, z_offset + 0.1);
	
	glColor3f(0.5 * red, 0.5 * green, 0.5 * blue);
	draw_rectangle(x_offset, 0.0, z_offset + 0.1, x_offset + width, height, z_offset + 0.1);
	draw_rectangle(x_offset, 0.0, z_offset, x_offset + width, height, z_offset );

	glColor3f(0.25 * red, 0.25 * green, 0.25 * blue);
	draw_rectangle(x_offset, 0.0, z_offset , x_offset, height, z_offset + 0.1);	
	draw_rectangle(x_offset + width, 0.0, z_offset , x_offset + width, height, z_offset + 0.1);

	
}

static void draw_bars(void)
{
	int x,y;
	GLfloat x_offset, z_offset, r_base, b_base;

	

	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glTranslatef(0.0,-0.5,-5.0);	      
	glRotatef(x_angle,1.0,0.0,0.0);
	glRotatef(y_angle,0.0,1.0,0.0);
	glRotatef(z_angle,0.0,0.0,1.0);

	glBegin(GL_TRIANGLES);
	for(y = 0; y < 16; y++)
	{
		z_offset = -1.6 + ((15 - y) * 0.2);

		b_base = y * (1.0 / 15);
		r_base = 1.0 - b_base;
			
		for(x = 0; x < 16; x++)
		{
			x_offset = -1.6 + (x * 0.2);			
				
			draw_bar(x_offset, z_offset, heights[y][x], r_base - (x * (r_base / 15.0)), x * (1.0 / 15), b_base);
		}
	}
	glEnd();

	glPopMatrix();
	wait_for_vsync();
	glXSwapBuffers(dpy,window);
}

#define DEFAULT_W	640
#define DEFAULT_H 480

void *draw_thread_func(void *arg)
{
	Bool configured = FALSE;

	window_w = prefs_get_int(ap_prefs, "opengl_spectrum", "width", DEFAULT_W);	
	window_h = prefs_get_int(ap_prefs, "opengl_spectrum", "height", DEFAULT_H);
	
	if ((window = create_window(window_w, window_h)) == 0)
	{
		alsaplayer_error("unable to create window");
		pthread_exit(NULL);
	}
	
	XMapWindow(dpy, window);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1, 1, -1, 1, 1.5, 10);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	while(going)
	{
		while(XPending(dpy))
		{
			XEvent event;
			KeySym keysym;
			char buf[16];
			
			XNextEvent(dpy, &event);
			switch(event.type)
			{
			case ConfigureNotify:
				glViewport(0,0,event.xconfigure.width, event.xconfigure.height);
				window_w = event.xconfigure.width;
				window_h = event.xconfigure.height;
				prefs_set_int(ap_prefs, "opengl_spectrum", "width", window_w);
				prefs_set_int(ap_prefs, "opengl_spectrum", "height", window_h);
				configured = TRUE;
				break;
			case KeyPress:

				
				XLookupString (&event.xkey, buf, 16, &keysym, NULL);
				switch(keysym)
				{
				case XK_Escape:
					
					going = FALSE;
					break;
				case XK_z:
					/*xmms_remote_playlist_prev(oglspectrum_vp.xmms_session); */
					break;
				case XK_x:
					/*xmms_remote_play(oglspectrum_vp.xmms_session); */
					break;
				case XK_c:
					/*xmms_remote_pause(oglspectrum_vp.xmms_session); */
					break;
				case XK_v:
					/*xmms_remote_stop(oglspectrum_vp.xmms_session); */
					break;
				case XK_b:
					/* xmms_remote_playlist_next(oglspectrum_vp.xmms_session); */
					break;
				case XK_Up:					
					x_speed -= 0.1;
					if(x_speed < -3.0)
						x_speed = -3.0;
					break;
				case XK_Down:					
					x_speed += 0.1;
					if(x_speed > 3.0)
						x_speed = 3.0;
					break;
				case XK_Left:
					y_speed -= 0.1;
					if(y_speed < -3.0)
						y_speed = -3.0;
					
					break;
				case XK_Right:
					y_speed += 0.1;
					if(y_speed > 3.0)
						y_speed = 3.0;
					break;
				case XK_w:
					z_speed -= 0.1;
					if(z_speed < -3.0)
						z_speed = -3.0;
					break;
				case XK_q:
					z_speed += 0.1;
					if(z_speed > 3.0)
						z_speed = 3.0;
					break;
				case XK_Return:
					x_speed = 0.0;
					y_speed = 0.5;
					z_speed = 0.0;
					x_angle = 20.0;
					y_angle = 45.0;
					z_angle = 0.0;
					break;					
				}
				
				break;
			case ClientMessage:
				if ((Atom)event.xclient.data.l[0] == wm_delete_window_atom)
				{
					going = FALSE;
				}
				break;
			}
		}
		if(configured)
		{
			x_angle += x_speed;
			if(x_angle >= 360.0)
				x_angle -= 360.0;
			
			y_angle += y_speed;
			if(y_angle >= 360.0)
				y_angle -= 360.0;

			z_angle += z_speed;
			if(z_angle >= 360.0)
				z_angle -= 360.0;

			draw_bars();
		}
	}

	if (glxcontext)
	{
		glXMakeCurrent(dpy, 0, NULL);
		glXDestroyContext(dpy, glxcontext);
		glxcontext = NULL;
	}
	if (window)
	{
		if (grabbed_pointer)
		{
			XUngrabPointer(dpy, CurrentTime);
			grabbed_pointer = FALSE;
		}

		XDestroyWindow(dpy, window);
		window = 0;
	}
	pthread_mutex_unlock(&scope_mutex);
	stop_display(0); /* Close down display */
	pthread_exit(NULL);
}

static void start_display(void)
{
	int x, y;

	for(x = 0; x < 16; x++)
	{
		for(y = 0; y < 16; y++)
		{
			heights[y][x] = 0.0;
		}
	}
	scale = 1.0 / log(256.0);

	x_speed = 0.0;
	y_speed = 0.5;
	z_speed = 0.0;
	x_angle = 20.0;
	y_angle = 45.0;
	z_angle = 0.0;

	going = TRUE;
	pthread_create(&draw_thread, NULL, draw_thread_func, NULL);
}

static void stop_display(int join_thread)
{
	if (going && join_thread)
	{
		going = FALSE;
		pthread_join(draw_thread, NULL);
	}

	if (colormap)
	{
		XFreeColormap(dpy, colormap);
		colormap = 0;
	}
	if (dpy)
	{
		XCloseDisplay(dpy);
		dpy = NULL;
	}
}

static int oglspectrum_init(void *arg)
{
	pthread_mutex_init(&scope_mutex, NULL);

	if (prefs_get_bool(ap_prefs, "opengl_spectrum", "active", 0)) {
		oglspectrum_start();
	}
	return 1;		
}	

static void oglspectrum_start(void)
{
	if (pthread_mutex_trylock(&scope_mutex) != 0) {
		alsaplayer_error("spectrum already running");
		return;
	}	
	start_display();
}

static void oglspectrum_stop(void)
{
	stop_display(1);
}

static void oglspectrum_set_fft(void *fft_buffer, int samples, int channels)
{
	int i,c;
	int y;
	GLfloat val;
	int *buf = (int *)fft_buffer;

	
	int xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};

	for(y = 15; y > 0; y--) {
		for(i = 0; i < 16; i++) {
			heights[y][i] = heights[y - 1][i];
		}
	}
	
	for(i = 0; i < NUM_BANDS; i++) {
		for(c = xscale[i], y = 0; c < xscale[i + 1]; c++) {
			if((buf[c]+buf[samples+c]) > y)
				y = buf[c]+buf[samples+c];
		}
		y >>= 7;
		if(y > 0)
			val = (log(y) * scale);
		else
			val = 0;
		heights[0][i] = val;
	}
}

static int oglspectrum_running(void)
{
	return going;
}

static void oglspectrum_shutdown(void)
{
	prefs_set_bool(ap_prefs, "opengl_spectrum", "active", oglspectrum_running());
	if (oglspectrum_running()) {
		oglspectrum_stop();
	}
}


scope_plugin oglspectrum_plugin = {
	SCOPE_PLUGIN_VERSION,
	"Spectrum GL",
	"Andy Lo A Foe",
	NULL,
	oglspectrum_init,
	oglspectrum_start,
	oglspectrum_running,
	oglspectrum_stop,
	oglspectrum_shutdown,
	NULL,
	oglspectrum_set_fft
};


scope_plugin *scope_plugin_info(void)
{
	return &oglspectrum_plugin;
}

