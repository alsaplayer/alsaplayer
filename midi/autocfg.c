
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>


static int choose(const struct dirent *nm) {
	char *fname = nm->d_name;
	int len = strlen(fname);
	if (len < 5) return 0;
	if (!strcmp( fname + len - 4, ".sf2" )) return 1;
	if (!strcmp( fname + len - 4, ".SF2" )) return 1;
	return 0;
}

void autocfg() {
	struct dirent **namelist;
	int n, haveone;
	char *homedir;
	char prefs_path[1024];

	if (!(homedir = getenv("HOME"))) return;
	sprintf(prefs_path, "%s/.alsaplayer", homedir);

        n = scandir(prefs_path, &namelist, choose, 0);

        if (n < 0) return;

	chdir(prefs_path);
	haveone = 0;
       	while(n--) {
		if (!haveone) haveone = sf2cfg(namelist[n]->d_name, "timidity.cfg");
        	free(namelist[n]);
       	}
	free(namelist);
}
