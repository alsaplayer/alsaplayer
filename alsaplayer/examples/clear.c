#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	if (ap_do (0, AP_DO_CLEAR_PLAYLIST)) {
		printf("Cleared playlist\n");
	}	
  return 0;
}
