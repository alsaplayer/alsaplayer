#include <alsaplayer/control.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
	int i;
	char *artist = NULL;
	char *title = NULL;
	float speed = 0.0;
	float *qspeed = NULL;
	int session_id = 0;
	

	artist = ap_get_artist(session_id);
	title = ap_get_title(session_id);

	if (artist && title) {
		printf("File playing: %s - %s\n", artist, title);
	}
	if (artist)
		free(artist);
	if (title)
		free(title);
	
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
	qspeed = ap_get_speed(session_id);

	if (qspeed) {
		printf ("Current speed = %.2f\n", *qspeed);
		free(qspeed);
	}	
	return 0;
}
