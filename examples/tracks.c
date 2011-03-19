#include <stdio.h>
#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	int tracks = 0;  
	if (ap_get_tracks (0, &tracks))
		printf("tracks = %d\n", tracks);
	return 0;
}
