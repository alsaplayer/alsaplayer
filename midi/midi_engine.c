
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

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "effects.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "version.h"

#include <sys/stat.h>

#include "input_plugin.h"

/*#define PLUGDEBUG*/
/************************************/

int free_instruments_afterwards=0;
/*int free_instruments_afterwards=1;*/
static char def_instr_name[256]="";
int cfg_select = 0;
#ifdef CHANNEL_EFFECT
extern void effect_activate( int iSwitch ) ;
extern int init_effect(void) ;
#endif /*CHANNEL_EFFECT*/

int have_commandline_midis = 0;
int32 tmpi32, output_rate=0;
char *output_name=0;

#if 0
static int set_channel_flag(int32 *flags, int32 i, const char *name)
{
  if (i==0) *flags=0;
  else if ((i<1 || i>16) && (i<-16 || i>-1))
    {
		fprintf(stderr,
	      "%s must be between 1 and 16, or between -1 and -16, or 0\n", 
	      name);
      return -1;
	 }
  else
    {
      if (i>0) *flags |= (1<<(i-1));
		else *flags &= ~ ((1<<(-1-i)));
    }
  return 0;
}

static int set_value(int32 *param, int32 i, int32 low, int32 high, const char *name)
{
  if (i<low || i > high)
    {
      fprintf(stderr, "%s must be between %ld and %ld\n", name, low, high);
      return -1;
    }
  else *param=i;
  return 0;
}
#endif

int set_play_mode(char *cp)
{
  PlayMode *pmp, **pmpp=play_mode_list;

  while ((pmp=*pmpp++))
    {
      if (pmp->id_character == *cp)
	{
	  play_mode=pmp;
	  while (*(++cp))
		 switch(*cp)
	      {
	      case 'U': pmp->encoding |= PE_ULAW; break; /* uLaw */
			case 'l': pmp->encoding &= ~PE_ULAW; break; /* linear */

			case '1': pmp->encoding |= PE_16BIT; break; /* 1 for 16-bit */
	      case '8': pmp->encoding &= ~PE_16BIT; break;

	      case 'M': pmp->encoding |= PE_MONO; break;
	      case 'S': pmp->encoding &= ~PE_MONO; break; /* stereo */

	      case 's': pmp->encoding |= PE_SIGNED; break;
	      case 'u': pmp->encoding &= ~PE_SIGNED; break;

	      case 'x': pmp->encoding ^= PE_BYTESWAP; break; /* toggle */

			default:
		fprintf(stderr, "Unknown format modifier `%c'\n", *cp);
		return 1;
	      }
	  return 0;
	}
	 }
  
  fprintf(stderr, "Playmode `%c' is not compiled in.\n", *cp);
  return 1;
}

#if 0
static int set_ctl(char *cp)
{
  ControlMode *cmp, **cmpp=ctl_list;

  while ((cmp=*cmpp++))
    {
      if (cmp->id_character == *cp)
	{
	  ctl=cmp;
	  while (*(++cp))
	    switch(*cp)
			{
			case 'v': cmp->verbosity++; break;
	   		case 'q': cmp->verbosity--; break;
			case 't': /* toggle */
		cmp->trace_playing= (cmp->trace_playing) ? 0 : 1; 
		break;

	      default:
		fprintf(stderr, "Unknown interface option `%c'\n", *cp);
		return 1;
	      }
	  return 0;
	}
	 }
  
  fprintf(stderr, "Interface `%c' is not compiled in.\n", *cp);
  return 1;
}
#endif

char *cfg_names[30];


void clear_config(void)
{
    ToneBank *bank=0;
    int i, j;

    for (i = 0; i < MAXBANK; i++)
    {
		if (tonebank[i])
		{
			bank = tonebank[i];
			if (bank->name) free((void *)bank->name);
			bank->name = 0;
			for (j = 0; j < MAXPROG; j++)
			  if (bank->tone[j].name)
			  {
			     free((void *)bank->tone[j].name);
			     bank->tone[j].name = 0;
			  }
			if (i > 0)
			{
			     free(tonebank[i]);
			     tonebank[i] = 0;
			}
		}
		if (drumset[i])
		{
			bank = drumset[i];
			if (bank->name) free((void *)bank->name);
			bank->name = 0;
			for (j = 0; j < MAXPROG; j++)
			  if (bank->tone[j].name)
			  {
			     free((void *)bank->tone[j].name);
			     bank->tone[j].name = 0;
			  }
			if (i > 0)
			{
			     free(drumset[i]);
			     drumset[i] = 0;
			}
		}
    }

    memset(drumset[0], 0, sizeof(ToneBank));
    memset(tonebank[0], 0, sizeof(ToneBank));
    clear_pathlist(0);
}


int reverb_options=7;
int global_reverb = 0;
int global_chorus = 0;
int global_echo = 0;
int global_detune = 0;

int got_a_configuration=0;

/*
static char midi_name[FILENAME_MAX+1];
static char midi_path_name[FILENAME_MAX+1];
*/

/************************************/

struct midi_local_data
{
	char midi_name[FILENAME_MAX+1];
	char midi_path_name[FILENAME_MAX+1];
	int count;
	FILE *midi_fd;
	int is_playing;
	int is_open;
};

/************************************/

static uint32 info_sample_count = 0;

static int look_midi_file(input_object *obj)
{
        struct midi_local_data *data;
	MidiEvent *event;
	uint32 events, samples;
	int rc;
	int32 val;
	FILE *fp;

	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;

  ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MIDI file: %s", data->midi_name);

	data->is_open = FALSE;

  if (!strcmp(data->midi_path_name, "-"))
    {
      fp=stdin;
      strcpy(current_filename, "(stdin)");
    }
  else if (!(fp=open_file(data->midi_path_name, 1, OF_VERBOSE, 0)))
    return 0;

  ctl->file_name(current_filename);

  event=read_midi_file_info(fp, &events, &samples);


  if (fp != stdin)
      close_file(fp);
  
  if (!event)
      return 0;

  ctl->cmsg(CMSG_INFO, VERB_NOISY, 
	    "%d supported events, %d samples", events, samples);

  ctl->total_time(samples);
  data->is_open = TRUE;
  data->count = samples;
  free(event);
  return 1;
}

static void init_midi()
{
#ifdef PLUGDEBUG
	fprintf(stderr,"init_midi\n");
#endif

  output_fragsize = 4096;

#ifdef DEFAULT_PATH
  if (!got_a_configuration)
  add_to_pathlist(DEFAULT_PATH, 0);
#endif

  if (!got_a_configuration)
	read_config_file(CONFIG_FILE, 1);
  if (output_rate) play_mode->rate=output_rate;
  if (output_name)
	 {
		play_mode->name=output_name;
	 }
  init_tables();

  if (!control_ratio) {

	  control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
	  if(control_ratio<1)
		 control_ratio=1;
	  else if (control_ratio > MAX_CONTROL_RATIO)
		 control_ratio=MAX_CONTROL_RATIO;
  }

  if (*def_instr_name) set_default_instrument(def_instr_name);
  if (got_a_configuration < 2) read_config_file(current_config_file, 0);
	return;
}


/*
 * test, if it's a .MIDI file, 0 if ok (and set the speed, stereo etc.)
 *                            < 0 if not
 */
static int test_midifile(char *buffer)
{
	if (strncmp(buffer, "MThd", 4)) return -1;
	return 0;
}

static int midi_open(input_object *obj, char *name)
{
        struct midi_local_data *data;
	int rc = 0;
	char *ptr;
	
	if (!obj) return 0;

	obj->local_data = malloc(sizeof(struct midi_local_data));
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;

	if (!got_a_configuration) init_midi();

        if (strlen(name) > FILENAME_MAX) {
                strncpy(data->midi_path_name, name, FILENAME_MAX-1);
                data->midi_path_name[FILENAME_MAX-1] = 0;
        } else  strcpy(data->midi_path_name, name);


	ptr = strrchr(name, '/');
        if (ptr) ptr++;
        else ptr = name;

        if (strlen(ptr) > FILENAME_MAX) {
                strncpy(data->midi_name, ptr, FILENAME_MAX-1);
                data->midi_name[FILENAME_MAX-1] = 0;
        } else  strcpy(data->midi_name, ptr);

	data->is_open = FALSE;
	data->is_playing = FALSE;

  	rc = look_midi_file(obj);
#ifdef PLUGDEBUG
	fprintf(stderr,"midi open maybe(%s) returned %d\n", name, rc);
#endif
	obj->flags = P_SEEK;

	return rc;
}

static int midi_truly_open(input_object *obj)
{
        struct midi_local_data *data;
	
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;

	if (data->is_playing) {
		play_midi_finish();
		data->is_playing = FALSE;
	}
#ifdef PLUGDEBUG
	fprintf(stderr,"midi truly open(%s)\n", data->midi_path_name);
#endif
	if (!got_a_configuration) init_midi();

  	flushing_output_device = FALSE;
	play_mode->purge_output();

  	play_midi_file(data->midi_path_name);
	data->is_playing = TRUE;
	data->is_open = TRUE;
	data->count = sample_count;

	return 1;
}


void midi_close(input_object *obj)
{
        struct midi_local_data *data;

	if (!obj) return;
	if (!obj->local_data) return;
	data = (struct midi_local_data *)obj->local_data;

	if (data->is_playing) {
		play_midi_finish();
	}
	free(obj->local_data);
	obj->local_data = NULL;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_close\n");
#endif
}

static int midi_play_frame(input_object *obj, char *buf)
{
        struct midi_local_data *data;
	int rc = 0;
	
#ifdef PLUGDEBUG
fprintf(stderr,"midi_play_frame to %x\n", buf);
#endif
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;

	if (!data->is_playing) midi_truly_open(obj);

	if (!data->is_playing) return 0;

	if (bbcount < output_fragsize || (!flushing_output_device && bbcount < 3 * output_fragsize)) {
#ifdef PLUGDEBUG
		fprintf(stderr,"bbcount of %d < fragsize %d: ", bbcount, output_fragsize);
#endif
		rc = play_some_midi();
		if (rc == RC_TUNE_END) flushing_output_device = TRUE;
#ifdef PLUGDEBUG
if (rc == RC_TUNE_END) fprintf(stderr,"tune is ending: bbcount after play is %d\n", bbcount);
		fprintf(stderr,"bbcount after play is %d\n", bbcount);
#endif
	}

#ifdef PLUGDEBUG
if (bbcount < output_fragsize) fprintf(stderr, "Can't play -- wish to stop.\n");
#endif
	if (bbcount < output_fragsize) return 0;

	rc = plug_output(buf);

#ifdef PLUGDEBUG
if (!rc) fprintf(stderr, "Nothing to write -- wish to stop.\n");
#endif
	if (!rc) return 0; /* nothing was written */

	return 1;
}

static int midi_frame_seek(input_object *obj, int frame)
{
        struct midi_local_data *data;
	int result = 0;
	int current_frame = 0, tim_time = 0;

	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;
	if (data->is_playing) {
		current_frame = (b_out_count() + bbcount) / output_fragsize;
		if (current_frame == frame) return 1;
		if (current_frame > frame - 2 && current_frame < frame + 2) return 1;
		tim_time = frame * output_fragsize / 4;
		if (tim_time < 0) tim_time = 0;
		if (tim_time > data->count) return 0;
		result = skip_to(tim_time);
	}
	else if (!frame) result = 1;
	else result = 0;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_frame_seek to %d; result %d from skip_to(%d)\n", frame,
		result, tim_time);
#endif
	return result;
}


static int midi_frame_size(input_object *obj)
{
        struct midi_local_data *data;
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_frame_size is %d\n", output_fragsize);
#endif
	return output_fragsize;
}


static int midi_nr_frames(input_object *obj)
{
        struct midi_local_data *data;
	int result = 0;	

	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;

	result = data->count * 4 / output_fragsize;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_nr_frames is %d\n", result);
#endif
	return result;
}
		

static int midi_sample_rate(input_object *obj)
{
        struct midi_local_data *data;
	int result = 0;	

	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;

	result = 44100;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_sample_rate is %d\n", result);
#endif
	return result;
}


static int midi_channels(input_object *obj)
{
#ifdef PLUGDEBUG
fprintf(stderr,"midi_channels\n");
#endif
	return 2; /* Yes, always stereo ...  */
}

/************

	result = sample_count * 4 / output_fragsize;

midi_nr_frames is 11042
midi_frame_to_sec for 11042 frames is 1025 for frag=4096
midi_nr_frames is 9304
midi_frame_to_sec for 9304 frames is 864 for frag=4096
midi_nr_frames is 9100
midi_frame_to_sec for 9100 frames is 845 for frag=4096
midi_nr_frames is 9070
midi_frame_to_sec for 9070 frames is 842 for frag=4096
midi_nr_frames is 19291
midi_frame_to_sec for 19291 frames is 1791 for frag=4096
midi_frame_seek to 0; result 0 from skip_to(0)
midi_frame_to_sec for 0 frames is 0 for frag=4096
midi_nr_frames is 11042
midi_frame_to_sec for 11042 frames is 1025 for frag=4096
midi_frame_to_sec for 4 frames is 0 for frag=4096
midi_nr_frames is 11042
midi_frame_to_sec for 11042 frames is 1025 for frag=4096
midi_frame_to_sec for 9 frames is 0 for frag=4096
midi_nr_frames is 11042

4096 bytes = 1 frame = 1024 samples at 44100 samples/sec takes 1024/44100 seconds
********/
static long midi_frame_to_sec(input_object *obj, int frame)
{
        struct midi_local_data *data;
	unsigned long result = 0;
	unsigned long sample_pos;
	
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;

	sample_pos = frame * output_fragsize / 4;
	/*result = (unsigned long)( pos / 44100 / 100 / 4 );*/
	/*result = (unsigned long)( pos / 44100 / 100 );*/
	/*result = (unsigned long)( pos / 44100);*/
	result = (unsigned long)( sample_pos / 44100);
	result *= 100; /* centiseconds ?? */
#ifdef PLUGDEBUG
fprintf(stderr,"midi_frame_to_sec for %d frames is %lu for frag=%d\n", frame, result, output_fragsize);
#endif
	return result;
}

static float midi_can_handle(const char *name) {
	FILE *fd;
	struct stat st;
	char mbuff[4];

#ifdef PLUGDEBUG
fprintf(stderr,"midi_can_handle(%s)?\n", name);
#endif

	if (stat(name, &st)) return 0.0;

	if (!S_ISREG(st.st_mode)) return 0.0;

	if ((fd = fopen(name, "r")) == NULL) {
		return 0.0;
	}	
	if (fread(mbuff, 1, 4, fd) != 4) {
			fclose(fd);
			return 0.0;
	}
	fclose(fd);

	if (test_midifile(mbuff) >= 0) return 1.0;
	else return 0.0;
}

static int midi_stream_info(input_object *obj, stream_info *info)
{
        struct midi_local_data *data;

	if (!obj) return 0;
	if (!obj->local_data) return 0;
	data = (struct midi_local_data *)obj->local_data;

#ifdef PLUGDEBUG
fprintf(stderr,"midi_stream_info\n");
#endif
	if (!obj || !info)
			return 0;
	sprintf(info->stream_type, "%d-bit %dKhz %s MIDI", 16, 
		44100 / 1000, "stereo");
	info->author[0] = 0;
	info->status[0] = 0;
	strcpy(info->title, data->midi_name);	
	
	return 1;
}

static int midi_init()
{
#ifdef PLUGDEBUG
fprintf(stderr,"midi_init\n");
#endif
	init_midi();
	return 1;
}


/*****for reference

typedef struct _input_plugin
{
	input_version_type version;	
	input_flags_type	flags;
	char name[256];
	char author[256];
	void *handle;
	input_init_type init;
	input_shutdown_type shutdown;
	input_plugin_handle_type plugin_handle;
	input_can_handle_type can_handle;
	input_open_type open;
	input_close_type close;
	input_play_frame_type play_frame;
	input_frame_seek_type frame_seek;
	input_frame_size_type frame_size;
	input_nr_frames_type nr_frames;
	input_frame_to_sec_type frame_to_sec;
	input_sample_rate_type sample_rate;
	input_channels_type channels;
	input_stream_info_type stream_info;
	input_nr_tracks_type nr_tracks;
	input_track_seek_type track_seek;
} input_plugin;

 * **********/

input_plugin midi_plugin = {
	INPUT_PLUGIN_VERSION,
	0,
	{ "MIDI player v0.01" },
	{ "Greg Lee" },
	NULL,
	midi_init,
	NULL,
	NULL,
	midi_can_handle,
	midi_open,
	midi_close,
	midi_play_frame,
	midi_frame_seek,
	midi_frame_size,
	midi_nr_frames,
	midi_frame_to_sec,
	midi_sample_rate,
	midi_channels,
	midi_stream_info,
	NULL,
	NULL
};

input_plugin *input_plugin_info(void)
{
#ifdef PLUGDEBUG
fprintf(stderr,"I'm me\n");
#endif
	return &midi_plugin;
}

