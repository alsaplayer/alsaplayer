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

    The midi plugin for alsaplayer is a version of Tuukka Toivonen's TiMidity
    done by Greg Lee, greg@ling.lll.hawaii.edu, April 2002.
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
#include "md.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "resample.h"
#include "version.h"
#include "sbk.h"

#include <sys/stat.h>

#include "input_plugin.h"
#ifdef DO_PREFS
#include "prefs.h"
#endif



/*#define PLUGDEBUG*/
/************************************/

int config_voices=DEFAULT_VOICES;
int config_amplification = DEFAULT_AMPLIFICATION;
int free_instruments_afterwards=0;
/*int free_instruments_afterwards=1;*/
static char def_instr_name[256]="";
int cfg_select = 0;
#ifdef CHANNEL_EFFECT
extern void effect_activate( int iSwitch ) ;
extern int init_effect(struct md *d) ;
#endif /*CHANNEL_EFFECT*/

int have_commandline_midis = 0;
int32 tmpi32, output_rate=0;
char *output_name=0;

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

static int look_midi_file(struct md *d)
{
	int rc;
	int32 val;

  ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MIDI file: %s", d->midi_name);

  if (!(d->fp=fopen(d->midi_path_name, "r")))
    return 0;

  ctl->file_name(d->midi_name);

  read_midi_file(d);

  if (d->fp) close_file(d->fp);
  
  if (!d->event)
      return 0;

  ctl->cmsg(CMSG_INFO, VERB_NOISY, 
	    "%d supported events, %d samples", d->event_count, d->sample_count);

  ctl->total_time(d->sample_count);
  free(d->event);
  return 1;
}


static int configuration_failed = FALSE;

static char *homedir;
static char prefs_path[1024];
static char *get_homedir()
{
	char *homedir = NULL;
	homedir = getenv("HOME");
	return homedir;
}

static int init_midi()
{
	if (configuration_failed) return FALSE;
	if (got_a_configuration) return TRUE;
#ifdef PLUGDEBUG
	fprintf(stderr,"init_midi\n");
#endif

	autocfg();

#ifdef DO_PREFS
	prefs_set_bool(ap_prefs, "midi", "active", 1);
#endif

  output_fragsize = 8192;

  if (read_config_file("timidity.cfg", 1)) {
	  configuration_failed = TRUE;
	  return FALSE;
  }
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
  if (got_a_configuration < 2) {
	  if (read_config_file(current_config_file, 0)) {
	  	configuration_failed = TRUE;
	  	return FALSE;
	  }
  }
  return TRUE;
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
        struct md *d;
	int rc = 0;
	char *ptr;
	
	if (!init_midi()) return FALSE;
	if (!obj) return 0;

	obj->local_data = malloc(sizeof(struct md));
	if (!obj->local_data) return 0;
	d = (struct md *)obj->local_data;

	d->bbuf = 0;
	d->event = 0;
	d->is_playing = FALSE;
	d->is_open = FALSE;
	d->bboffset = 0;
	d->bbcount = 0;
	d->outchunk = 0;
	d->starting_up = 1;
	d->flushing = 0;
	d->out_count = 0;
	d->total_bytes = 0;
	d->super_buffer_count = 0;
	d->voices = config_voices;
	d->amplification = config_amplification;
	d->drumchannels = DEFAULT_DRUMCHANNELS;
	d->adjust_panning_immediately = 1;
	d->voice_reserve = 7 * config_voices / 8;
	d->dont_chorus = 0;
	d->dont_reverb = 0;
	d->dont_cspline = 0;
	d->current_interpolation = config_interpolation;
	d->dont_keep_looping = 0;

	d->XG_System_reverb_type = 0;
	d->XG_System_chorus_type = 0;
	d->XG_System_variation_type = 0;
	d->GM_System_On = 0;
	d->XG_System_On = 0;
	d->GS_System_On = 0;

	d->author[0] = '\0';
	d->title[0] = '\0';

	d->xmp_epoch = -1;
	d->xxmp_epoch = 0;
	d->time_expired = 0;
	d->last_time_expired = 0;
	d->last_req_time = 0;
	d->last_calc = 0;
	d->trouble_ahead = 0;
	d->calc_window = 1000;
	d->output_buffer_full = 10;
	d->flushing_output_device = FALSE;

#ifdef PLAYLOCK
	if (sem_init(&d->play_lock, 0, 1)) fprintf(stderr,"no semaphore\n");
#endif


        if (strlen(name) > FILENAME_MAX) {
                strncpy(d->midi_path_name, name, FILENAME_MAX-1);
                d->midi_path_name[FILENAME_MAX-1] = 0;
        } else  strcpy(d->midi_path_name, name);


	ptr = strrchr(name, '/');
        if (ptr) ptr++;
        else ptr = name;

        if (strlen(ptr) > FILENAME_MAX) {
                strncpy(d->midi_name, ptr, FILENAME_MAX-1);
                d->midi_name[FILENAME_MAX-1] = 0;
        } else  strcpy(d->midi_name, ptr);

  	rc = look_midi_file(d);
#ifdef PLUGDEBUG
	fprintf(stderr,"midi open maybe(%s) returned %d\n", name, rc);
#endif
	obj->flags = P_SEEK | P_REENTRANT;
	if (rc) {
		obj->ready = TRUE;
		obj->nr_frames = d->sample_count * 4 / output_fragsize;
		obj->nr_tracks = 0;
		obj->nr_channels = 2;
		obj->frame_size = output_fragsize;
	}
	return rc;
}

static int midi_truly_open(struct md *d)
{
	if (d->is_playing) {
		play_midi_finish(d);
		d->is_playing = FALSE;
	}
#ifdef PLUGDEBUG
	fprintf(stderr,"midi truly open(%s)\n", d->midi_path_name);
#endif
	if (!got_a_configuration) init_midi();

#ifdef CHANNEL_EFFECT
	init_effect(d);
	effect_activate(TRUE);
#endif
	play_mode->purge_output(d);

	/*d->is_playing = TRUE;*/
	/*d->is_open = TRUE;*/
  	play_midi_file(d);

	if (d->is_playing) return 1;
	return 0;
}


void midi_close(input_object *obj)
{
        struct md *d;

	if (!init_midi()) return;
	if (!obj) return;
	if (!obj->local_data) return;
	d = (struct md *)obj->local_data;

#ifdef PLAYLOCK
	sem_wait(&d->play_lock);
#endif
	if (d->is_playing) {
		play_midi_finish(d);
	}
#ifdef PLAYLOCK
	sem_destroy(&d->play_lock);
#endif
	if (d->bbuf) free(d->bbuf);
	free(obj->local_data);
	obj->local_data = NULL;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_close\n");
#endif
}


#ifdef PLAYLOCK
static void play_more(struct md *d)
{
	int rc;
	if (!d->is_playing) return;
	if (!d->bbuf) return;
	if (d->flushing_output_device) return;

	if (d->bbcount < BB_SIZE/2) {
		if ( sem_trywait(&d->play_lock) ) return;
		rc = play_some_midi(d);
		sem_post(&d->play_lock);
		if (rc == RC_TUNE_END) d->flushing_output_device = TRUE;
	}
}
#endif

#include <sys/time.h>

extern int gettimeofday(struct timeval *, struct timezone *);
static struct timeval tv;
static struct timezone tz;
static void time_sync(uint32 resync, int dosync, struct md *d)
{
	unsigned jiffies;

	if (dosync) {
		d->last_time_expired = resync;
		d->xmp_epoch = -1;
		d->xxmp_epoch = 0; 
		d->time_expired = 0;
		/*return;*/
	}
	gettimeofday (&tv, &tz);
	if (d->xmp_epoch < 0) {
		d->xxmp_epoch = tv.tv_sec;
		d->xmp_epoch = tv.tv_usec;
	}
	jiffies = (tv.tv_sec - d->xxmp_epoch)*100 + (tv.tv_usec - d->xmp_epoch)/10000;
	d->time_expired = (jiffies * play_mode->rate)/100 + d->last_time_expired;
}

static int midi_play_frame(input_object *obj, char *buf)
{
        struct md *d;
	int rc = 0;
	int need_more, obfp, slacktime;
	unsigned calc_start, req_interval;

#ifdef PLUGDEBUG
fprintf(stderr,"midi_play_frame to %x\n", buf);
#endif
	if (!init_midi()) return FALSE;
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	d = (struct md *)obj->local_data;

	if (!d->is_playing) {
		time_sync(0,1,d);
	       	midi_truly_open(d);
		d->last_slack = 0;
       	}

	if (!d->is_playing) return 0;
	time_sync(0,0,d);
	calc_start = d->time_expired;
	if (d->last_req_time) req_interval = d->time_expired - d->last_req_time;
	else req_interval = d->calc_window;

	slacktime = req_interval - d->last_calc;

	if (slacktime) {
		if (d->last_slack) d->trouble_ahead = 0;
		else if (d->trouble_ahead) d->trouble_ahead--;
	}
	else {
		d->trouble_ahead++;
		if (!d->last_slack) d->trouble_ahead += 2;
	}
	d->last_slack = slacktime;

/* Adjust the window. */
	if (!slacktime) /*d->calc_window -= d->calc_window / 4*/;
	else if (slacktime < d->calc_window) d->calc_window -= (d->calc_window - slacktime) / 2;
	else if (slacktime > d->calc_window)
		       d->calc_window += (slacktime - d->calc_window)/ 4;

	d->last_req_time = d->time_expired;
#ifdef TIMEREQDEBUG
	fprintf(stderr,"mpf: t=%d req int=%d cs=%d poly=%d bbc=%d\n", d->time_expired,
	     req_interval, d->current_sample, d->current_polyphony, d->bbcount);
#endif

	obfp = d->bbcount * 100 / BB_SIZE;

/*
fprintf(stderr,"mpf: slack=%d lc=%d req int=%d cwindow=%d poly=%d fp=%d rp=%d dv=%d vperm=%d obfp=%d trouble=%d lost=%d\n",
	     slacktime, d->last_calc,
	     req_interval, d->calc_window,
	     d->current_polyphony, 0,
	     d->voices - d->current_free_voices, d->current_dying_voices,
	     d->voices - d->voice_reserve,
	     obfp, d->trouble_ahead, d->lost_notes);
*/
	d->output_buffer_full = obfp - d->trouble_ahead;

	need_more = TRUE;
	if (d->bbcount < output_fragsize) {
		need_more = TRUE;
	}
	else if (!slacktime) need_more = FALSE;
	/*else if (d->last_calc > d->calc_window) need_more = FALSE;*/
	else if (d->bbcount > BB_SIZE - 3 * output_fragsize) {
		need_more = FALSE;
	}
	d->last_calc = 0;

	while (need_more) {
		int calc_time;
#ifdef PLUGDEBUG
		fprintf(stderr,"bbcount of %d < fragsize %d: ", d->bbcount, output_fragsize);
#endif
#ifdef PLAYLOCK
		sem_wait(&d->play_lock);
#endif
		rc = play_some_midi(d);
#ifdef PLAYLOCK
		sem_post(&d->play_lock);
#endif
		need_more = FALSE;
		time_sync(0,0,d);
#ifdef TIMEREQDEBUG
		fprintf(stderr,"calc time %d for bbc=%d\n", d->time_expired - calc_start, d->bbcount);
#endif
		calc_time = d->time_expired - calc_start;

		d->last_calc = calc_time;

		if (rc == RC_TUNE_END) d->flushing_output_device = TRUE;
		else if (d->super_buffer_count || calc_time > d->calc_window) /* nothing */;
		else if (d->bbcount < BB_SIZE - 3 * output_fragsize &&
			       	d->time_expired - calc_start <= 3*d->calc_window / 4) need_more = TRUE;
/*
	fprintf(stderr,"\tcalc %d, need_more=%d, bbcount=%d\n", d->time_expired - calc_start, need_more, d->bbcount);
*/

#ifdef PLUGDEBUG
if (rc == RC_TUNE_END) fprintf(stderr,"tune is ending: bbcount after play is %d\n", d->bbcount);
		fprintf(stderr,"bbcount after play is %d\n", d->bbcount);
#endif
	}

#ifdef PLUGDEBUG
if (d->bbcount < output_fragsize) fprintf(stderr, "Can't play -- wish to stop.\n");
#endif
	if (d->bbcount < output_fragsize) return 0;

	rc = plug_output(buf, d);

#ifdef PLUGDEBUG
if (!rc) fprintf(stderr, "Nothing to write -- wish to stop.\n");
#endif
	if (!rc) return 0; /* nothing was written */

	return 1;
}

static int midi_frame_seek(input_object *obj, int frame)
{
        struct md *d;
	int result = 0;
	int current_frame = 0, tim_time = 0;

	if (!init_midi()) return FALSE;
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	d = (struct md *)obj->local_data;
	if (d->is_playing) {
		current_frame = (b_out_count(d) + d->bbcount) / output_fragsize;
		if (current_frame == frame) return 1;
		/*if (current_frame > frame - 2 && current_frame < frame + 2) return 1;*/
		tim_time = frame * output_fragsize / 4;
		if (tim_time < 0) tim_time = 0;
		if (tim_time > d->sample_count) return 0;
		play_mode->purge_output(d);
		result = skip_to(tim_time, d);
	}
	else if (!frame) result = 1;
	else result = 0;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_frame_seek to %d; result %d from skip_to(%d, d)\n", frame,
		result, tim_time);
#endif
	return result;
}


static int midi_frame_size(input_object *obj)
{
        struct md *d;
	if (!init_midi()) return FALSE;
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	d = (struct md *)obj->local_data;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_frame_size is %d\n", output_fragsize);
#endif
	/*play_more(d);*/
	return output_fragsize;
}


static int midi_nr_frames(input_object *obj)
{
        struct md *d;
	int result = 0;	

	if (!init_midi()) return FALSE;
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	d = (struct md *)obj->local_data;
#ifdef PLAYLOCK
	play_more(d);
#endif
	result = d->sample_count * 4 / output_fragsize;
#ifdef PLUGDEBUG
fprintf(stderr,"midi_nr_frames is %d\n", result);
#endif
	return result;
}
		

static int midi_sample_rate(input_object *obj)
{
        struct md *d;
	int result = 0;	

	if (!init_midi()) return FALSE;
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	d = (struct md *)obj->local_data;

	/*play_more(d);*/
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

static long midi_frame_to_sec(input_object *obj, int frame)
{
        struct md *d;
	unsigned long result = 0;
	unsigned long sample_pos;
	
	if (!init_midi()) return FALSE;
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	d = (struct md *)obj->local_data;

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

	if (!init_midi()) return FALSE;

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
        struct md *d;

	if (!init_midi()) return FALSE;
	if (!obj) return 0;
	if (!obj->local_data) return 0;
	d = (struct md *)obj->local_data;

#ifdef PLUGDEBUG
fprintf(stderr,"midi_stream_info\n");
#endif
	if (!info) return 0;

	sprintf(info->stream_type, "%s midi: %d track%s, %d events",
		d->XG_System_On? "XG" : ( d->GS_System_On? "GS" : "GM"),
		d->track_info, (d->track_info > 1)? "s" : "", d->event_count);
	if (d->author[0]) snprintf(info->author, 80, "%s", d->author);
	else info->author[0] = '\0';
	sprintf(info->status, "notes: %3d", d->current_polyphony);
	if (d->title[0]) snprintf(info->title, 80, "%s", d->title);
	else strcpy(info->title, d->midi_name);	
	
	return 1;
}

static int midi_init()
{
#ifdef PLUGDEBUG
fprintf(stderr,"midi_init\n");
#endif
	if (!init_midi()) return FALSE;
	return 1;
}

static void midi_shutdown()
{
#ifdef PLUGDEBUG
fprintf(stderr,"midi_shutdown\n");
#endif
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
	/*0,*/ P_SEEK | P_REENTRANT,
	"MIDI player v0.01",
	"Greg Lee",
	NULL,
	midi_init,
	midi_shutdown,
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

