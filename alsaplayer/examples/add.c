#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
  if (argc == 2) {
   	printf("Adding: %s\n", argv[1]);
		ap_set_string (0, AP_SET_STRING_ADD_FILE, argv[1]);
	}	
  return 0;
}
