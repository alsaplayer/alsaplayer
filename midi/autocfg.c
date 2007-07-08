/* PC42b.h 
 *
 * Copyright (C) 2002 Greg Lee <greg@ling.lll.hawaii.edu>
 *
 *  This file is part of the MIDI input plugin for AlsaPlayer.
 *
 *  The MIDI input plugin for AlsaPlayer is free software;
 *  you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software
 *  Foundation; either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  The MIDI input plugin for AlsaPlayer is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *	$Id$   Greg Lee
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"

extern int sf2cfg(char *sffname, char *outname);

static int choose(const struct dirent *nm) {
	char *fname = nm->d_name;
	int len = strlen(fname);
	if (len < 5) return 0;
	if (!strcmp( fname + len - 4, ".sf2" )) return 1;
	if (!strcmp( fname + len - 4, ".SF2" )) return 1;
	return 0;
}

int autocfg() {
	struct dirent **namelist;
	int n, haveone = 0;
	char *homedir;
	char prefs_path[1024];
	char cfg_path[1024];
	char curr_path[1024];
	FILE *fp;

	if (!(homedir = getenv("HOME"))) return 0;
	sprintf(prefs_path, "%s/.alsaplayer", homedir);

	strcpy(cfg_path, prefs_path);
	strcat(cfg_path,"/timidity.cfg");

	if ( (fp=fopen(cfg_path, "r")) ) {
		haveone = 1;
		fclose(fp);
	}

	if (!haveone) {
		strcpy(cfg_path, prefs_path);
		strcat(cfg_path,"/PC42b.sf2");
		if ( (fp=fopen(cfg_path, "r")) ) {
			fclose(fp);
			current_config_pc42b = 1;
			add_to_pathlist(prefs_path, 0);
			return 1;
		}
	}

	if (!haveone) {
		if (!getcwd(curr_path, 1024)) return 0;

		chdir(prefs_path);

        	n = scandir(prefs_path, &namelist, choose, 0);
        	if (n < 0) return 0;

       		while(n--) {
			if (!haveone) haveone = sf2cfg(namelist[n]->d_name, "timidity.cfg");
        		free(namelist[n]);
       		}
		
		free(namelist);
		chdir(curr_path);
	}

	if (haveone) {
		current_config_file = strdup(cfg_path);
		add_to_pathlist(prefs_path, 0);
	}

#ifdef DEFAULT_PATH
	if (!haveone) {
		strcpy(cfg_path, DEFAULT_PATH);
		strcat(cfg_path,"PC42b.sf2");
		if ( (fp=fopen("PC42b.sf2", "r")) ) {
			fclose(fp);
			current_config_pc42b = 1;
			add_to_pathlist(DEFAULT_PATH, 0);
			return 1;
		}
	}
#endif

	return haveone;
}
