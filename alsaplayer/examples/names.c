#include <stdio.h>
#include <unistd.h>
#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	char name[1024];
	int i;

	for (i=0; i < 32; i++) {
		if (ap_session_running(i)) {
			ap_get_session_name(i, name);
			printf("Found session %d: \"%s\"\n", i, name);
		}
	}
	return 0;
}
