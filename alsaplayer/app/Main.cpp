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
 *  VIM: set ts=8
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <math.h>
#include <stdarg.h>
#include <locale.h>

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
#include "message.c"		/* This is a dirty hack */
#include "reader.h"

#define MAX_REMOTE_SESSIONS	32

Playlist *playlist = NULL;

int global_reverb_on = 0;
int global_reverb_delay = 2;
int global_reverb_feedback = 0;

int global_verbose = 0;
int global_session_id = -1;
int global_quiet = 0;

char *global_session_name = NULL;
char *global_interface_script = NULL;
char *global_pluginroot = NULL;

prefs_handle_t *ap_prefs = NULL;

void control_socket_start(Playlist *, interface_plugin *ui);
void control_socket_stop();

static char *default_pcm_device = "default";


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

void alsaplayer_set_error_function(void (*func)(const char *, ...))
{
	alsaplayer_error = func;
}

}; /* extern "C" { */


void exit_sighandler(int x)
{
	static int sigcount = 0;

	++sigcount;
	if (sigcount == 1) {
		alsaplayer_error("AlsaPlayer interrupted by signal %d", x);
		exit(1);
	}	
	if (sigcount > 5) {
		kill(getpid(), SIGKILL);
	}
}


interface_plugin_info_type load_interface(const char *name)
{
	void *handle;
	char path[1024], *pluginroot;
	struct stat statbuf;

	interface_plugin_info_type plugin_info;
	interface_plugin *ui;

	if (!global_pluginroot)
		pluginroot = ADDON_DIR;
	else
		pluginroot = global_pluginroot;

	if (!name)
		return NULL;

	if (strchr(name, '.'))
		strncpy(path, name, sizeof(path));
	else
		snprintf(path, sizeof(path), "%s/interface/lib%s.so", pluginroot, name);
#ifdef DEBUG
	alsaplayer_error("Loading interface plugin: %s\n", path);
#endif
	if (stat(path, &statbuf) != 0)	// Error reading object
		return NULL;

	handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
	if (!handle) {
		alsaplayer_error("%s\n", dlerror());
		return NULL;
	}

	plugin_info = (interface_plugin_info_type) dlsym(handle, "interface_plugin_info");
	if (!plugin_info) {
		alsaplayer_error("symbol error in shared object: %s", path);
		dlclose(handle);
		return NULL;
	}

	interface_plugin *plugin = plugin_info();
	if (plugin)
		plugin->handle = handle;
	ui = plugin_info();
	if (ui->version != INTERFACE_PLUGIN_VERSION) {
		alsaplayer_error("Wrong interface plugin version (v%d, wanted v%d)",
		ui->version,
		INTERFACE_PLUGIN_VERSION - INTERFACE_PLUGIN_BASE_VERSION);
		alsaplayer_error("Error loading %s", path);
		alsaplayer_error("Please remove this file from your system");
		return NULL;
	}	

	return plugin_info;
}



extern int init_reverb();
extern bool reverb_func(void *arg, void *data, int size);

static char *copyright_string =
    "AlsaPlayer " VERSION
    "\n(C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org> and others.";

static void list_available_plugins(const char *plugindir)
{
	char path[1024], *pluginroot;
	struct stat buf;
	bool first = true;

	if (!plugindir)
		return;

	if (!global_pluginroot)
		pluginroot = ADDON_DIR;
	else
		pluginroot = global_pluginroot;

	snprintf(path, sizeof(path), "%s/%s", pluginroot, plugindir);

	DIR *dir = opendir(path);
	dirent *entry;

	if (!dir)
		return;

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		snprintf(path, sizeof(path), "%s/%s/%s", pluginroot, plugindir, entry->d_name);
		if (stat(path, &buf)) {
			continue;
		}	
		if (S_ISREG(buf.st_mode)) {
			char *ext = strrchr(path, '.');
			if (!ext)
				continue;
			ext++;
			if (strcasecmp(ext, "so")) {
				continue;
			}
			snprintf(path, sizeof(path), "%s", entry->d_name);
			ext = strrchr(path, '.');
			if (ext)
				*ext = '\0';
			if (strncmp(path, "lib", 3)) {
				continue;
			}	
			char *name = path + 3;
			if (strcmp(plugindir, "output") == 0) { // Remove trailing _out
				ext = strrchr(name, '_');
				if (ext)
					*ext = '\0';
			}
			if (first) { // don't print comma
				first = false;
			} else {
				printf(" | ");
			}
			printf("%s", name);
		}
	}
}



static void help()
{
	printf(
		"\n"
		"Usage: alsaplayer [options] [filename <filename> ...]\n"
		"\n"
		"Available options:\n"
		"\n"
		"  -b,--background         fork to background (useful for daemon interfaces)\n"
		"  -c,--config file        use given config file for this session\n"
		"  -h,--help               print this help message\n"
		"  -i,--interface iface    use specific interface [default=gtk]. choices:\n");
	printf(
		"                          [ ");
	list_available_plugins("interface");
	printf(
		" ]\n"
		"  -I,--script file        script to pass to interface plugin\n"
		"  -n,--session n          use this session id [default=0]\n"
		"  -l,--startvolume vol    start with this volume [default=100]\n"
		"  -p,--path path          set the path alsaplayer looks for add-ons\n"
		"  -q,--quiet              quiet operation. less output\n"
		"  -s,--session-name name  name this session \"name\"\n"
		"  -v,--version            print version of this program\n"
		"  --verbose               be verbose about the output\n"
		"  --nosave                do not save playlist content at exit\n"
		"\n"
		"Player control (use -n to select another session than the default):\n"
		"\n"
		"  -e,--enqueue file(s)  queue files in running alsaplayer\n"
		"  -E,--replace file(s)  clears and queues files in running alsaplayer\n"
		"  --volume vol          set software volume [0-100]\n"
		"  --start               start playing\n"
		"  --stop                stop playing\n"
		"  --prev                jump to previous track\n"
		"  --next                jump to next track\n"
		"  --seek second         jump to specified second in current track\n"
		"  --relative second     jump second seconds from current position\n"
		"  --jump track          jump to specified playlist track\n"
		"  --clear               clear whole playlist\n"
		"\n"
		"Sound driver options:\n"
		"\n"
		"  -d,--device string    select card and device [default=\"default\"]\n"
		"  -f,--fragsize n       fragment size in bytes [default=4096]\n"
		"  -F,--frequency n      output frequency [default=%d]\n"
		"  -g,--fragcount n      fragment count [default=8]\n"
		"  -o,--output output    use specific output driver [default=alsa]. choices:\n"
		"  -r,--realtime         enable realtime scheduling (with proper rights)\n", OUTPUT_RATE);
	printf(
		"                        [ ");
	list_available_plugins("output");
	printf(
		" ]\n"
		"\n"
		"Experimental options:\n"
		"\n"
		"  -S,--loopsong         loop file\n"
		"  -P,--looplist         loop playlist\n"
		"  -x,--crossfade        crossfade playlist entries\n"
		"\n");
}

static void version()
{
	printf("%s %s\n", PACKAGE, VERSION);
}


static char *get_homedir()
{
	char *homedir = NULL;

	if ((homedir = getenv("HOME")) == NULL) {
		homedir = strdup("/tmp");
	}

	return homedir;
}

static int get_interface_from_argv0 (char *argv0, char *str)
{
	char *bs = strrchr (argv0, '/');
	
	if (bs)  argv0 = ++bs;
	
	if (sscanf(argv0, "alsaplayer-%s", str) == 1)  return 1;
  	if (sscanf(argv0, "jackplayer-%s", str) == 1)  return 1;

	return 0;
}

int main(int argc, char **argv)
{
	char *device_param = default_pcm_device;
	char *homedir;
	char prefs_path[1024];
	char str[1024];
	int start_vol = 100;
	int ap_result = 0;
	int use_fragsize = -1; // Initialized
	int use_fragcount = -1; // later
	int do_reverb = 0;
	int do_loopsong = 0;
	int do_looplist = 0;
	int do_enqueue = 0;
	int do_replace = 0;
	int do_realtime = 0;
	int do_remote_control = 0;
	int do_start = 0;
	int do_stop = 0;
	int do_prev = 0;
	int do_next = 0;
	int do_jump = -1;
	int do_clear = 0;
	int do_seek = -1;
	int do_relative = 0;
	int do_setvol = 0;
	
	int use_freq = OUTPUT_RATE;
	int use_vol = 100;
	int use_session = 0;
	int do_crossfade = 0;
	int do_save = 1;
	char *use_output = NULL;
	char *use_interface = NULL;
	char *use_config = NULL;
	
	int opt;
	int option_index;
	const char *options = "bCc:d:eEf:F:g:hi:JI:l:n:NMp:qrs:vRSQTPVxo:Z:";
	struct option long_options[] = {
		{ "background", 0, 0, 'b' },
		{ "config", 1, 0, 'c' },
		{ "device", 1, 0, 'd' },
		{ "enqueue", 0, 0, 'e' },
		{ "replace", 0, 0, 'E' },
		{ "fragsize", 1, 0, 'f' },
		{ "frequency", 1, 0, 'F' },
		{ "fragcount", 1, 0, 'g' },
		{ "help", 0, 0, 'h' },
		{ "interface", 1, 0, 'i' },
		{ "volume", 1, 0, 'Y' },
		{ "session", 1, 0, 'n' },
		{ "nosave", 0, 0, 'N' },
		{ "path", 1, 0, 'p' },
		{ "quiet", 0, 0, 'q' },
		{ "realtime", 0, 0, 'r' },
		{ "script", 1, 0, 'I'},
		{ "session-name", 1, 0, 's' },
		{ "version", 0, 0, 'v' },
		{ "verbose", 0, 0, 'V' },
		{ "reverb", 0, 0, 'R' },
		{ "loopsong", 0, 0, 'L' },
		{ "looplist", 0, 0, 'P' },
		{ "crossfade", 0, 0, 'x' },
		{ "output", 1, 0, 'o' },
		{ "stop", 0, 0, 'U' },
		{ "start", 0, 0, 'T' },
		{ "prev", 0, 0, 'Q' },
		{ "next", 0, 0, 'M' },
		{ "jump", 1, 0, 'J' },
		{ "seek", 1, 0, 'X' },
		{ "relative", 1, 0, 'Z' },
		{ "clear", 0, 0, 'C' },
		{ "startvolume", 1, 0, 'l' },
		{ 0, 0, 0, 0 }
	};	
		

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

	// Enable locale support
	setlocale (LC_ALL, "");
	
	// Init global mutexes
	pthread_mutex_init(&playlist_sort_seq_mutex, NULL);
#if !defined(EMBEDDED)
	init_effects();
#endif
	while ((opt = getopt_long(argc, argv, options, long_options, &option_index)) != EOF) {
		switch(opt) {
			case 'b': switch(fork()) {
					case -1: // Child, but error
						alsaplayer_error("Could not fork to background");
						break;
					case 0: // Child, continue as normal
						break;
					default:
						// Parent, exit
						exit(0);
				}		
				break;
			case 'c':
				if (strlen(optarg) < 1023) {
					use_config = optarg;
				} else {
					alsaplayer_error("config file path too long");
					return 1;
				}
				break;
			case 'd':
				device_param = optarg;
				break;
			case 'E':
				do_replace = 1;
			case 'e':
				do_enqueue = 1;
				break;
			case 'f':
				use_fragsize = atoi(optarg);
				if (!use_fragsize || (use_fragsize % 32)) {
					alsaplayer_error("invalid fragment size, must be multiple of 32");
					return 1;
				}
				if (use_fragsize > 16384) {
					alsaplayer_error("fragment sizes larger than 16384 bytes are not supported");
					return 1;
				}	
				break;
			case 'F':
				use_freq = atoi(optarg);
				if (use_freq < 8000 || use_freq > 48000) {
					alsaplayer_error("frequency out of range (8000-48000)");
					return 1;
				}
				break;
			case 'g':
				use_fragcount = atoi(optarg);
				if (use_fragcount < 2 || use_fragcount > 64) {
					alsaplayer_error("fragcount out of range (2-64)");
					return 1;
				}
				break;
			case 'h':
				help();
				return 0;
			case 'i':
				use_interface = optarg;
				break;
			case 'l':
				start_vol = atoi(optarg);
				if (start_vol < 0 || start_vol > 100) {
					alsaplayer_error("volume out of range: using 100");
					start_vol = 100;
				}
				break;
			case 'Y':
				do_remote_control = 1;
				do_setvol = 1;
				use_vol = atoi(optarg);
				if (use_vol < 0 || use_vol > 100) {
					alsaplayer_error("volume out of range: using 100");
					use_vol = 100;
				}
				break;
			case 'n':
				use_session = atoi(optarg);
				break;
			case 'N':
				do_save = 0;
				break;
			case 'p':
				global_pluginroot = strdup(optarg);
				break;
			case 'q':
				global_quiet = 1;
				break;
			case 'r':
				do_realtime = 1;
				break;
			case 's':
				if (strlen(optarg) < 32) {
					global_session_name = strdup(optarg);
				} else {
					alsaplayer_error("max 32 char session name, ignoring");
				}
				break;
			case 'v':
				version();
				return 0;
			case 'V':
				global_verbose = 1;
				break;
			case 'R':
				do_reverb = 1;
				break;
			case 'S':
				do_loopsong = 1;
				break;
			case 'P':
				do_looplist = 1;
				break;
			case 'x':
				do_crossfade = 1;
				break;
			case 'o':
				use_output = optarg;
				break;
			case '?':
				return 1;
			case 'I':
				global_interface_script = optarg;
				break;
			case 'U':
				do_remote_control = 1;
				do_stop = 1;
				break;
			case 'T': 
				do_remote_control = 1;
				do_start = 1;
				break;
			case 'Q':
				do_remote_control = 1;
				do_prev = 1;
				break;
			case 'M':
				do_remote_control = 1;
				do_next = 1;
				break;
			case 'J':
				do_remote_control = 1;
				do_jump = atoi(optarg);
				break;
			case 'C':
				do_remote_control = 1;
				do_clear = 1;
				break;
			case 'X':
				do_remote_control = 1;
				do_seek = atoi(optarg);
				break;
			case 'Z':
				do_remote_control = 1;
				do_relative = 1;
				do_seek = atoi(optarg);
				break;
			default:
				alsaplayer_error("Unknown option '%c'", opt);
				break;
		}	
	}
	
	homedir = get_homedir();

	snprintf(prefs_path, sizeof(prefs_path)-1,"%s/.alsaplayer/", homedir);
	mkdir(prefs_path, 0700);	/* XXX We don't do any error checking here */
	snprintf(prefs_path, sizeof(prefs_path)-1, "%s/.alsaplayer/config", homedir);
	if (use_config) 
		ap_prefs = prefs_load(use_config);
	else
		ap_prefs = prefs_load(prefs_path);
	if (!ap_prefs) {
		alsaplayer_error("Invalid config file %s\n", use_config ? use_config : prefs_path);
		return 1;
	}	
	/* Initialize some settings (and populate the prefs system if needed */

	if (use_fragsize < 0)
		use_fragsize = prefs_get_int(ap_prefs, "main", "period_size", 4096);
	if (use_fragcount < 0)
		use_fragcount = prefs_get_int(ap_prefs, "main", "period_count", 8);


	if (global_verbose)
		puts(copyright_string);

	if (!global_pluginroot) {
		global_pluginroot = strdup (ADDON_DIR);
	}	


	if (use_session == 0) {
		for (; use_session < MAX_REMOTE_SESSIONS+1; use_session++) {
			ap_result = ap_session_running(use_session);

			if (ap_result)
				break;
		}
		if (use_session == (MAX_REMOTE_SESSIONS+1)) {
			//alsaplayer_error("No remote session found");
			do_remote_control = 0;
			do_enqueue = 0;
		} else {
			//alsaplayer_error("Found remote session %d", use_session);
		}	
	}

	// Check if we're in remote control mode
	if (do_remote_control) {
		if (do_setvol) {
			ap_set_volume(use_session, use_vol);
			return 0;
		} else if (do_start) {
			ap_play(use_session);
			return 0;
		} else if (do_stop) {
			ap_stop(use_session);
			return 0;
		} else if (do_next) {
			ap_next(use_session);
			return 0;
		} else if (do_prev) {
			ap_prev(use_session);
			return 0;
		} else if (do_jump >= 0) {
			ap_jump_to(use_session, do_jump);
			return 0;
		} else if (do_clear) {
			ap_clear_playlist(use_session);
			return 0;
		} else if (do_relative) {
			if (do_seek != 0)
				ap_set_position_relative(use_session, do_seek);
			return 0;
		} else if (do_seek >= 0) {
			ap_set_position(use_session, do_seek);
			return 0;
		} else 	
			alsaplayer_error("No remote control command executed.");
	}

	// Check if we need to enqueue the files
	if (do_enqueue) {
		char queue_name[2048];
		int count = 0;
		int was_playing = 0;
		int playlist_length = 0;
		
		ap_result = 1;
		
		if (do_replace) {
			ap_is_playing(use_session, &was_playing);
			if (was_playing) {
				ap_stop(use_session);
			}
			ap_clear_playlist(use_session);
		} else {
			ap_get_playlist_length(use_session, &playlist_length);
			if (!playlist_length) { // Empty list so fire up after add
				was_playing = 1;
			}		
		}	
		
		count = optind;
		while (count < argc && ap_result) {
			if (is_playlist(argv[count])) {
				ap_add_playlist(use_session, argv[count]);
				count++;
				continue;
			}
			if (argv[count][0] != '/' &&
				strncmp(argv[count], "http://", 7) != 0 &&
				strncmp(argv[count], "ftp://", 6) != 0) {
				// Not absolute so append cwd
				if (getcwd(queue_name, 1024) == NULL) {
					alsaplayer_error("error getting cwd");
					return 1;
				}
				strcat(queue_name, "/");
				strncat(queue_name, argv[count], sizeof(queue_name)-strlen(queue_name));
			} else
				strncpy(queue_name, argv[count], sizeof(queue_name)-strlen(queue_name));
			count++;
			//alsaplayer_error("Adding %s", queue_name);
			ap_result = ap_add_path(use_session, queue_name);
			//alsaplayer_error("ap_result = %d", ap_result);	
		}
		if (was_playing)
			ap_jump_to(use_session, 1);
		if (ap_result)
			return 0;
	}

	AlsaNode *node;

	// Check if we want jack
	if (strcmp(argv[0], "jackplayer") == 0) {
		use_output = "jack";
	}
	
	// Check the output option
	if (use_output == NULL) {
		use_output = prefs_get_string(ap_prefs, "main",
				"default_output", "alsa");
	}

	// Else do the usual plugin based thing
	node = new AlsaNode(use_output, device_param, do_realtime);

	if (!node->RegisterPlugin(use_output)) {
		alsaplayer_error("Failed to load output plugin \"%s\". Trying defaults.", use_output);
		if (!node->RegisterPlugin())
			return 1;
	}
	int output_is_ok = 0;
	int output_alternate = 0;

	do {
		if (!node || !node->ReadyToRun()) {
			alsaplayer_error
				("failed to load output plugin (%s). exitting...",
					use_output ? use_output: "alsa,etc.");
			return 1;
		}
		if (!node->SetSamplingRate(use_freq) ||
				!node->SetStreamBuffers(use_fragsize, use_fragcount, 2)) {
			alsaplayer_error
				("failed to configure output device...trying OSS");
			/* Special case for OSS, since it's easiest to get going, so try it */
			if (!output_alternate) {
				output_alternate = 1;
				node->RegisterPlugin("oss");
				continue;
			} else {
				return 1;
			}
		}
		output_is_ok = 1;	/* output device initialized */
	} while (!output_is_ok);

	// Initialise reader
	reader_init ();

	// Initialise playlist - must be done before things try to register with it
	playlist = new Playlist(node);

	if (!playlist) {
		alsaplayer_error("Failed to create Playlist object");
		return 1;
	}
	// Add any command line arguments to the playlist
	if (optind < argc) {
		std::vector < std::string > newitems;
		while (optind < argc) {
#if 0
			if (strcmp(argv[optind], "-") == 0) {
				// stream from stdin
				// FIXME: implement
				alsaplayer_error("warning: streaming from stdin (experimental)");
				newitems.push_back("-");
				break;
			} else {
#else
			{
#endif
				char *ext;
				ext = strrchr(argv[optind], '.');
				if (ext && (strncasecmp(++ext, "m3u", 3) == 0 ||
						strncasecmp(ext, "pls", 3) == 0)) {
					playlist->Load(std::string(argv[optind++]),
						playlist->Length(), false);
				} else {	
					newitems.push_back(std::string(argv[optind++]));
				}	
			}
		}
		playlist->Insert(newitems, playlist->Length());
	} else {
		homedir = get_homedir();
		snprintf(prefs_path, sizeof(prefs_path)-1, "%s/.alsaplayer/alsaplayer.m3u", homedir);
		playlist->Load(prefs_path, playlist->Length(), false);
	}
	playlist->UnPause();
		
	// Loop song
	if (do_loopsong) {
		playlist->LoopSong();
	}
	// Loop Playlist
	if (do_looplist) {
		playlist->LoopPlaylist();
	}
	// Cross fading
	if (do_crossfade) {
		playlist->Crossfade();
	}
	// Set start volume
	playlist->GetCorePlayer()->SetVolume(start_vol);

	
	interface_plugin_info_type interface_plugin_info;
	interface_plugin *ui;

	if (get_interface_from_argv0 (argv[0], str))
		use_interface = str;
	
	if (use_interface && *use_interface) {
		if (!(interface_plugin_info = load_interface(use_interface))) {
			alsaplayer_error("Failed to load interface %s\n", use_interface);
			goto _fatal_err;
		}
	} else {
		if (!(interface_plugin_info = load_interface(prefs_get_string
						(ap_prefs, "main", "default_interface", "gtk")))) {
			if (!(interface_plugin_info = load_interface(prefs_get_string
						 (ap_prefs, "main", "fallback_interface", "text")))) {
				alsaplayer_error("Failed to load text interface. This is bad (%s,%s,%s)",
					 prefs_get_string(ap_prefs, "main", "default_interface", "gtk"),
					 prefs_get_string(ap_prefs, "main", "default_interface", "gtk"),
					global_pluginroot);
				goto _fatal_err;
			}
		}
	}
	if (interface_plugin_info) {
		ui = interface_plugin_info();

		if (global_verbose)
			printf("Interface plugin: %s\n", ui->name);
		if (!ui->init()) {
			alsaplayer_error("Failed to load interface plugin. Should fall back to text\n");
		} else {
			control_socket_start(playlist, ui);
			ui->start(playlist, argc, argv);
			ui->close();
			// Unfortunately gtk+ is a pig when it comes to
			// cleaning up its resources; it doesn't!
			// so we can never safely dlclose gtk+ based 
			// user interfaces, bah!
			//dlclose(ui->handle);
			control_socket_stop();
		}	
	}
	// Save playlist before exit
	homedir = get_homedir();
	snprintf(prefs_path, sizeof(prefs_path)-1, "%s/.alsaplayer/alsaplayer", homedir);
	playlist->Save(prefs_path, PL_FORMAT_M3U);

	// Save preferences
	if (ap_prefs && do_save) {
		if (prefs_save(ap_prefs) < 0) {
			alsaplayer_error("failed to save preferences.");
		}
	}	

_fatal_err:
	delete playlist;
	//delete p;
	delete node;
	if (global_session_name)
		free(global_session_name);
	if (global_pluginroot)
		free(global_pluginroot);

	return 0;
}
