
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"

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
	FILE *fp;

	strcpy(cfg_path, DEFAULT_PATH);
	strcat(cfg_path, "/PC42b.sf2");

	if ( (fp=fopen(cfg_path, "r")) ) {
		fclose(fp);
		current_config_pc42b = 1;
		add_to_pathlist(DEFAULT_PATH, 0);
		return 1;
	}

	if (!(homedir = getenv("HOME"))) return 0;
	sprintf(prefs_path, "%s/.alsaplayer", homedir);

	chdir(prefs_path);

	if ( (fp=fopen("PC42b.sf2", "r")) ) {
		fclose(fp);
		current_config_pc42b = 1;
		add_to_pathlist(prefs_path, 0);
		return 1;
	}

	strcpy(cfg_path, prefs_path);
	strcat(cfg_path,"/timidity.cfg");

	if ( (fp=fopen(cfg_path, "r")) ) {
		haveone = 1;
		fclose(fp);
	}

	if (!haveone) {

        	n = scandir(prefs_path, &namelist, choose, 0);
        	if (n < 0) return 0;

       		while(n--) {
			if (!haveone) haveone = sf2cfg(namelist[n]->d_name, "timidity.cfg");
        		free(namelist[n]);
       		}
		
		free(namelist);
	}

	if (haveone) {
		current_config_file = strdup(cfg_path);
		add_to_pathlist(prefs_path, 0);
	}
	return haveone;
}
