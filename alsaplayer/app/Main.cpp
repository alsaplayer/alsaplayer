/*  Main.cpp - main() function and other utils
 *  Copyright (C) 1998-2001 Andy Lo A Foe <andy@alsa-project.org>
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

#include "config.h"

#include "SampleBuffer.h"
#include "CorePlayer.h"
#include "Playlist.h"
#include "Effects.h"
#include "input_plugin.h"
#include "output_plugin.h"
#include "interface_plugin.h"
#include "utilities.h"

Playlist * playlist = NULL;

int global_reverb_on = 0;
int global_reverb_delay = 2;
int global_reverb_feedback = 0;

static char addon_dir[1024];

static char *default_pcm_device = "hw:0,0";

const char *default_output_addons[] = {
	{ "libalsa.so" },
	{ "libnas.so" },
	{ "liboss.so" },
	{ "libsparc.so" },
	{ "libesound.so" },
	{ "libsgi.so" },
	{ "libnull.so" },
	NULL };



void exit_sighandler(int x)
{
	static int sigcount = 0;

	++sigcount;
	if (sigcount == 1)
		fprintf(stderr, "\nAlsaPlayer interrupted by signal %d\n", x);
	if (sigcount == 1) 
		exit(1);
	if (sigcount > 5){
		kill(getpid(), SIGKILL);
	}	
}

void load_output_addons(AlsaNode *node, char *module = NULL)
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
		if (stat(path, &statbuf) != 0) // Error reading object
			return;		
		if ((handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))) {
			output_plugin_info = (output_plugin_info_type) dlsym(handle, "output_plugin_info");
			if (output_plugin_info) {
				node->RegisterPlugin(output_plugin_info());
				return;
			} else {
				fprintf(stderr, "symbol error in shared object: %s\n", path);
				fprintf(stderr, "Try starting up with \"-o\" to load a\n"
								"specific output module (alsa, oss, esd, etc.)\n");
				dlclose(handle);
				return;
			}
		} else {
			printf("%s\n", dlerror());
		}
	}

	else
	
	for (int i=0; default_output_addons[i]; i++) {
		sprintf(path, "%s/output/%s", addon_dir, default_output_addons[i]);
		if (stat(path, &statbuf) != 0)
			continue;
		if ((handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))) {
			output_plugin_info = (output_plugin_info_type) dlsym(handle, "output_plugin_info");
			if (output_plugin_info) {
#ifdef DEBUG
				printf("Loading output plugin: %s\n", path);	
#endif
				if (node->RegisterPlugin(output_plugin_info())) {
					if (!node->ReadyToRun()) {
						fprintf(stderr, "%s failed to init\n", path);
						continue; // This is not clean
					}
					return; // Return as soon as we load one successfully!
				} else {
					fprintf(stderr, "%s failed to load\n", path);
				}
			} else {
				printf("could not find symbol in shared object\n");
				dlclose(handle);
			}
		} else {
			printf("%s\n", dlerror());
		}
	}
	// If we arrive here it means we haven't found any suitable output-addons
	fprintf(stderr, "ERROR: I could not find a suitable output module on your\n"
			"       system. Make sure they're in \"%s/output/\".\n"
			"       Use the -o paræmeter to select one.\n", addon_dir);
}


interface_plugin_info_type load_interface(char *name)
{
	void *handle;
	char path[1024];
	struct stat statbuf;

	interface_plugin_info_type plugin_info;

	if (name) {
		sprintf(path, "%s/interface/lib%s.so", addon_dir, name);
#ifdef DEBUG
		printf("Loading output plugin: %s\n", path);
#endif
		if (stat(path, &statbuf) != 0) // Error reading object
			return NULL;		
		if ((handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL))) {
			plugin_info = (interface_plugin_info_type) dlsym(handle, "interface_plugin_info");
			if (plugin_info) {
				interface_plugin *plugin = plugin_info();
				if (plugin)
					plugin->handle = handle;
				return plugin_info;
			} else {
				fprintf(stderr, "symbol error in shared object: %s\n", path);
				dlclose(handle);
				return NULL;
			}
		} else {
			printf("%s\n", dlerror());
		}
	}
	return NULL;
}



extern int init_reverb();
extern bool reverb_func(void *arg, void *data, int size);

static char *copyright_string = "AlsaPlayer "VERSION"\n(C) 1999-2001 Andy Lo A Foe <andy@alsaplayer.org> and others.";

static void help()
{
	fprintf(stdout,
"\n"
"Usage: alsaplayer [options] [filename <filename> ...]\n"
"\n"	
"Available options:\n"
"\n"
"  -d,--device string          select card and device [default=hw:0,0]\n"
"  -f,--fragsize #             fragment size in bytes [default=4096]\n"
"  -F,--frequency #            output frequency [default=%d]\n"
"  -g,--fragcount #            fragment count [default=8]\n"
"  -h,--help                   print this help message\n"
"  -i,--interface iface        load in the iface interface [default=gtk]\n"
"  -l,--volume #               set software volume [0-100]\n"
"  -p,--path [path]            print/set the path alsaplayer looks for add-ons\n" 
"  -q,--quiet                  quiet operation. no output\n"
"  -r,--realtime               enable realtime scheduling (must be SUID root)\n"
"  -v,--version                print version of this program\n"
"\n"
"Testing options:\n"
"\n"
"  --reverb      	                 use reverb function (CPU intensive!)\n"
"  -S, --loopsong                        Loop file\n"
"  -P, --looplist                        Loop Playlist\n"
"  -o,--output [alsa|oss|nas|sgi|sparc]  Use ALSA, OSS, NAS, SGI or Sparc driver for output\n"
"\n", OUTPUT_RATE
	);
}

static void version()
{
	fprintf(stdout,
"%s version %s\n\n", PACKAGE, VERSION); 	
}


int main(int argc, char **argv)
{
	char *use_pcm = default_pcm_device;
	int use_fragsize = 4096;
	int use_fragcount = 8;
	int use_reverb = 0; // TEMP!
	int use_loopSong = 0; 
	int use_loopList = 0;
	int be_quiet = 0;
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
	CorePlayer *p;
	//char path[256], *home;
	char use_interface[256];

	// First setup signal handler
	signal(SIGTERM,exit_sighandler); // kill
	signal(SIGHUP,exit_sighandler);  // kill -HUP / xterm closed
	signal(SIGINT,exit_sighandler);  // Interrupt from keyboard
	signal(SIGQUIT,exit_sighandler); // Quit from keyboard
	// fatal errors
	signal(SIGBUS,exit_sighandler);  // bus error
	signal(SIGSEGV,exit_sighandler); // segfault
	signal(SIGILL,exit_sighandler);  // illegal instruction
	signal(SIGFPE,exit_sighandler);  // floating point exc.
	signal(SIGABRT,exit_sighandler); // abort()
	
	strcpy(addon_dir, ADDON_DIR);

	init_effects();

	memset(use_interface, 0, sizeof(use_interface));

	for (arg_pos=1; arg_pos < argc; arg_pos++) {
		if (strcmp(argv[arg_pos], "--help") == 0 ||
			strcmp(argv[arg_pos], "-h") == 0) {
			help();
			return 0;
		} else if (strcmp(argv[arg_pos], "--interface") == 0 ||
				strcmp(argv[arg_pos], "-i") == 0) {
				if (argc <= arg_pos+1) {
						fprintf(stderr, "argument expected for %s\n",
										argv[arg_pos]);
						return 1;
				}
				strcpy(use_interface, argv[++arg_pos]);
				last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--volume") == 0 ||
				   strcmp(argv[arg_pos], "-l") == 0) {
			int tmp = -1;	   
			if (sscanf(argv[++arg_pos], "%d", &tmp) != 1 ||
				(tmp < 0 || tmp > 100)) {
				fprintf(stderr, "invalid value for volume: %d\n", tmp);
				return 1;
			}
			use_vol = tmp;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--version") == 0 ||
				   strcmp(argv[arg_pos], "-v") == 0) {
			version();
			return 0;
		} else if (strcmp(argv[arg_pos], "--path") == 0 ||
				   strcmp(argv[arg_pos], "-p") == 0) {
			if (arg_pos+1 == argc) { // Last option so display and exit
				fprintf(stdout,"Input  add-on path: %s/input/\n", addon_dir);
				fprintf(stdout,"Output add-on path: %s/output/\n", addon_dir);
				fprintf(stdout,"Scope  add-on path: %s/scopes/\n", addon_dir);
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
			if (argc <= arg_pos+1) {
				fprintf(stderr, "argument expected for %s\n",
					argv[arg_pos]);
				return 1;
			}
			use_alsa = use_sparc = use_esd = use_oss = 0;
			if (strcmp(argv[arg_pos+1], "alsa") == 0) {
				//printf("using ALSA output method\n");
				use_alsa = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos+1], "sgi") == 0) {
				use_sgi = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos+1], "oss") == 0) {
				//printf("Using OSS output method\n");
				use_oss = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos+1], "null") == 0) {
				use_null = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos+1], "esd") == 0) {
				//printf("Using ESD output method\n");
				use_esd = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos+1], "sparc") == 0) {
				use_sparc = 1;
				last_arg = ++arg_pos;
			} else if (strcmp(argv[arg_pos+1], "nas") == 0) {
				use_nas = 1;
				last_arg = ++arg_pos;
			} else {
				fprintf(stderr, "Unsupported output module: %s\n",
					argv[arg_pos+1]);
				return 1;
			}
			use_user = 1;
		} else if (strcmp(argv[arg_pos], "--quiet") == 0 ||
				strcmp(argv[arg_pos], "-q") == 0) {
			be_quiet = 1;
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--frequency") == 0 ||
					strcmp(argv[arg_pos], "-F") == 0) {
			if (sscanf(argv[++arg_pos], "%d", &use_freq) != 1) {
				fprintf(stderr, "numeric argument expected for %s\n",
					argv[arg_pos-1]);
				return 1;
			}
			if (use_freq < 8000 || use_freq > 48000) {
				fprintf(stderr, "frequency out of range [8000-48000]\n");
				return 1;
			}		
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--fragsize") == 0 ||
				   strcmp(argv[arg_pos], "-f") == 0) {
			if (sscanf(argv[++arg_pos], "%d", &use_fragsize) != 1) {
				fprintf(stderr, "numeric argument expected for %s\n",
					argv[arg_pos-1]);
				return 1;
			}
			if (!use_fragsize || (use_fragsize % 32)) {
				fprintf(stderr, "invalid fragment size, must be multiple of 32\n");
				return 1;
			}
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--fragcount") == 0 ||
				   strcmp(argv[arg_pos], "-g") == 0) {
			if (sscanf(argv[++arg_pos], "%d", &use_fragcount) != 1) {
				fprintf(stderr, "numeric argument expected for --fragcount\n");
				return 1;
			}
			if (!use_fragcount) {
				fprintf(stderr, "fragment count cannot be 0\n");
				return 1;
			}
			last_arg = arg_pos;
		} else if (strcmp(argv[arg_pos], "--device") == 0 ||
		           strcmp(argv[arg_pos], "-d") == 0) {
			use_pcm = argv[++arg_pos];
			last_arg = arg_pos;
		}
	}	

	if (!be_quiet)
		fprintf(stdout, "%s\n", copyright_string);	

	// Connect to an output system
	AlsaNode *node = new AlsaNode(use_pcm, use_realtime);

	if (!node) {
		printf("Bwah!!\n");
		return 1;
	}

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
	} else
		load_output_addons(node);

	if (!node->ReadyToRun()) {
		fprintf(stderr, "ERROR: failed to load output add-on. Exiting...\n");
		return 1;
	}	
	if (!node->SetSamplingRate(use_freq)) {
		fprintf(stderr, "ERROR: failed to set sampling frequency. Exiting...\n");
		return 1;
	}
	if (!node->SetStreamBuffers(use_fragsize, use_fragcount, 2)) {
		fprintf(stderr, "ERROR: failed to set fragment size/count. Exiting...\n");
		return 1;
	}	
	
	p = new CorePlayer(node);

	if (!p)  {
		fprintf(stderr, "ERROR: failed to create CorePlayer object...\n");
		return 1;
	}

	// Reverb effect
	AlsaSubscriber *reverb = NULL;
	if (use_reverb) {
		init_reverb();
		reverb = new AlsaSubscriber();
		reverb->Subscribe(node);
		reverb->EnterStream(reverb_func, p);
	}

	// Initialise playlist - must be done before things try to register with it
	playlist = new Playlist(p);

	if (!playlist) {
		fprintf(stderr, "ERROR: Failed to create Playlist object\n");
		return 1;
	}	
	// Add any command line arguments to the playlist
	if (++last_arg < argc) {
		std::vector<std::string> newitems;
		while (last_arg < argc) {
			newitems.push_back(std::string(argv[last_arg++]));
		}
		playlist->Insert(newitems, playlist->Length());
	}

	// Loop song
	if (use_loopSong) {	 
	 playlist->LoopSong();
	}

	// Loop Playlist
	if (use_loopList) {	 
	 playlist->LoopPlaylist();
	}

	interface_plugin_info_type interface_plugin_info;
	interface_plugin *ui;

	if (strlen(use_interface)) {
		if (!(interface_plugin_info = load_interface(use_interface))) {
			printf("Failed to load interface %s\n", use_interface);
			goto _fatal_err;
		}	
	} else {
		if (!(interface_plugin_info = load_interface("gtk"))) {
			if (!(interface_plugin_info = load_interface("cli"))) {
				printf("Failed to load cli interface. This is bad\n");
				goto _fatal_err;
			}
		}	
	}	
	if (interface_plugin_info) {
		ui = interface_plugin_info();
		printf("Loading Interface plugin: %s\n", ui->name); 
		ui->init();
		ui->start(p, playlist, argc, argv);
		ui->close();
		//dlclose(ui->handle);
	}	
_fatal_err:	
	delete playlist;
	delete p;
	delete node;
	return 0;	
}

// Last Edited: <27 Aug 2001 19:21:40 (Andy)>
