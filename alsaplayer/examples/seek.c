#include <alsaplayer/control.h>

int main (int argc, char *argv[])
{
	int pos;
	if (argc == 2) {
		pos = atoi(argv[1]);

		if (ap_set_int (0, AP_SET_INT_POS_SECOND, pos) != -1) {
			printf("Seeked to second %d\n", pos);
		}
	}	
  return 0;
}
