/*  Main.cpp - main() function and other utils
 *  Copyright (C) 1998-2002 Andy Lo A Foe <andy@alsaplayer.org>
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
 *
 *
 *  $Id$
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <math.h>
#include <stdarg.h>

#include "config.h"

#include "AlsaPlayer.h"
#include "SampleBuffer.h"
#include "CorePlayer.h"
#include "Playlist.h"
#include "Effects.h"
#include "input_plugin.h"
#include "output_plugin.h"
#include "interface_plugin.h"
#include "utilities.h"
#include "prefs.h"
#include "alsaplayer_error.h"
#include "external.cpp"		/* This is a dirty hack */

Playlist *playlist = NULL;

int global_reverb_on = 0;
int global_reverb_delay = 2;
int global_reverb_feedback = 0;

int global_verbose = 0;
int global_session_id = -1;

char *global_session_name = NULL;

prefs_handle_t *ap_prefs = NULL;

void control_socket_start(Playlist *);
void control_socket_stop();

static char addon_dir[1024];

static char *default_pcm_device = "default";

const char *default_output_addons[] = {
	{"alsa"},
	{"nas"},
	{"oss"},
	{"sparc"},
	{"esound"},
	{"sgi"},
	{"null"},
	NULL
};

extern "C" {

/* This code was swiped from Paul Davis' JACK */

static void default_alsaplayer_error(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
		va_end(ap);
	fputc('\n', stderr);
}

void (*alsaplayer_error) (const char *fmt, ...) = &default_alsaplayer_error;

};


void exit_sighandler(int x)
{
	static int sigcount = 0;

	++sigcount;
	if (sigcount == 1)
		alsaplayer_error("AlsaPlayer interrupted by signal %d", x);
	if (sigcount == 1)
		exit(1);
	if (sigcount > 5) {
		kill(getpid(), SIGKILL);
	}
}

void load_output_addons(AlsaNode * node, char *module = NULL)
{
	void *handle;
	char path[1024];
	struct stat statbuf;

	output_plugin_info_type output_plugin_info;

	if (module) {
		sprintf(path, "%s/output/lib%s.so", addon_dir, module);
#ifdef DEBUG
		printf("Loading output plugin: %s\n", path);
#endif
		if (stat(path, &statbuf) != 0)	// Error reading object
			return;
		if ((handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))) {
			output_plugin_info =
			    (output_plugin_info_type) dlsym(handle,
							    "output_plugin_info");
			if (output_plugin_info) {
				node->RegisterPlugin(output_plugin_info());
				return;
			} else {
				alsaplayer_error
				    ("symbol error in shared object: %s\n"
				     "Try starting up with \"-o\" to load a\n"
				     "specific output module (alsa, oss, esd, etc.)",
				     path);
				dlclose(handle);
				return;
			}
		} else {
			alsaplayer_error("%s\n", dlerror());
		}
	}

	else

		for (int i = 0; default_output_addons[i]; i++) {
			sprintf(path, "%s/output/lib%s.so", addon_dir,
				default_output_addons[i]);
			if (stat(path, &statbuf) != 0)
				continue;
			if ((handle =
			     dlopen(path, RTLD_NOW | RTLD_GLOBAL))) {
				output_plugin_info =
				    (output_plugin_info_type) dlsym(handle,
								    "output_plugin_info");
				if (output_plugin_info) {
#ifdef DEBUG
					alsaplayer_error
					    ("Loading output plugin: %s\n",
					     path);
#endif
					if (node->
					    RegisterPlugin
					    (output_plugin_info())) {
						if (!node->ReadyToRun()) {
							alsaplayer_error
							    ("%s failed to init",
							     path);
							continue;	// This is not clean
						}
						return;	// Return as soon as we load one successfully!
					} else {
						alsaplayer_error
						    ("%s failed to load",
						     path);
					}
				} else {
					alsaplayer_error
					    ("could not find symbol in shared object");
					dlclose(handle);
				}
			} else {
				alsaplayer_error("%s\n", dlerror());
			}
		}
	// If we arrive here it means we haven't found any suitable output-addons
	alsaplayer_error
	    ("I could not find a suitable output module on your\n"
	     "       system. Make sure they're in \"%s/output/\".\n"
	     "       Use the -o parameter to select one.", addon_dir);
}


interface_plugin_info_type load_interface(char *name)
{
	void *handle;
	char path[1024];
	struct stat statbuf;

	interface_plugin_info_type plugin_info;

	if (name) {
		if (strchr(name, '.'))
			strcpy(path, name);
		else
			sprintf(path, "%s/interface/lib%s.so", addon_dir, name);
#ifdef DEBUG
		alsaplayer_error("Loading output plugin: %s\n", path);
#endif
		if (stat(path, &statbuf) != 0)	// Error reading object
			return NULL;
		if ((handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL))) {
			plugin_info =
			    (interface_plugin_info_type) dlsym(handle,
			    			"interface_plugin_info");
			if (plugin_info) {
				interface_plugin *plugin = plugin_info();
				if (plugin)
					plugin->handle = handle;
				return plugin_info;
			} else {
				alsaplayer_error
				    ("symbol error in shared object: %s", path);
				dlclose(handle);
				return NULL;
			}
		} else {
			alsaplayer_error("%s\n", dlerror());
		}
	}
	return NULL;
}



extern int init_reverb();
extern bool reverb_func(void *arg, void *data, int size);

static char *copyright_string =
    "AlsaPlayer " VERSION
    "\n(C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org> and others.";

static void help()
{
	fprintf(stdout,
		"\n"
		"Usage: alsaplayer [options] [filename <filename> ...]\n"
		"\n"
		"Available options:\n"
		"\n"
		"  -d,--device string      select card and device [default=\"default\"]\n"
		"  -e,--enqueue file(s)    queue files in running alsaplayer\n"
		"  -f,--fragsize #         fragment size in bytes [default=4096]\n"
		"  -F,--frequency #        output frequency [default=%d]\n"
		"  -g,--fragcount #        fragment count [default=8]\n"
		"  -h,--help               print this help message\n"
		"  -i,--interface iface    load in the iface interface [default=gtk]\n"
		"  -l,--volume #           set software volume [0-100]\n"
		"  -n,--session #          select session # [default=0]\n"
		"  -p,--path [path]        print/set the path alsaplayer looks for add-ons\n"
		"  -q,--quiet              quiet operation. no output\n"
		"  -r,--realtime           enable realtime scheduling (must be SUID root)\n"
		"  -s,--session-name name  name this session \"name\"\n"
		"  -v,--version            print version of this program\n"
		"  --verbose               be verbose about the output\n"
		"\n"
		"Testing options:\n"
		"\n"
		"  --reverb                use reverb function (CPU intensive!)\n"
		"  -S,--loopsong           loop file\n"
		"  -P,--looplist           loop Playlist\n"
		"  -x,--crossfade          crossfade between playlist entries (experimental)\n"
		"  -o,--output [alsa|oss|nas|sgi|...]  Use ALSA, OSS, NAS, SGI, etc. driver for output\n"
		"\n", OUTPUT_RATE);
}

static void version()
{
	fprintf(stdout, "%s version %s\n\n", PACKAGE, VERSION);
}


static char *get_homedir()
{
	char *homedir = NULL;

	if ((homedir = getenv("HOME")) == NULL) {
		homedir = strdup("/tmp");
	}

	return homedir;
}


int main(int argc, char **argv)
{
	char *use_pcm = default_pcm_device;
	char *homedir;
	char prefs_path[1024];
	int use_fragsize;
	int use_fragcount;
	int use_reverb = 0;	// TEMP!
	int use_loopSong = 0;
	int use_loopList = 0;
	int be_quiet = 0;
	int do_enqueue = 0;
	int use_realtime = 0;
	int use_freq = OUTPUT_RATE;
	int use_alsa = 1;
	int use_oss = 0;
	int use_sgi = 0;
	int use_null = 0;
	int use_esd = 0;
	int use_sparc = 0;
	int use_nas = 0;
	int use_user = 0;
	int last_arg = 0;
	int arg_pos = 0;
	int use_vol = 100;
	int use_session = 0;
	int use_crossfade = 0;
	int use_other_output = 0;
	int tmp;
	char use_output[256];
	char use_interface[256];
	prefs_handle_t *prefs;

	// First setup signal handler
	signal(SIGTERM, exit_sighandler);	// kill
	signal(SIGHUP, exit_sighandler);	// kill -HUP / xterm closed
	signal(SIGINT, exit_sighandler);	// Interrupt from keyboard
	signal(SIGQUIT, exit_sighandler);	// Quit from keyboard
	// fatal errors
	signal(SIGBUS, exit_sighandler);	// bus error
	signal(SIGSEGV, exit_sighandler);	// segfault
	signal(SIGILL, exit_sighandler);	// illegal instruction
	signal(SIGFPE, exit_sighandler);	// floating point exc.
	signal(SIGABRT, exit_sighandler);	// abort()

	strcpy(addon_dir, ADDON_DIR);

	init_effects();

	homedir = get_homedir();

	sprintf(prefs_path, "%s/.alsaplayer/", homedir);
	mkdir(prefs_path, 0700);	/* XXX We don't do any error checking here */
	sprintf(prefs_path, "%s/.alsaplayer/config", homedir);

	ap_prefs = prefs_load(prefs_path);

	memset(use_interface, 0, sizeof(use_interface));

	/* Initialize some settings (and populate the prefs system if needed */

	use_fragsize =
	    prefs_get_int(ap_prefs, "main", "period_size", 4096);
	use_fragcount = prefs_get_int(ap_prefs, "main", "period_count", 8);

	for (arg_pos = 1; arg_pos < argc; arg_pos++) {
		if (strcmp(argv[arg_pos], "--help") == 0 ||
		    strcmp(argv[arg_pos], "-h") == 0) {
			help();
			return 0;
		} else if (strcmp(argv[arg_pos], "--interface") == 0 ||
			   strcmp(argv[arg_pos], "-i") == 0) {
			if (argc <= arg_pos + 1) {
				alsaplayer_error
				    ("argument expected for %s",
				     argv[arg_pos]);
				return 1;
			}
			strcpy(use_interface, argv[++arg_pos]);
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--volume") == 0 ||
			   strcmp(argv[arg_pos], "-l") == 0) {
			tmp = -1;
			if (sscanf(argv[++arg_pos], "%d", &tmp) != 1 ||
			    (tmp < 0 || tmp > 100)) {
				alsaplayer_error
				    ("invalid value for volume: %d", tmp);
				return 1;
			}
			use_vol = tmp;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--version") == 0 ||
			   strcmp(argv[arg_pos], "-v") == 0) {
			version();
			return 0;
		} else if (strcmp(argv[arg_pos], "--enqueue") == 0 ||
			   strcmp(argv[arg_pos], "-e") == 0) {
			do_enqueue = 1;
			arg_pos++;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--session") == 0 ||
			   strcmp(argv[arg_pos], "-n") == 0) {
			if (sscanf(argv[++arg_pos], "%d", &tmp) != 1
			    || tmp < 0) {
				alsaplayer_error
				    ("invalid value for session: %d", tmp);
				return 1;
			}
			use_session = tmp;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--session-name") == 0 ||
			   strcmp(argv[arg_pos], "-s") == 0) {
			if (0 < strlen(argv[++arg_pos]) < 32) {
				global_session_name =
				    (char *) malloc(strlen(argv[arg_pos]) +
						    1);
				if (global_session_name)
					strcpy(global_session_name,
					       argv[arg_pos]);
				last_arg = arg_pos;
			} else {
				alsaplayer_error
				    ("expecting session name (32 chars max)\n");
				return 1;
			}
		} else if (strcmp(argv[arg_pos], "--crossfade") == 0 ||
			   strcmp(argv[arg_pos], "-x") == 0) {
			use_crossfade = 1;
			arg_pos++;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--path") == 0 ||
			   strcmp(argv[arg_pos], "-p") == 0) {
			if (arg_pos + 1 == argc) {	// Last option so display and exit
				fprintf(stdout,
					"Input  add-on path: %s/input/\n",
					addon_dir);
				fprintf(stdout,
					"Output add-on path: %s/output/\n",
					addon_dir);
				fprintf(stdout,
					"Scope  add-on path: %s/scopes/\n",
					addon_dir);
				return 0;
			}
			strcpy(addon_dir, argv[++arg_pos]);
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--reverb") == 0) {
			use_reverb = 1;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--loopsong") == 0 ||
			   strcmp(argv[arg_pos], "-S") == 0) {
			use_loopSong = 1;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--looplist") == 0 ||
			   strcmp(argv[arg_pos], "-P") == 0) {
			use_loopList = 1;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--realtime") == 0 ||
			   strcmp(argv[arg_pos], "-r") == 0) {
			use_realtime = 1;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--output") == 0 ||
			   strcmp(argv[arg_pos], "-o") == 0) {
			//printf("Use define output method!\n");                
			if (argc <= arg_pos + 1) {
				alsaplayer_error
				    ("argument expected for %s\n",
				     argv[arg_pos]);
				return 1;
			}
			use_alsa = use_sparc = use_esd = use_oss = 0;
			if (strcmp(argv[arg_pos + 1], "alsa") == 0) {
				//printf("using ALSA output method\n");
				use_alsa = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos + 1], "sgi") == 0) {
				use_sgi = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos + 1], "oss") == 0) {
				//printf("Using OSS output method\n");
				use_oss = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos + 1], "null") == 0) {
				use_null = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos + 1], "esd") == 0) {
				//printf("Using ESD output method\n");
				use_esd = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos + 1], "sparc") == 0) {
				use_sparc = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos + 1], "nas") == 0) {
				use_nas = 1;
				last_arg = ++arg_pos;
			} else {
				use_other_output = 1;
				strcpy(use_output, argv[arg_pos + 1]);
				last_arg = ++arg_pos;
			}
			use_user = 1;
		} else if (strcmp(argv[arg_pos], "--quiet") == 0 ||
			   strcmp(argv[arg_pos], "-q") == 0) {
			be_quiet = 1;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--verbose") == 0) {
			global_verbose = 1;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--frequency") == 0 ||
			   strcmp(argv[arg_pos], "-F") == 0) {
			if (sscanf(argv[++arg_pos], "%d", &use_freq) != 1) {
				alsaplayer_error
				    ("numeric argument expected for %s",
				     argv[arg_pos - 1]);
				return 1;
			}
			if (use_freq < 8000 || use_freq > 48000) {
				alsaplayer_error
				    ("frequency out of range [8000-48000]");
				return 1;
			}
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--fragsize") == 0 ||
			   strcmp(argv[arg_pos], "-f") == 0) {
			if (sscanf(argv[++arg_pos], "%d", &use_fragsize) !=
			    1) {
				alsaplayer_error
				    ("numeric argument expected for %s\n",
				     argv[arg_pos - 1]);
				return 1;
			}
			if (!use_fragsize || (use_fragsize % 32)) {
				alsaplayer_error
				    ("invalid fragment size, must be multiple of 32");
				return 1;
			}
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--fragcount") == 0 ||
			   strcmp(argv[arg_pos], "-g") == 0) {
			if (sscanf(argv[++arg_pos], "%d", &use_fragcount)
			    != 1) {
				alsaplayer_error
				    ("numeric argument expected for --fragcount");
				return 1;
			}
			if (!use_fragcount) {
				alsaplayer_error
				    ("fragment count cannot be 0");
				return 1;
			}
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--device") == 0 ||
			   strcmp(argv[arg_pos], "-d") == 0) {
			use_pcm = argv[++arg_pos];
			last_arg = arg_pos;
		}
	}
	if (global_verbose)
		fprintf(stdout, "%s\n", copyright_string);

	// Check if we need to enqueue the files
	if (do_enqueue) {
		char queue_name[2048];
		int ap_result = 0;

		if (use_session == 0) {
			for (; use_session < 32; use_session++) {
				if (ap_session_running(use_session)) {
					ap_result = 0;
					break;
				}
				ap_result = -1;
			}
		}
		while (last_arg < argc && ap_result == 0) {
			if (argv[last_arg][0] != '/') {	// Not absolute so append cwd
				if (getcwd(queue_name, 1024) == NULL) {
					alsaplayer_error
					    ("error getting cwd\n");
					return 1;
				}
				strcat(queue_name, "/");
				strcat(queue_name, argv[last_arg]);
			} else
				strcpy(queue_name, argv[last_arg]);
			last_arg++;
			if (ap_set_string
			    (use_session, AP_SET_STRING_ADD_FILE,
			     queue_name) == -1) {
				last_arg--;
				ap_result = -1;
			}
		}
		if (ap_result != -1)
			return 0;
	}

	AlsaNode *node;

	// Check if we want jack
	if (strcmp(argv[0], "jackplayer") == 0 ||
	    strcmp(use_output, "jack") == 0) {
		if (strcmp(use_pcm, default_pcm_device) != 0) {
			printf("not default\n");
			char *comma;
			sprintf(use_output, "jack %s", use_pcm);
			if (comma = strchr(use_output, ',')) {
				*comma = ' ';
			}
		} else {
			sprintf(use_output, "jack");
		}
		node = new AlsaNode(use_output, use_realtime);
	} else {		// Else do the usual plugin based thing
		node = new AlsaNode(use_pcm, use_realtime);
		if (use_user) {
			if (use_alsa)
				load_output_addons(node, "alsa");
			else if (use_oss)
				load_output_addons(node, "oss");
			else if (use_esd)
				load_output_addons(node, "esound");
			else if (use_sparc)
				load_output_addons(node, "sparc");
			else if (use_nas)
				load_output_addons(node, "nas");
			else if (use_sgi)
				load_output_addons(node, "sgi");
			else if (use_null)
				load_output_addons(node, "null");
			else if (use_other_output) {
				load_output_addons(node, use_output);
			}
		} else
			load_output_addons(node);
	}

	int output_is_ok = 0;
	int output_alternate = 0;

	do {
		if (!node || !node->ReadyToRun()) {
			alsaplayer_error
			    ("failed to load output add-on. exitting...");
			return 1;
		}
		if (!node->SetSamplingRate(use_freq) ||
		    !node->SetStreamBuffers(use_fragsize, use_fragcount, 2)) {
			alsaplayer_error
			    ("failed to configure output device...trying OSS");
			/* Special case for OSS, since it's easiest to get going, so try it */
			if (!output_alternate) {
				output_alternate = 1;
				load_output_addons(node, "oss");
				continue;
			} else {
				return 1;
			}
		}
		output_is_ok = 1;	/* output device initialized */
	} while (!output_is_ok);

	// Initialise playlist - must be done before things try to register with it
	playlist = new Playlist(node);

	if (!playlist) {
		alsaplayer_error("Failed to create Playlist object");
		return 1;
	}
	// Add any command line arguments to the playlist
	if (last_arg < argc) {
		std::vector < std::string > newitems;
		while (last_arg < argc) {
			newitems.push_back(std::string(argv[last_arg++]));
		}
		playlist->Insert(newitems, playlist->Length());
	}
	// If playlist is empty check if we have a playlist from
	// the previous run
	if (!playlist->Length()) {
		homedir = get_homedir();
		sprintf(prefs_path, "%s/.alsaplayer/alsaplayer.m3u", homedir);
		playlist->Pause();
		playlist->Load(prefs_path, playlist->Length(), false);
	} else {
		playlist->UnPause();
	}

	// Loop song
	if (use_loopSong) {
		playlist->LoopSong();
	}
	// Loop Playlist
	if (use_loopList) {
		playlist->LoopPlaylist();
	}
	// Cross fading
	if (use_crossfade) {
		playlist->Crossfade();
	}

	interface_plugin_info_type interface_plugin_info;
	interface_plugin *ui;

	if (sscanf(argv[0], "alsaplayer-%s", &use_interface) == 1 ||
	    sscanf(argv[0], "jackplayer-%s", &use_interface) == 1) {
		/* Determine interface from the command line */
		printf("Using interface %s\n", use_interface);
	}
	if (strlen(use_interface)) {
		if (!
		    (interface_plugin_info =
		     load_interface(use_interface))) {
			alsaplayer_error("Failed to load interface %s\n",
					 use_interface);
			goto _fatal_err;
		}
	} else {
		if (!(interface_plugin_info =
		      load_interface(prefs_get_string
				     (ap_prefs, "main",
				      "default_interface", "gtk")))) {
			if (!
			    (interface_plugin_info =
			     load_interface(prefs_get_string
					    (ap_prefs, "main",
					     "fallback_interface",
					     "text")))) {
				alsaplayer_error
				    ("Failed to load text interface. This is bad (%s,%s)",
				     prefs_get_string(ap_prefs, "main",
						      "default_interface",
						      "gtk"),
				     prefs_get_string(ap_prefs, "main",
						      "default_interface",
						      "gtk"));
				goto _fatal_err;
			}
		}
	}
	if (interface_plugin_info) {
		ui = interface_plugin_info();
		// Load socket interface first
		control_socket_start(playlist);
		if (global_verbose)
			fprintf(stdout, "Interface plugin: %s\n",
				ui->name);
		if (!ui->init()) {
			alsaplayer_error
			    ("Failed to load interface plugin. Should fall back to text\n");
		} else {
			ui->start(playlist, argc, argv);
			ui->close();
			dlclose(ui->handle);
		}
		control_socket_stop();
	}
	// Save playlist before exit
	homedir = get_homedir();
	sprintf(prefs_path, "%s/.alsaplayer/alsaplayer", homedir);
	playlist->Save(prefs_path, PL_FORMAT_M3U);

      _fatal_err:
	delete playlist;
	//delete p;
	delete node;
	if (global_session_name)
		free(global_session_name);

	// Save preferences
	if (ap_prefs)
		prefs_save(ap_prefs);

	return 0;
}
