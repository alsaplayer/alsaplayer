#include <alsaplayer/control.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
	int i;
	char str[1024];
	float val = 0.0;

	if (ap_get_string(0, AP_GET_STRING_SONG_NAME, str) != -1) {
		printf("File playing: %s\n", str);
	}
	if (argc == 2) {
		if (sscanf (argv[1], "%f", &val) == 1) {
			if (val == 0.0) {
				printf ("Pausing player\n");
				ap_do (0, AP_DO_PAUSE);
			} else {
				float tester;
				printf ("Setting speed to %.2f\n", val);
				ap_set_float (0, AP_SET_FLOAT_SPEED, val);
			}
		}
	}
	if (ap_get_float(0, AP_GET_FLOAT_SPEED, &val) != -1) {
		printf ("Current speed = %.2f\n", val);
	}	
	return 0;
}
