#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	if (ap_clear_playlist(0)) {
		printf("Cleared playlist\n");
	}	
  return 0;
}
