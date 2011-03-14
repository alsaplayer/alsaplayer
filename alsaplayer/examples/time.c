#include <stdio.h>
#include <unistd.h>
#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	int pos_block, pos_sec;
	int length_block, length_sec;

	puts("");
	while (ap_session_running(0)) {
		// Totals
		ap_get_blocks(0, &length_block);
		ap_get_length(0, &length_sec);

		// Current position
		ap_get_block(0, &pos_block);
		ap_get_position(0, &pos_sec);

		printf("Run: %5d / %5d = %.2f <-> %5d / %5d = %.2f   \r",
			pos_block, length_block,
			(float)pos_block / (float)length_block,
			pos_sec, length_sec,
			(float)pos_sec / (float)length_sec);
		fflush(stdout);
		usleep(50000);
	}
	puts("done");
	return 0;
}
