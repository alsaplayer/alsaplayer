#include <alsaplayer/control.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
	int i;
	char artist[AP_ARTIST_MAX];
	char title[AP_TITLE_MAX];
	float speed = 0.0;
	int session_id = 0;

	if (ap_get_title(session_id, title) &&
	    ap_get_artist(session_id, artist)) {
		printf("File playing: %s - %s\n", title, artist);
	}
	
	if (argc == 2) {
		if (sscanf (argv[1], "%f", &speed) == 1) {
			if (speed == 0.0) {
				printf ("Pausing player\n");
				ap_pause(session_id);
			} else {
				printf ("Setting speed to %.2f\n", speed);
				ap_set_speed(session_id, speed);
			}
		}
	}
	if (ap_get_speed(session_id, &speed)) {
		printf ("Current speed = %.2f\n", speed);
	}	
	return 0;
}
