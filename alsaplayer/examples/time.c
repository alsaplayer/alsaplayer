#include <stdio.h>
#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	int pos_frame, pos_sec;
	int length_frame, length_sec;

	if (ap_version() != AP_CONTROL_VERSION) {
		printf("protocol version mismatch. please recompile this program\n");
		return 1;
	}	
	printf("\n");
	while (ap_session_running(0)) {
		ap_get_int(0, AP_GET_INT_SONG_LENGTH_FRAME, &length_frame);
		ap_get_int(0, AP_GET_INT_SONG_LENGTH_SECOND, &length_sec);
		ap_get_int(0, AP_GET_INT_POS_FRAME, &pos_frame);
		ap_get_int(0, AP_GET_INT_POS_SECOND, &pos_sec);
		fprintf(stdout, "Run: %5d / %5d = %.2f <-> %5d / %5d = %.2f\n",
			pos_frame, length_frame, 
			(float)pos_frame / (float)length_frame,
			pos_sec, length_sec,
			(float)pos_sec / (float)length_sec);
		fflush(stdout);
		sleep(1);	
	}
	fprintf(stdout, "done\n");
	return 0;
}
