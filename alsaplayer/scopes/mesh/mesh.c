#include "trackball.h"

Display *meshscope_dpy = NULL;
static sound_sample actSS[FFT_BUFFER_SIZE];
static sound_sample oldSS[FFT_BUFFER_SIZE];
static float val[NUMOLD][NUMBARS];
static Colormap colormap = 0;
static Atom wm_delete_window_atom;
static GLXContext glxcontext = NULL;
static fft_state *fftstate;
static pthread_t meshscope_thread;
static pthread_mutex_t meshscope_mutex;
static int running = 0;
Window *meshscope_win;
static int is_init = 0;
float meshscope_curquat[4];

GLfloat lightZeroPosition[] =
{0.0, 0.0, 0.0, 1.0};
GLfloat lightZeroColor[] =
{1.0, 1.0, 1.0, 1.0};
static int lighting = 0;
static int solid = 0;
static double fftmult[FFT_BUFFER_SIZE / 2 + 2];


void stop_meshscope ()
{
	running = 0;
}


void meshscope_set_data (void *audio_buffer, int size)
{
	int i;
	short *sound = (short *) audio_buffer;

	if (!sound) {
		memset (&actSS, 0, sizeof (actSS));
		return;
	}
	if (running && sound) {
		sound_sample *newset = actSS;
		sound += (size / 2 - FFT_BUFFER_SIZE) * 2; // Use the very latest data

		for (i = 0; i < FFT_BUFFER_SIZE; i++) {
			*newset++ = (sound_sample) ((
			(int) (*sound) + (int) *(sound + 1)) >> 1);
			sound += 2;
		}
	}
}


static Window create_window (int width, int height)
{
  int attr_list[] =
  {GLX_RGBA,
   GLX_DEPTH_SIZE, 16,
   GLX_DOUBLEBUFFER,
   None};
  int scrnum;
  XSetWindowAttributes attr;
  unsigned long mask;
  Window root, win;
  XVisualInfo *visinfo;
  Atom wm_protocols[1];

  scrnum = DefaultScreen (meshscope_dpy);
  root = RootWindow (meshscope_dpy, scrnum);

  visinfo = glXChooseVisual (meshscope_dpy, scrnum, attr_list);
  if (!visinfo)
    return 0;

  attr.background_pixel = 0;
  attr.border_pixel = 0;
  attr.colormap = colormap = XCreateColormap (meshscope_dpy, root, visinfo->visual, AllocNone);
  attr.event_mask = StructureNotifyMask | KeyPressMask;
  mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

  win = XCreateWindow (meshscope_dpy, root, 0, 0, width, height,
                       0, visinfo->depth, InputOutput,
                       visinfo->visual, mask, &attr);
  wm_delete_window_atom = wm_protocols[0] = XInternAtom (meshscope_dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols (meshscope_dpy, win, wm_protocols, 1);

  glxcontext = glXCreateContext (meshscope_dpy, visinfo, NULL, True);

  glXMakeCurrent (meshscope_dpy, win, glxcontext);
  XMapWindow (meshscope_dpy, win);
  XFlush (meshscope_dpy);

  return win;
}


void initGL ()
{
  glXMakeCurrent (meshscope_dpy, meshscope_win, glxcontext);
  trackball (meshscope_curquat, 0.0, 0.0, 0.0, 0.0);
  if (lighting) {
    glEnable (GL_LIGHTING);
    glLightModeli (GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
    glLightfv (GL_LIGHT0, GL_POSITION, lightZeroPosition);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, lightZeroColor);
    glEnable (GL_LIGHT0);
  } else {
    glDisable (GL_LIGHTING);
  }
  glMatrixMode (GL_PROJECTION);
  glMatrixMode (GL_PROJECTION);
  gluPerspective ( /* field of view in degree */ 40.0,
  /* aspect ratio */ 1.0,
  /* Z near */ 1.0,
  /* Z far */ 100.0);
  glMatrixMode (GL_MODELVIEW);
  gluLookAt (0.0, 20.0, 35.0,        /* eye is at (0,0,30) */
             0.0, 0.0, 0.0,        /* center is at (0,0,0) */
             0.0, 1.0, 0.0);        /* up is in positive Y direction */
  glPushMatrix ();                /* dummy push so we can pop on model
                                   recalc */
  glLineWidth (1.0);
  if (!solid)
    glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
  glEnable (GL_DEPTH_TEST);

}


Window *create_meshscope_window ()
{
	meshscope_dpy = XOpenDisplay (NULL);
	meshscope_win = create_window (MESH_W, MESH_H);
	return meshscope_win;
}


void drawMesh ()
{
  int i, j, k, m;
  static unsigned short l = NUMOLD;
  float f, g, color[3];
  double fftout[NUMSAMPLES];
  memcpy (&oldSS, &actSS, sizeof (actSS));

  fft_perform (oldSS, fftout, fftstate);
  glXMakeCurrent (meshscope_dpy, meshscope_win, glxcontext);
  /* Draw simple triangle */
  glClearColor (0, 0, 0, 1);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glColor3f (1, 1, 1);
  for (i = 1; i < NUMSAMPLES - 1; i += STEPSIZE) {
    glBegin (PRIMITIVES);
    k = i / STEPSIZE+1;
    for (j = i, val[l % NUMOLD][k] = 0; j < i + STEPSIZE; j++) {
      f = (sqrt (fftout[j]) * fftmult[j]);
      if (f > val[l % NUMOLD][k])
        val[l % NUMOLD][k] = f;
    }
    if (val[l % NUMOLD][k] > 127.0)
      val[l % NUMOLD][k] = 127.0;
    if (val[l % NUMOLD][k] < 0.0)
      val[l % NUMOLD][k] = 0.0;
    for (m = 0; m < NUMOLD; m++) {
      g = (float) (val[(l - m) % NUMOLD][k]) / 127.0 * 2.0;
      color[0] = MAX (g - 1.0, 0.0);
      color[2] = MIN (2.0 - g, 1.0);
      if (lighting)
        glMaterialfv (GL_FRONT, GL_DIFFUSE, color);
      else
        glColor3f (color[0], color[1], color[2]);
      glVertex3f ((float) i / (float) NUMSAMPLES * 20.0 - 10.0, val[(l - m) % NUMOLD][k] / 12.8, (5 - m) * 2);

      g = (float) (val[(l - m) % NUMOLD][k - 1]) / 127.0 * 2.0;
      color[0] = MAX (g - 1.0, 0.0);
      color[2] = MIN (2.0 - g, 1.0);
      if (lighting)
        glMaterialfv (GL_FRONT, GL_DIFFUSE, color);
      else
        glColor3f (color[0], color[1], color[2]);
      glVertex3f ((float) (i - STEPSIZE) / (float) NUMSAMPLES * 20.0 - 10.0, val[(l - m) % NUMOLD][k - 1] / 12.8, (5 - m) * 2);
    }
    glEnd ();
  }
  /* Swap backbuffer to front */
  glXSwapBuffers (meshscope_dpy, meshscope_win);
  l++;

}


static void meshscope(GtkWidget * win)
{
	Bool configured = FALSE;
	while (running) {
		while (XPending (meshscope_dpy)) {
			XEvent event;

			XNextEvent (meshscope_dpy, &event);
			switch (event.type) {
				case ConfigureNotify:
					glViewport (0, 0, event.xconfigure.width, event.xconfigure.height);
					configured = TRUE;
					break;
				case ButtonPress:
					mousepress(event);
					break;
				case ButtonRelease:
					mouserelease(event);
					break;
				case MotionNotify:
					mousemove(event);
					break;
				default:
					printf("event %x\n", event.type);
					break;
			}
		}
		drawMesh ();
		animate();
		dosleep (25000);
	}
	//XUnmapWindow(meshscope_win, d);
}


void run_meshscope (void *data)
{
	int i;
	nice (SCOPE_NICE);
	meshscope_win = create_meshscope_window ();
	initGL ();

	for (i = 0; i <= FFT_BUFFER_SIZE / 2 + 1; i++) {
		double mult = (double) 128 / ((FFT_BUFFER_SIZE * 16384) ^ 2);
		// Result now guaranteed (well, almost) to be in range 0..128
		// Low values represent more frequencies, and thus get more
		// intensity - this helps correct for that
		mult *= log (i + 1) / log (2);
		mult *= 3;	// Adhoc parameter, looks about right for me.

		fftmult[i] = mult;
	}
	memset(&val, 0, sizeof(val)); // reset
	memset(&actSS, 0, sizeof(actSS));
	meshscope(meshscope_win);
	pthread_mutex_unlock (&meshscope_mutex);
}


void start_meshscope (void *data)
{
	if (pthread_mutex_trylock (&meshscope_mutex) != 0) {
		printf ("meshscope already running\n");
		return;
	}
	running = 1;
	pthread_create (&meshscope_thread, NULL,
		(void *(*)(void *)) run_meshscope, data);
	pthread_detach (meshscope_thread);
}


static int open_meshscope ()
{
	fftstate = fft_init ();
	if (!fftstate)
		return 0;
	return 1;
}


static int init_meshscope ()
{
	pthread_mutex_init (&meshscope_mutex, NULL);		
	return 1;
}


static void close_meshscope ()
{
}


static int meshscope_running ()
{
	return running;
}


scope_plugin meshscope_plugin =
{
  SCOPE_PLUGIN_VERSION,
  {"Meshscope"},
  {"Stefan Eilemann & Andy Lo A Foe"},
  init_meshscope,
  open_meshscope,
  start_meshscope,
  meshscope_running,
  stop_meshscope,
  close_meshscope,
  meshscope_set_data
};


scope_plugin *scope_plugin_info ()
{
  return &meshscope_plugin;
}
