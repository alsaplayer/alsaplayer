/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    controls.h
*/

/* Return values for ControlMode.read */

#define RC_ERROR -1
#define RC_NONE 0
#define RC_QUIT 1
#define RC_NEXT 2
#define RC_PREVIOUS 3 /* Restart this song at beginning, or the previous
			 song if we're less than a second into this one. */
#define RC_FORWARD 4
#define RC_BACK 5
#define RC_JUMP 6
#define RC_TOGGLE_PAUSE 7 /* Pause/continue */
#define RC_RESTART 8 /* Restart song at beginning */

#define RC_PAUSE 9 /* Really pause playing */
#define RC_CONTINUE 10 /* Continue if paused */
#define RC_REALLY_PREVIOUS 11 /* Really go to the previous song */
#define RC_CHANGE_VOLUME 12
#define RC_LOAD_FILE 13		/* Load a new midifile */
#define RC_TUNE_END 14		/* The tune is over, play it again sam? */
#define RC_TRY_OPEN_DEVICE 15
#define RC_PATCHCHANGE 16
#define RC_CHANGE_VOICES 17
#define RC_STOP 18

#define CMSG_INFO	0
#define CMSG_WARNING	1
#define CMSG_ERROR	2
#define CMSG_FATAL	3
#define CMSG_TRACE	4
#define CMSG_TIME	5
#define CMSG_TOTAL	6
#define CMSG_FILE	7
#define CMSG_TEXT	8
#define CMSG_LYRIC	9

#define VERB_ALWAYS	-1
#define VERB_NORMAL	0
#define VERB_VERBOSE	1
#define VERB_NOISY	2
#define VERB_DEBUG	3
#define VERB_DEBUG_SILLY	4

/* 
 * MESSAGE FROM KERNEL TO MOTIF
 */
#define REFRESH_MESSAGE 1
#define HELPMODE_MESSAGE 2
#define TOTALTIME_MESSAGE 3
#define MASTERVOL_MESSAGE 4
#define FILENAME_MESSAGE 5
#define CURTIME_MESSAGE 6
#define NOTE_MESSAGE 7
#define PROGRAM_MESSAGE 8
#define VOLUME_MESSAGE 9
#define EXPRESSION_MESSAGE 10
#define PANNING_MESSAGE 11
#define SUSTAIN_MESSAGE 12
#define PITCH_MESSAGE 13
#define RESET_MESSAGE 14
#define CLOSE_MESSAGE 15
#define CMSG_MESSAGE 16
#define FILE_LIST_MESSAGE 17
#define NEXT_FILE_MESSAGE 18
#define PREV_FILE_MESSAGE 19
#define TUNE_END_MESSAGE 20
#define DEVICE_OPEN 21
#define DEVICE_NOT_OPEN 22
#define PATCH_CHANGED_MESSAGE 23
#define JUMP_MESSAGE 24
/* 
 * MESSAGE ON THE PIPE FROM MOTIF TOWARD KERNEL
 */
#define MOTIF_CHANGE_VOLUME 1
#define MOTIF_CHANGE_LOCATOR 2
#define MOTIF_QUIT 3
#define MOTIF_PLAY_FILE 4
#define MOTIF_NEXT 5
#define MOTIF_PREV 6
#define MOTIF_RESTART 7
#define MOTIF_FWD 8
#define MOTIF_RWD 9
#define MOTIF_PAUSE 10
#define TRY_OPEN_DEVICE 11
#define MOTIF_PATCHSET 12
#define MOTIF_EFFECTS 13
#define MOTIF_CHANGE_VOICES 14
#define MOTIF_CHECK_STATE 15
#define MOTIF_FILTER 16
#define MOTIF_INTERPOLATION 17
#define MOTIF_REVERB 18
#define MOTIF_CHORUS 19
#define MOTIF_DRY 20
#define MOTIF_STOP 21
#define MOTIF_EVS 22

/* from TiMidity++, but not implemented */
#define MOTIF_KEYUP 23
#define MOTIF_KEYDOWN 24
#define MOTIF_SLOWER 25
#define MOTIF_FASTER 26
#define MOTIF_TOGGLE_DRUMS 27


typedef struct {
  const char *id_name;
  char id_character;
  int verbosity, trace_playing, opened;

  int (*open)(int using_stdin, int using_stdout);
  void (*pass_playing_list)(int number_of_files, const char *list_of_files[]);
  void (*close)(void);
  int (*read)(int32 *valp);
  int (*cmsg)(int type, int verbosity_level, const char *fmt, ...);

  void (*refresh)(void);
  void (*reset)(void);
  void (*file_name)(char *name);
  void (*total_time)(uint32 tt);
  void (*current_time)(uint32 ct);

  void (*note)(int v);
  void (*master_volume)(int mv);
  void (*program)(int channel, int val, const char *name);
  void (*volume)(int channel, int val);
  void (*expression)(int channel, int val);
  void (*panning)(int channel, int val);
  void (*sustain)(int channel, int val);
  void (*pitch_bend)(int channel, int val);
  
} ControlMode;

extern int last_rc_command;
extern int last_rc_arg;
extern ControlMode *ctl_list[], *ctl; 
extern int check_for_rc(void);
