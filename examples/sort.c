#include <stdio.h>
#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	if (argc == 2) {
	  ap_sort (0, argv[1]);
	}	
	return 0;
}
