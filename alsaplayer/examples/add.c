#include <stdio.h>
#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	if (argc == 2) {
	  printf("Adding: %s\n", argv[1]);
	  ap_add_path(0, argv[1]);
	}	
	return 0;
}
