/*
	ap-control

	Author: Frank Baumgart, frank.baumgart@gmx.net
	Andy did the "list playlist" feature
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <control.h>


static void usage(void)
{
	puts("usage: ap-control command\n\n"
	"supported commands:\n\n"
	"play <title> [<title> ...]\n"
	"playlist [<playlistfile>]\n"
	"playlist-clear\n"
	"save\n"
	"shuffle\n"
	"sort <direction>\n"
	"stop\n"
	"prev\n"
	"next\n"
	"pause\n"
	"cont\n"
	"seek second\n"
	"query\n"
	"title\n"
	"time\n"
	"position\n"
	"loop-on\n"
	"loop-off\n"
	"quit");

	exit(1);
}


int main(int argc, char *argv[])
{
	int items = 0, c;
	char **playlist;
	
	if (argc < 2)
		usage();

	if (!strcmp(argv[1], "play") && argc >= 3)
	{
		int i;
		for (i = 2; i < argc; i++)
			ap_add_path(0, argv[i]);

		return 0;
	}

	if (!strcmp(argv[1], "playlist"))
	{
		int ret = 1;
		if (argc >= 3) {
			ret &= ap_clear_playlist(0);
			ret &= ap_add_playlist(0, argv[2]);

			return ret == 1;
		}
		if (ap_get_playlist(0, &items, &playlist)) {
			// printf("Found %d items on playlist\n", items);
			if (items) {
				for (c = 0; c < items; c++) {
					printf("%d. %s\n", c+1, playlist[c]);
				}
				// Memory cleanup
				for (c = 0; c < items; c++) {
					free(playlist[c]);
				}
				free(playlist);
			}
		}	
		return 0;
	}

	if (!strcmp(argv[1], "playlist-clear"))
		return ap_clear_playlist(0) == 1;

#if 0
	if (!strcmp(argv[1], "playlist-save") && argc == 3)
		return ap_save_playlist(0, argv[2]) == 1;
#endif

	if (!strcmp(argv[1], "sort") && argc == 3)
		return ap_sort(0, argv[2]) == 1;

	if (!strcmp(argv[1], "seek") && argc == 3)
		return ap_set_position(0, atoi(argv[2])) == 1;

	if (!strcmp(argv[1], "stop"))
		return (ap_clear_playlist(0) && ap_stop(0));

	if (!strcmp(argv[1], "prev"))
		return ap_prev(0) == 1;

	if (!strcmp(argv[1], "next"))
		return ap_next(0) == 1;

	if (!strcmp(argv[1], "pause"))
		return ap_pause(0) == 1;

	if (!strcmp(argv[1], "cont"))
		return ap_unpause(0) == 1;

	if (!strcmp(argv[1], "quit"))
		return ap_quit(0) == 1;

	if (!strcmp(argv[1], "loop-on"))
		return ap_set_playlist_looping(0, 1) == 1;

	if (!strcmp(argv[1], "loop-off"))
		return ap_set_playlist_looping(0, 0) == 1;

	if (!strcmp(argv[1], "shuffle"))
		return ap_shuffle_playlist(0) == 1;

	if (!strcmp(argv[1], "save"))
		return ap_save_playlist(0) == 1;

	if (!strcmp(argv[1], "query"))
	{
		int playing;

		if (ap_is_playing(0, &playing))
		{
			puts(playing ? "playing" : "not playing");
			return 0;
		}
		return 1;
	}

	if (!strcmp(argv[1], "position"))
	{
		int result;

		if (ap_get_playlist_position(0, &result))
		{
			printf("%d\n", result);
			return 0;
		}
		return 1;
	}

	if (!strcmp(argv[1], "title"))
	{
		char result[AP_TITLE_MAX];

		if (ap_get_title(0, result) && *result)
		{
			puts(result);
			return 0;
		}
		return 1;
	}

	if (!strcmp(argv[1], "time"))
	{
		int pos_frame, pos_sec;
		int length_frame, length_sec;

		if (!ap_session_running(0))
			return 1;

		// Totals
		ap_get_frames(0, &length_frame);
		ap_get_length(0, &length_sec);

		// Current position
		ap_get_frame(0, &pos_frame);
		ap_get_position(0, &pos_sec);

		printf("Frame: %d/%d\n", pos_frame, length_frame);
		printf("Seconds: %d/%d\n", pos_sec, length_sec);

		return 0;
	}

	usage();

	return 0;
}
