/*
	$Id$

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

    playmidi.c -- random stuff in need of rearrangement

*/

#include <stdio.h>
#ifndef __WIN32__
#include <unistd.h>
#endif
#include <stdlib.h>

#ifndef NO_STRING_H
# include <string.h>
#else
#include <strings.h>
#endif

#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include <sys/time.h>
#include "output.h"
#include "mix.h"
#include "controls.h"
#include "resample.h"

#include "tables.h"
/*<95GUI_MODIF_BEGIN>*/
#ifdef CHANNEL_EFFECT
extern void effect_ctrl_change( MidiEvent* pCurrentEvent );
extern void effect_ctrl_reset( int idChannel ) ;
int opt_effect = 0;
#endif /* CHANNEL_EFFECT */
/*<95GUI_MODIF_END>*/

#ifdef tplus
#define PORTAMENTO_TIME_TUNING		(1.0 / 5000.0)
#define PORTAMENTO_CONTROL_RATIO	256	/* controls per sec */
#endif

#ifdef __WIN32__
extern int intr;
#endif


#ifdef tplus
int dont_cspline=0;
#endif
int opt_dry = 1;
int opt_expression_curve = 1;
int opt_volume_curve = 1;
int opt_stereo_surround = 1;
int dont_filter_melodic=1;
int dont_filter_drums=1;
int dont_chorus=0;
int dont_reverb=0;
int current_interpolation=1;
int dont_keep_looping=0;
static int voice_reserve=0;

Channel channel[MAXCHAN];
signed char drumvolume[MAXCHAN][MAXNOTE];
signed char drumpanpot[MAXCHAN][MAXNOTE];
signed char drumreverberation[MAXCHAN][MAXNOTE];
signed char drumchorusdepth[MAXCHAN][MAXNOTE];

Voice voice[MAX_VOICES];

int
    voices=DEFAULT_VOICES;

int32
    control_ratio=0,
    amplification=DEFAULT_AMPLIFICATION;

FLOAT_T
    master_volume;

int32 drumchannels=DEFAULT_DRUMCHANNELS;
int adjust_panning_immediately=1;

int GM_System_On=0;
int XG_System_On=0;
int GS_System_On=0;

int XG_System_reverb_type;
int XG_System_chorus_type;
int XG_System_variation_type;

static int32 lost_notes, cut_notes, played_notes;
#ifdef CHANNEL_EFFECT
int32 common_buffer[AUDIO_BUFFER_SIZE*2], /* stereo samples */
             *buffer_pointer;
static uint32 buffered_count;

MidiEvent *event_list=0, *current_event=0;
uint32 sample_count;
uint32 current_sample;
#else
static int32 common_buffer[AUDIO_BUFFER_SIZE*2], /* stereo samples */
             *buffer_pointer;
static uint32 buffered_count;

static MidiEvent *event_list=0, *current_event=0;
static uint32 sample_count, current_sample;
#endif

#ifdef tplus
static void update_portamento_controls(int ch);
#endif

static void kill_note(int i);

static void adjust_amplification(void)
{ 
  master_volume = (double)(amplification) / 100.0L;
  master_volume /= 4;
}

static void adjust_master_volume(int32 vol)
{ 
  master_volume = (double)(vol*amplification) / 1638400.0L;
  master_volume /= 4;
}

static void reset_voices(void)
{
  int i;
  for (i=0; i<MAX_VOICES; i++)
    voice[i].status=VOICE_FREE;
}

/* Process the Reset All Controllers event */
static void reset_controllers(int c)
{
  channel[c].volume=90; /* Some standard says, although the SCC docs say 0. */
  channel[c].expression=127; /* SCC-1 does this. */
  channel[c].sustain=0;
  channel[c].pitchbend=0x2000;
  channel[c].pitchfactor=0; /* to be computed */
  channel[c].reverberation = global_reverb;
  channel[c].chorusdepth = global_chorus;
#ifdef tplus
  channel[c].modulation_wheel = 0;
  channel[c].portamento_time_lsb = 0;
  channel[c].portamento_time_msb = 0;
  channel[c].tuning_lsb = 64;
  channel[c].tuning_msb = 64;
  channel[c].porta_control_ratio = 0;
  channel[c].portamento = 0;
  channel[c].last_note_fine = -1;

  channel[c].vibrato_ratio = 0;
  channel[c].vibrato_depth = 0;
  channel[c].vibrato_delay = 0;
/*
  memset(channel[c].envelope_rate, 0, sizeof(channel[c].envelope_rate));
*/
  update_portamento_controls(c);
#endif
#ifdef CHANNEL_EFFECT
  if (opt_effect) effect_ctrl_reset( c ) ;
#endif /* CHANNEL_EFFECT */
}

static void redraw_controllers(int c)
{
  ctl->volume(c, channel[c].volume);
  ctl->expression(c, channel[c].expression);
  ctl->sustain(c, channel[c].sustain);
  ctl->pitch_bend(c, channel[c].pitchbend);
}

static void reset_midi(void)
{
  int i;
  for (i=0; i<MAXCHAN; i++)
    {
      reset_controllers(i);
      /* The rest of these are unaffected by the Reset All Controllers event */
      channel[i].program=default_program;
      channel[i].panning=NO_PANNING;
      channel[i].pitchsens=2;
      channel[i].bank=0; /* tone bank or drum set */
      channel[i].harmoniccontent=64,
      channel[i].releasetime=64,
      channel[i].attacktime=64,
      channel[i].brightness=64,
      /*channel[i].kit=0;*/
      channel[i].sfx=0;
      /* channel[i].transpose and .kit are initialized in readmidi.c */
    }
  reset_voices();
}

static void select_sample(int v, Instrument *ip)
{
  int32 f, cdiff, diff, midfreq;
  int s,i;
  Sample *sp, *closest;

  s=ip->samples;
  sp=ip->sample;

  if (s==1)
    {
      voice[v].sample=sp;
      return;
    }

  f=voice[v].orig_frequency;
#if 0
/*
    Even if in range, a later patch may be closer. */

  for (i=0; i<s; i++)
    {
      if (sp->low_freq <= f && sp->high_freq >= f)
	{
	  voice[v].sample=sp;
	  return;
	}
      sp++;
    }
#endif
  /* 
     No suitable sample found! We'll select the sample whose root
     frequency is closest to the one we want. (Actually we should
     probably convert the low, high, and root frequencies to MIDI note
     values and compare those.) */

  cdiff=0x7FFFFFFF;
  closest=sp=ip->sample;
  midfreq = (sp->low_freq + sp->high_freq) / 2;
  for(i=0; i<s; i++)
    {
      diff=sp->root_freq - f;
  /*  But the root freq. can perfectly well lie outside the keyrange
   *  frequencies, so let's try:
   */
      /* diff=midfreq - f; */
      if (diff<0) diff=-diff;
      if (diff<cdiff)
	{
	  cdiff=diff;
	  closest=sp;
	}
      sp++;
    }
  voice[v].sample=closest;
  return;
}

static void select_stereo_samples(int v, InstrumentLayer *lp)
{
  Instrument *ip;
  InstrumentLayer *nlp, *bestvel;
  int diffvel, midvel, mindiff;

/* select closest velocity */
  bestvel = lp;
  mindiff = 500;
  for (nlp = lp; nlp; nlp = nlp->next) {
	midvel = (nlp->hi + nlp->lo)/2;
	if (!midvel) diffvel = 127;
	else if (voice[v].velocity < nlp->lo || voice[v].velocity > nlp->hi)
		diffvel = 200;
	else diffvel = voice[v].velocity - midvel;
	if (diffvel < 0) diffvel = -diffvel;
	if (diffvel < mindiff) {
		mindiff = diffvel;
		bestvel = nlp;
	}
  }
  ip = bestvel->instrument;

  if (ip->right_sample) {
    ip->sample = ip->right_sample;
    ip->samples = ip->right_samples;
    select_sample(v, ip);
    voice[v].right_sample = voice[v].sample;
  }
  else voice[v].right_sample = 0;

  ip->sample = ip->left_sample;
  ip->samples = ip->left_samples;
  select_sample(v, ip);
}

#ifndef tplus
static
#endif
void recompute_freq(int v)
{
  int 
    sign=(voice[v].sample_increment < 0), /* for bidirectional loops */
    pb=channel[voice[v].channel].pitchbend;
  double a;
  int32 tuning = 0;
  
  if (!voice[v].sample->sample_rate)
    return;

#ifdef tplus
#if 0
  if(!opt_modulation_wheel)
      voice[v].modulation_wheel = 0;
  if(!opt_portamento)
      voice[v].porta_control_ratio = 0;
#endif
  voice[v].vibrato_control_ratio = voice[v].orig_vibrato_control_ratio;
 
  if(voice[v].modulation_wheel > 0)
      {
	  voice[v].vibrato_control_ratio =
	      (int32)(play_mode->rate * MODULATION_WHEEL_RATE
		      / (2.0 * VIBRATO_SAMPLE_INCREMENTS));
	  voice[v].vibrato_delay = 0;
      }
#endif

#ifdef tplus
  if(voice[v].vibrato_control_ratio || voice[v].modulation_wheel > 0)
#else
  if (voice[v].vibrato_control_ratio)
#endif
    {
      /* This instrument has vibrato. Invalidate any precomputed
         sample_increments. */

      int i=VIBRATO_SAMPLE_INCREMENTS;
      while (i--)
	voice[v].vibrato_sample_increment[i]=0;
#ifdef tplus29
      if(voice[v].modulation_wheel > 0)
      {
	  voice[v].vibrato_control_ratio =
	      (int32)(play_mode->rate * MODULATION_WHEEL_RATE
		      / (2.0 * VIBRATO_SAMPLE_INCREMENTS));
	  voice[v].vibrato_delay = 0;
      }
#endif
    }


#ifdef tplus
  /* fine: [0..128] => [-256..256]
   * 1 coarse = 256 fine (= 1 note)
   * 1 fine = 2^5 tuning
   */
  tuning = (((int32)channel[voice[v].channel].tuning_lsb - 0x40)
	    + 64 * ((int32)channel[voice[v].channel].tuning_msb - 0x40)) << 7;
#endif

#ifdef tplus
  if(!voice[v].porta_control_ratio)
  {
#endif
#ifdef tplus29
  if(tuning == 0 && pb == 0x2000)
#else
  if (pb==0x2000 || pb<0 || pb>0x3FFF)
#endif
    voice[v].frequency=voice[v].orig_frequency;
  else
    {
      pb-=0x2000;
      if (!(channel[voice[v].channel].pitchfactor))
	{
	  /* Damn. Somebody bent the pitch. */
	  int32 i=pb*channel[voice[v].channel].pitchsens + tuning;
#ifdef tplus29
tuning;
	      if(i >= 0)
		  channel[ch].pitchfactor =
		      bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F];
	      else
	      {
		  i = -i;
		  channel[ch].pitchfactor = 1.0 /
		      (bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F]);
	      }
#else
	  if (pb<0)
	    i=-i;
	  channel[voice[v].channel].pitchfactor=
	    bend_fine[(i>>5) & 0xFF] * bend_coarse[i>>13];
#endif
	}
#ifndef tplus29
      if (pb>0)
#endif
	voice[v].frequency=
	  (int32)(channel[voice[v].channel].pitchfactor *
		  (double)(voice[v].orig_frequency));
#ifndef tplus29
      else
	voice[v].frequency=
	  (int32)((double)(voice[v].orig_frequency) /
		  channel[voice[v].channel].pitchfactor);
#endif
    }
#ifdef tplus
  }
  else /* Portament */
  {
      int32 i;
      FLOAT_T pf;

      pb -= 0x2000;

      i = pb * channel[voice[v].channel].pitchsens + (voice[v].porta_pb << 5) + tuning;

      if(i >= 0)
	  pf = bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F];
      else
      {
	  i = -i;
	  pf = 1.0 / (bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F]);
      }
      voice[v].frequency = (int32)((double)(voice[v].orig_frequency) * pf);
      /*voice[v].cache = NULL;*/
  }
#endif

  a = FRSCALE(((double)(voice[v].sample->sample_rate) *
	      (double)(voice[v].frequency)) /
	     ((double)(voice[v].sample->root_freq) *
	      (double)(play_mode->rate)),
	     FRACTION_BITS);

  /* what to do if incr is 0? --gl */
  /* if (!a) a++; */
  a += 0.5;
  if ((int32)a < 1) a = 1;

  if (sign) 
    a = -a; /* need to preserve the loop direction */

  voice[v].sample_increment = (int32)(a);
}

static int vcurve[128] = {
0,0,18,29,36,42,47,51,55,58,
60,63,65,67,69,71,73,74,76,77,
79,80,81,82,83,84,85,86,87,88,
89,90,91,92,92,93,94,95,95,96,
97,97,98,99,99,100,100,101,101,102,
103,103,104,104,105,105,106,106,106,107,
107,108,108,109,109,109,110,110,111,111,
111,112,112,112,113,113,114,114,114,115,
115,115,116,116,116,116,117,117,117,118,
118,118,119,119,119,119,120,120,120,120,
121,121,121,122,122,122,122,123,123,123,
123,123,124,124,124,124,125,125,125,125,
126,126,126,126,126,127,127,127
};

static void recompute_amp(int v)
{
  int32 tempamp;
  int chan = voice[v].channel;
  int vol = channel[chan].volume;
  int expr = channel[chan].expression;
  int vel = vcurve[voice[v].velocity];
  FLOAT_T curved_expression, curved_volume;

  /* TODO: use fscale */

  if (channel[chan].kit)
   {
    int note = voice[v].sample->note_to_use;
    if (note>0 && drumvolume[chan][note]>=0) vol = drumvolume[chan][note];
   }

  if (opt_expression_curve == 2) curved_expression = 127.0 * def_vol_table[expr];
  else if (opt_expression_curve == 1) curved_expression = 127.0 * expr_table[expr];
  else curved_expression = (FLOAT_T)expr;

  if (opt_volume_curve == 2) curved_volume = 127.0 * def_vol_table[vol];
  else if (opt_volume_curve == 1) curved_volume = 127.0 * expr_table[vol];
  else curved_volume = (FLOAT_T)vol;

  tempamp= (int32)((FLOAT_T)vel * curved_volume * curved_expression); /* 21 bits */

  if (!(play_mode->encoding & PE_MONO))
    {
      /*if (voice[v].panning > 60 && voice[v].panning < 68)*/
      if (voice[v].panning > 62 && voice[v].panning < 66)
	{
	  voice[v].panned=PANNED_CENTER;

	  voice[v].left_amp=
	    FRSCALENEG((double)(tempamp) * voice[v].volume * master_volume,
		      21);
	}
      else if (voice[v].panning<5)
	{
	  voice[v].panned = PANNED_LEFT;

	  voice[v].left_amp=
	    FRSCALENEG((double)(tempamp) * voice[v].volume * master_volume,
		      20);
	}
      else if (voice[v].panning>123)
	{
	  voice[v].panned = PANNED_RIGHT;

	  voice[v].left_amp= /* left_amp will be used */
	    FRSCALENEG((double)(tempamp) * voice[v].volume * master_volume,
		      20);
	}
      else
	{
	  voice[v].panned = PANNED_MYSTERY;

	  voice[v].left_amp=
	    FRSCALENEG((double)(tempamp) * voice[v].volume * master_volume,
		      27);
	  voice[v].right_amp=voice[v].left_amp * (voice[v].panning);
	  voice[v].left_amp *= (double)(127-voice[v].panning);
	}
    }
  else
    {
      voice[v].panned=PANNED_CENTER;

      voice[v].left_amp=
	FRSCALENEG((double)(tempamp) * voice[v].volume * master_volume,
		  21);
    }
}


static int current_polyphony = 0;
#ifdef POLYPHONY_COUNT
static int future_polyphony = 0;
#endif

#define NOT_CLONE 0
#define STEREO_CLONE 1
#define REVERB_CLONE 2
#define CHORUS_CLONE 3


/* just a variant of note_on() */
static int vc_alloc(int j, int clone_type)
{
  int i=voices; 
  int vc_count = 0, vc_ret = -1;

  while (i--)
    {
      if (i == j) continue;
      if (voice[i].status == VOICE_FREE) {
	vc_ret = i;
	vc_count++;
      }
    }
  if (vc_count > voice_reserve || clone_type == STEREO_CLONE) return vc_ret;
  return -1;
}

static void kill_others(MidiEvent *e, int i)
{
  int j=voices; 

  /*if (!voice[i].sample->exclusiveClass) return; */

  while (j--)
    {
      if (voice[j].status & (VOICE_FREE|VOICE_OFF|VOICE_DIE)) continue;
      if (i == j) continue;
      if (voice[i].channel != voice[j].channel) continue;
      if (voice[j].sample->note_to_use)
      {
    	/* if (voice[j].sample->note_to_use != voice[i].sample->note_to_use) continue; */
	if (!voice[i].sample->exclusiveClass) continue;
    	if (voice[j].sample->exclusiveClass != voice[i].sample->exclusiveClass) continue;
        kill_note(j);
      }
     /*
      else if (voice[j].note != (e->a &0x07f)) continue;
      kill_note(j);
      */
    }
}


static void clone_voice(Instrument *ip, int v, MidiEvent *e, uint8 clone_type, int variationbank)
{
  int w, k, played_note, chorus, reverb, milli;
  int chan = voice[v].channel;


  if (clone_type == STEREO_CLONE) {
	if (!voice[v].right_sample && variationbank != 3) return;
	if (variationbank == 6) return;
  }
  chorus = ip->sample->chorusdepth;
  reverb = ip->sample->reverberation;

  if (channel[chan].kit) {
	if ((k=drumreverberation[chan][voice[v].note]) >= 0) reverb = (reverb>k)? reverb : k;
	if ((k=drumchorusdepth[chan][voice[v].note]) >= 0) chorus = (chorus>k)? chorus : k;
  }
  else {
	if ((k=channel[chan].chorusdepth) >= 0) chorus = (chorus>k)? chorus : k;
	if ((k=channel[chan].reverberation) >= 0) reverb = (reverb>k)? reverb : k;
  }

  if (clone_type == REVERB_CLONE) chorus = 0;
  else if (clone_type == CHORUS_CLONE) reverb = 0;
  else if (clone_type == STEREO_CLONE) reverb = chorus = 0;

  if (reverb > 127) reverb = 127;
  if (chorus > 127) chorus = 127;

  if (clone_type == REVERB_CLONE) {
	 if ( (reverb_options & OPT_REVERB_EXTRA) && reverb < global_echo)
		reverb = global_echo;
	 if (/*reverb < 8 ||*/ dont_reverb) return;
  }
  if (clone_type == CHORUS_CLONE) {
	 if (variationbank == 32) chorus = 30;
	 else if (variationbank == 33) chorus = 60;
	 else if (variationbank == 34) chorus = 90;
	 if ( (reverb_options & OPT_CHORUS_EXTRA) && chorus < global_detune)
		chorus = global_detune;
	 if (/*chorus < 4 ||*/ dont_chorus) return;
  }

  if (!voice[v].right_sample) {
	voice[v].right_sample = voice[v].sample;
  }
  else if (clone_type == STEREO_CLONE) variationbank = 0;
	/* don't try to fake a second patch if we have a real one */

  if ( (w = vc_alloc(v, clone_type)) < 0 ) return;

  if (clone_type==STEREO_CLONE) voice[v].clone_voice = w;
  voice[w].clone_voice = v;
  voice[w].clone_type = clone_type;

  voice[w].status = voice[v].status;
  voice[w].channel = voice[v].channel;
  voice[w].note = voice[v].note;
  voice[w].sample = voice[v].right_sample;
  voice[w].velocity= (e->b * (127 - voice[w].sample->attenuation)) / 127;
  voice[w].left_sample = voice[v].left_sample;
  voice[w].right_sample = voice[v].right_sample;
  voice[w].orig_frequency = voice[v].orig_frequency;
  voice[w].frequency = voice[v].frequency;
  voice[w].sample_offset = voice[v].sample_offset;
  voice[w].sample_increment = voice[v].sample_increment;
  voice[w].current_x0 =
    voice[w].current_x1 =
    voice[w].current_y0 =
    voice[w].current_y1 = 0;
  voice[w].echo_delay = voice[v].echo_delay;
  voice[w].starttime = voice[v].starttime;
  voice[w].envelope_volume = voice[v].envelope_volume;
  voice[w].envelope_target = voice[v].envelope_target;
  voice[w].envelope_increment = voice[v].envelope_increment;
  voice[w].modulation_volume = voice[v].modulation_volume;
  voice[w].modulation_target = voice[v].modulation_target;
  voice[w].modulation_increment = voice[v].modulation_increment;
  voice[w].tremolo_sweep = voice[w].sample->tremolo_sweep_increment;
  voice[w].tremolo_sweep_position = voice[v].tremolo_sweep_position;
  voice[w].tremolo_phase = voice[v].tremolo_phase;
  voice[w].tremolo_phase_increment = voice[w].sample->tremolo_phase_increment;
  voice[w].lfo_sweep = voice[w].sample->lfo_sweep_increment;
  voice[w].lfo_sweep_position = voice[v].lfo_sweep_position;
  voice[w].lfo_phase = voice[v].lfo_phase;
  voice[w].lfo_phase_increment = voice[w].sample->lfo_phase_increment;
  voice[w].modLfoToFilterFc=voice[w].sample->modLfoToFilterFc;
  voice[w].modEnvToFilterFc=voice[w].sample->modEnvToFilterFc;
  voice[w].vibrato_sweep = voice[w].sample->vibrato_sweep_increment;
  voice[w].vibrato_sweep_position = voice[v].vibrato_sweep_position;
  voice[w].left_mix = voice[v].left_mix;
  voice[w].right_mix = voice[v].right_mix;
  voice[w].left_amp = voice[v].left_amp;
  voice[w].right_amp = voice[v].right_amp;
  voice[w].tremolo_volume = voice[v].tremolo_volume;
  voice[w].lfo_volume = voice[v].lfo_volume;
  for (k = 0; k < VIBRATO_SAMPLE_INCREMENTS; k++)
    voice[w].vibrato_sample_increment[k] =
      voice[v].vibrato_sample_increment[k];
  for (k=ATTACK; k<MAXPOINT; k++)
    {
	voice[w].envelope_rate[k]=voice[v].envelope_rate[k];
	voice[w].envelope_offset[k]=voice[v].envelope_offset[k];
    }
  voice[w].vibrato_phase = voice[v].vibrato_phase;
  voice[w].vibrato_depth = voice[w].sample->vibrato_depth;
  voice[w].vibrato_control_ratio = voice[w].sample->vibrato_control_ratio;
  voice[w].vibrato_control_counter = voice[v].vibrato_control_counter;
  voice[w].envelope_stage = voice[v].envelope_stage;
  voice[w].modulation_stage = voice[v].modulation_stage;
  voice[w].control_counter = voice[v].control_counter;

  voice[w].panning = voice[v].panning;

  if (voice[w].right_sample)
  	voice[w].panning = voice[w].right_sample->panning;
  else voice[w].panning = 127;

  if (clone_type == STEREO_CLONE) {
    int left, right;
    int panrequest = voice[v].panning;
    if (variationbank == 3) {
	voice[v].panning = 0;
	voice[w].panning = 127;
    }
    else {
	if (voice[v].sample->panning > voice[w].sample->panning) {
	  left = w;
	  right = v;
	}
	else {
	  left = v;
	  right = w;
	}
	if (panrequest < 64) {
	  if (opt_stereo_surround) {
	    voice[left].panning = 0;
	    voice[right].panning = panrequest;
	  }
	  else {
	    if (voice[left].sample->panning < panrequest)
		voice[left].panning = voice[left].sample->panning;
	    else voice[left].panning = panrequest;
	    voice[right].panning = (voice[right].sample->panning + panrequest + 32) / 2;
	  }
	}
	else {
	  if (opt_stereo_surround) {
	    voice[left].panning = panrequest;
	    voice[right].panning = 127;
	  }
	  else {
	    if (voice[right].sample->panning > panrequest)
		voice[right].panning = voice[right].sample->panning;
	    else voice[right].panning = panrequest;
	    voice[left].panning = (voice[left].sample->panning + panrequest - 32) / 2;
	  }
	}
    }
#ifdef DEBUG_CLONE_NOTES
fprintf(stderr,"STEREO_CLONE v%d vol%f pan%d\n", w, voice[w].volume, voice[w].panning);
#endif
  }

#ifdef tplus
  voice[w].porta_control_ratio = voice[v].porta_control_ratio;
  voice[w].porta_dpb = voice[v].porta_dpb;
  voice[w].porta_pb = voice[v].porta_pb;
  /* ?? voice[w].vibrato_delay = 0; */
  voice[w].vibrato_delay = voice[w].sample->vibrato_delay;
  voice[w].modulation_delay = voice[w].sample->modulation_rate[DELAY];
#endif
  voice[w].volume = voice[w].sample->volume;

  milli = play_mode->rate/1000;

  if (reverb) {

#if 0
	if (voice[w].panning < 64) voice[w].panning = 127;
	else voice[w].panning = 0;
#endif
    if (opt_stereo_surround) {
	if (voice[w].panning > 64) voice[w].panning = 127;
	else voice[w].panning = 0;
    }
    else {
	if (voice[v].panning < 64) voice[w].panning = 64 + reverb/2;
	else voice[w].panning = 64 - reverb/2;
    }

#ifdef DEBUG_REVERBERATION
printf("r=%d vol %f", reverb, voice[w].volume);
#endif

/* try 98->99 for melodic instruments ? (bit much for percussion) */
	voice[w].volume *= vol_table[(127-reverb)/8 + 98];

#ifdef DEBUG_REVERBERATION
printf(" -> vol %f", voice[w].volume);

printf(" delay %d", voice[w].echo_delay);
#endif
	/*voice[w].echo_delay += (reverb>>1) * milli;*/
	voice[w].echo_delay += reverb * milli;
#ifdef DEBUG_REVERBERATION
printf(" -> delay %d\n", voice[w].echo_delay);
#endif


	voice[w].envelope_rate[DECAY] *= 2;
	voice[w].envelope_rate[RELEASE] /= 2;

	if (XG_System_reverb_type >= 0) {
	    int subtype = XG_System_reverb_type & 0x07;
	    int rtype = XG_System_reverb_type >>3;
	    switch (rtype) {
		case 0: /* no effect */
		  break;
		case 1: /* hall */
		  if (subtype) voice[w].echo_delay += 100 * milli;
		  break;
		case 2: /* room */
		  voice[w].echo_delay /= 2;
		  break;
		case 3: /* stage */
		  voice[w].velocity = voice[v].velocity;
		  break;
		case 4: /* plate */
		  voice[w].panning = voice[v].panning;
		  break;
		case 16: /* white room */
		  voice[w].echo_delay = 0;
		  break;
		case 17: /* tunnel */
		  voice[w].echo_delay *= 2;
		  voice[w].velocity /= 2;
		  break;
		case 18: /* canyon */
		  voice[w].echo_delay *= 2;
		  break;
		case 19: /* basement */
		  voice[w].velocity /= 2;
		  break;
	        default: break;
	    }
	}
#ifdef DEBUG_CLONE_NOTES
fprintf(stderr,"REVERB_CLONE v%d vol=%f pan=%d reverb=%d delay=%dms\n", w, voice[w].volume, voice[w].panning, reverb,
	voice[w].echo_delay / milli);
#endif
  }
  played_note = voice[w].sample->note_to_use;
  if (!played_note) {
	played_note = e->a & 0x7f;
	if (variationbank == 35) played_note += 12;
	else if (variationbank == 36) played_note -= 12;
	else if (variationbank == 37) played_note += 7;
	else if (variationbank == 36) played_note -= 7;
  }
    played_note = ( (played_note - voice[w].sample->freq_center) * voice[w].sample->freq_scale ) / 1024 +
		voice[w].sample->freq_center;
  voice[w].note = played_note;
  voice[w].orig_frequency = freq_table[played_note];

  if (chorus) {
	/*voice[w].orig_frequency += (voice[w].orig_frequency/128) * chorus;*/
/*fprintf(stderr, "voice %d v sweep from %ld (cr %d, depth %d)", w, voice[w].vibrato_sweep,
	 voice[w].vibrato_control_ratio, voice[w].vibrato_depth);*/

	if (opt_stereo_surround) {
	  if (voice[v].panning < 64) voice[w].panning = voice[v].panning + 32;
	  else voice[w].panning = voice[v].panning - 32;
	}

	if (!voice[w].vibrato_control_ratio) {
		voice[w].vibrato_control_ratio = 100;
		voice[w].vibrato_depth = 6;
		voice[w].vibrato_sweep = 74;
	}
	/*voice[w].velocity = (voice[w].velocity * chorus) / 128;*/
#if 0
	voice[w].velocity = (voice[w].velocity * chorus) / (128+96);
	voice[v].velocity = voice[w].velocity;
	voice[w].volume = (voice[w].volume * chorus) / (128+96);
	voice[v].volume = voice[w].volume;
#endif
	/* voice[w].volume *= 0.9; */
	voice[w].volume *= 0.40;
	voice[v].volume = voice[w].volume;
	recompute_amp(v);
        apply_envelope_to_amp(v);
	voice[w].vibrato_sweep = chorus/2;
	voice[w].vibrato_depth /= 2;
	if (!voice[w].vibrato_depth) voice[w].vibrato_depth = 2;
	voice[w].vibrato_control_ratio /= 2;
	/*voice[w].vibrato_control_ratio += chorus;*/
/*fprintf(stderr, " to %ld (cr %d, depth %d).\n", voice[w].vibrato_sweep,
	 voice[w].vibrato_control_ratio, voice[w].vibrato_depth);
	voice[w].vibrato_phase = 20;*/

	voice[w].echo_delay += 30 * milli;

	if (XG_System_chorus_type >= 0) {
	    int subtype = XG_System_chorus_type & 0x07;
	    int chtype = 0x0f & (XG_System_chorus_type >> 3);
	    switch (chtype) {
		case 0: /* no effect */
		  break;
		case 1: /* chorus */
		  chorus /= 3;
		  if(channel[ voice[w].channel ].pitchbend + chorus < 0x2000)
            		voice[w].orig_frequency =
				(uint32)( (FLOAT_T)voice[w].orig_frequency * bend_fine[chorus] );
        	  else voice[w].orig_frequency =
			(uint32)( (FLOAT_T)voice[w].orig_frequency / bend_fine[chorus] );
		  if (subtype) voice[w].vibrato_depth *= 2;
		  break;
		case 2: /* celeste */
		  voice[w].orig_frequency += (voice[w].orig_frequency/128) * chorus;
		  break;
		case 3: /* flanger */
		  voice[w].vibrato_control_ratio = 10;
		  voice[w].vibrato_depth = 100;
		  voice[w].vibrato_sweep = 8;
		  voice[w].echo_delay += 200 * milli;
		  break;
		case 4: /* symphonic : cf Children of the Night /128 bad, /1024 ok */
		  voice[w].orig_frequency += (voice[w].orig_frequency/512) * chorus;
		  voice[v].orig_frequency -= (voice[v].orig_frequency/512) * chorus;
		  recompute_freq(v);
		  break;
		case 8: /* phaser */
		  break;
	      default:
		  break;
	    }
	}
	else {
	    chorus /= 3;
	    if(channel[ voice[w].channel ].pitchbend + chorus < 0x2000)
          	voice[w].orig_frequency =
			(uint32)( (FLOAT_T)voice[w].orig_frequency * bend_fine[chorus] );
            else voice[w].orig_frequency =
		(uint32)( (FLOAT_T)voice[w].orig_frequency / bend_fine[chorus] );
	}
#ifdef DEBUG_CLONE_NOTES
fprintf(stderr,"CHORUS_CLONE v%d vol%f pan%d chorus%d\n", w, voice[w].volume, voice[w].panning, chorus);
#endif
  }
  voice[w].loop_start = voice[w].sample->loop_start;
  voice[w].loop_end = voice[w].sample->loop_end;
  voice[w].echo_delay_count = voice[w].echo_delay;
  if (reverb) voice[w].echo_delay *= 2;

  recompute_freq(w);
  recompute_amp(w);
  if (voice[w].sample->modes & MODES_ENVELOPE)
    {
      /* Ramp up from 0 */
      voice[w].envelope_stage=ATTACK;
      voice[w].modulation_stage=ATTACK;
      voice[w].envelope_volume=0;
      voice[w].modulation_volume=0;
      voice[w].control_counter=0;
      voice[w].modulation_counter=0;
      recompute_envelope(w);
      if (voice[w].sample->cutoff_freq)
      	voice[w].bw_index = 1 + (voice[w].sample->cutoff_freq+50) / 100;
      else voice[w].bw_index = 0;
      recompute_modulation(w);
      apply_envelope_to_amp(w);
    }
  else
    {
      voice[w].envelope_increment=0;
      voice[w].modulation_increment=0;
      apply_envelope_to_amp(w);
    }
}

static void start_note(MidiEvent *e, int i)
{
  InstrumentLayer *lp;
  Instrument *ip;
  int j, banknum, ch=e->channel;
  int played_note, drumpan=NO_PANNING;
  int32 rt;
  int attacktime, releasetime, decaytime, variationbank;
  int brightness = channel[ch].brightness;
  int harmoniccontent = channel[ch].harmoniccontent;

  if (check_for_rc()) return;

  if (channel[ch].sfx) banknum=channel[ch].sfx;
  else banknum=channel[ch].bank;

  voice[i].velocity=e->b;

  if (channel[ch].kit)
    {
      if (!(lp=drumset[banknum]->tone[e->a].layer))
	{
	  if (!(lp=drumset[0]->tone[e->a].layer))
	    return; /* No instrument? Then we can't play. */
	}
      ip = lp->instrument;
      if (ip->type == INST_GUS && ip->samples != 1)
	{
	  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, 
	       "Strange: percussion instrument with %d samples!", ip->samples);
	  return;
	}

      if (ip->sample->note_to_use) /* Do we have a fixed pitch? */
	{
	  voice[i].orig_frequency=freq_table[(int)(ip->sample->note_to_use)];
	  drumpan=drumpanpot[ch][(int)ip->sample->note_to_use];
	}
      else
	voice[i].orig_frequency=freq_table[e->a & 0x7F];
    }
  else
    {
      if (channel[ch].program==SPECIAL_PROGRAM)
	lp=default_instrument;
      else if (!(lp=tonebank[banknum]->tone[channel[ch].program].layer))
	{
	  if (!(lp=tonebank[0]->tone[channel[ch].program].layer))
	    return; /* No instrument? Then we can't play. */
	}
      ip = lp->instrument;

      if (ip->sample->note_to_use) /* Fixed-pitch instrument? */
	voice[i].orig_frequency=freq_table[(int)(ip->sample->note_to_use)];
      else
	voice[i].orig_frequency=freq_table[e->a & 0x7F];
    } /* not drum channel */

    select_stereo_samples(i, lp);
    kill_others(e, i);

    voice[i].starttime = e->time;
    played_note = voice[i].sample->note_to_use;
    if (!played_note) played_note = e->a & 0x7f;
    played_note = ( (played_note - voice[i].sample->freq_center) * voice[i].sample->freq_scale ) / 1024 +
		voice[i].sample->freq_center;
    voice[i].note = played_note;
    voice[i].orig_frequency = freq_table[played_note];

  voice[i].status=VOICE_ON;
  voice[i].channel=ch;
  voice[i].note=e->a;
  voice[i].velocity= (e->b * (127 - voice[i].sample->attenuation)) / 127;
#if 0
  voice[i].velocity=e->b;
#endif
  voice[i].sample_offset=0;
  voice[i].sample_increment=0; /* make sure it isn't negative */
  /* why am I copying loop points? */
  voice[i].loop_start = voice[i].sample->loop_start;
  voice[i].loop_end = voice[i].sample->loop_end;
  voice[i].volume = voice[i].sample->volume;
  voice[i].current_x0 =
    voice[i].current_x1 =
    voice[i].current_y0 =
    voice[i].current_y1 = 0;
#ifdef tplus
  voice[i].modulation_wheel=channel[ch].modulation_wheel;
#endif

  voice[i].tremolo_phase=0;
  voice[i].tremolo_phase_increment=voice[i].sample->tremolo_phase_increment;
  voice[i].tremolo_sweep=voice[i].sample->tremolo_sweep_increment;
  voice[i].tremolo_sweep_position=0;

  voice[i].lfo_phase=0;
  voice[i].lfo_phase_increment=voice[i].sample->lfo_phase_increment;
  voice[i].lfo_sweep=voice[i].sample->lfo_sweep_increment;
  voice[i].lfo_sweep_position=0;
  voice[i].modLfoToFilterFc=voice[i].sample->modLfoToFilterFc;
  voice[i].modEnvToFilterFc=voice[i].sample->modEnvToFilterFc;
  voice[i].modulation_delay = voice[i].sample->modulation_rate[DELAY];

  voice[i].vibrato_sweep=voice[i].sample->vibrato_sweep_increment;
  voice[i].vibrato_depth=voice[i].sample->vibrato_depth;
  voice[i].vibrato_sweep_position=0;
  voice[i].vibrato_control_ratio=voice[i].sample->vibrato_control_ratio;
  voice[i].vibrato_delay = voice[i].sample->vibrato_delay;
#ifdef tplus
  if(channel[ch].vibrato_ratio && voice[i].vibrato_depth > 0)
  {
      voice[i].vibrato_control_ratio = channel[ch].vibrato_ratio;
      voice[i].vibrato_depth = channel[ch].vibrato_depth;
      voice[i].vibrato_delay = channel[ch].vibrato_delay;
  }
  else voice[i].vibrato_delay = 0;
#endif
  voice[i].vibrato_control_counter=voice[i].vibrato_phase=0;
  for (j=0; j<VIBRATO_SAMPLE_INCREMENTS; j++)
    voice[i].vibrato_sample_increment[j]=0;

  attacktime = channel[ch].attacktime;
  releasetime = channel[ch].releasetime;
  decaytime = 64;
  variationbank = channel[ch].variationbank;



  switch (variationbank) {
	case  8:
		attacktime = 64+32;
		break;
	case 12:
		decaytime = 64-32;
		break;
	case 16:
		brightness = 64+16;
		break;
	case 17:
		brightness = 64+32;
		break;
	case 18:
		brightness = 64-16;
		break;
	case 19:
		brightness = 64-32;
		break;
	case 20:
		harmoniccontent = 64+16;
		break;
	case 24:
		voice[i].modEnvToFilterFc=2.0;
      		voice[i].sample->cutoff_freq = 800;
		break;
	case 25:
		voice[i].modEnvToFilterFc=-2.0;
      		voice[i].sample->cutoff_freq = 800;
		break;
	case 27:
		voice[i].modLfoToFilterFc=2.0;
		voice[i].lfo_phase_increment=109;
		voice[i].lfo_sweep=122;
      		voice[i].sample->cutoff_freq = 800;
		break;
	case 28:
		voice[i].modLfoToFilterFc=-2.0;
		voice[i].lfo_phase_increment=109;
		voice[i].lfo_sweep=122;
      		voice[i].sample->cutoff_freq = 800;
		break;
	default:
		break;
  }


#ifdef RATE_ADJUST_DEBUG
{
int e_debug=0, f_debug=0;
int32 r;
if (releasetime!=64) e_debug=1;
if (attacktime!=64) f_debug=1;
if (e_debug) printf("ADJ release time by %d on %d\n", releasetime -64, ch);
if (f_debug) printf("ADJ attack time by %d on %d\n", attacktime -64, ch);
#endif

  for (j=ATTACK; j<MAXPOINT; j++)
    {
	voice[i].envelope_rate[j]=voice[i].sample->envelope_rate[j];
	voice[i].envelope_offset[j]=voice[i].sample->envelope_offset[j];
#ifdef RATE_ADJUST_DEBUG
if (f_debug) printf("\trate %d = %ld; offset = %ld\n", j,
voice[i].envelope_rate[j],
voice[i].envelope_offset[j]);
#endif
    }

  if (voice[i].sample->keyToVolEnvHold) {
	FLOAT_T rate_adjust;
	rate_adjust = pow(2.0, (60 - voice[i].note) * voice[i].sample->keyToVolEnvHold / 1200.0);
	voice[i].envelope_rate[HOLD] =
		(uint32)( (FLOAT_T)voice[i].envelope_rate[HOLD] / rate_adjust );
  }
  if (voice[i].sample->keyToVolEnvDecay) {
	FLOAT_T rate_adjust;
	rate_adjust = pow(2.0, (60 - voice[i].note) * voice[i].sample->keyToVolEnvDecay / 1200.0);
	voice[i].envelope_rate[DECAY] =
		(uint32)( (FLOAT_T)voice[i].envelope_rate[DECAY] / rate_adjust );
  }

  voice[i].echo_delay=voice[i].envelope_rate[DELAY];
  voice[i].echo_delay_count = voice[i].echo_delay;

#ifdef RATE_ADJUST_DEBUG
if (e_debug) {
printf("(old rel time = %ld)\n",
(voice[i].envelope_offset[2] - voice[i].envelope_offset[3]) / voice[i].envelope_rate[3]);
r = voice[i].envelope_rate[3];
r = r + ( (64-releasetime)*r ) / 100;
voice[i].envelope_rate[3] = r;
printf("(new rel time = %ld)\n",
(voice[i].envelope_offset[2] - voice[i].envelope_offset[3]) / voice[i].envelope_rate[3]);
}
}
#endif

  if (attacktime!=64)
    {
	rt = voice[i].envelope_rate[ATTACK];
	rt = rt + ( (64-attacktime)*rt ) / 100;
	if (rt > 1000) voice[i].envelope_rate[ATTACK] = rt;
    }
  if (releasetime!=64)
    {
	rt = voice[i].envelope_rate[RELEASE];
	rt = rt + ( (64-releasetime)*rt ) / 100;
	if (rt > 1000) voice[i].envelope_rate[RELEASE] = rt;
    }
  if (decaytime!=64)
    {
	rt = voice[i].envelope_rate[DECAY];
	rt = rt + ( (64-decaytime)*rt ) / 100;
	if (rt > 1000) voice[i].envelope_rate[DECAY] = rt;
    }

  if (channel[ch].panning != NO_PANNING)
    voice[i].panning=channel[ch].panning;
  else
    voice[i].panning=voice[i].sample->panning;
  if (drumpan != NO_PANNING)
    voice[i].panning=drumpan;

#ifdef tplus
  voice[i].porta_control_counter = 0;
  if(channel[ch].portamento && !channel[ch].porta_control_ratio)
      update_portamento_controls(ch);
  if(channel[ch].porta_control_ratio)
  {
      if(channel[ch].last_note_fine == -1)
      {
	  /* first on */
	  channel[ch].last_note_fine = voice[i].note * 256;
	  channel[ch].porta_control_ratio = 0;
      }
      else
      {
	  voice[i].porta_control_ratio = channel[ch].porta_control_ratio;
	  voice[i].porta_dpb = channel[ch].porta_dpb;
	  voice[i].porta_pb = channel[ch].last_note_fine -
	      voice[i].note * 256;
	  if(voice[i].porta_pb == 0)
	      voice[i].porta_control_ratio = 0;
      }
  }

  /* What "cnt"? if(cnt == 0) 
      channel[ch].last_note_fine = voice[i].note * 256; */
#endif

  if (voice[i].sample->sample_rate)
  {

		if (brightness >= 0 && brightness != 64) {
			int32 fq = voice[i].sample->cutoff_freq;
			if (brightness > 64) {
				if (!fq || fq > 8000) fq = 0;
				else fq += (brightness - 64) * (fq / 80);
			}
			else {
				if (!fq) fq = 6400 + (brightness - 64) * 80;
				else fq += (brightness - 64) * (fq / 80);
			}
			if (fq && fq < 349) fq = 349;
			else if (fq > 19912) fq = 19912;
			voice[i].sample->cutoff_freq = fq;

		}
		if (harmoniccontent >= 0 && harmoniccontent != 64) {
			voice[i].sample->resonance = harmoniccontent / 256.0;
		}
  }


  if (reverb_options & OPT_STEREO_EXTRA || variationbank == 1) {
    int pan = voice[i].panning;
    int disturb = 0;
    /* If they're close up (no reverb) and you are behind the pianist,
     * high notes come from the right, so we'll spread piano etc. notes
     * out horizontally according to their pitches.
     */
    if (channel[ch].program < 21) {
	    int n = voice[i].velocity - 32;
	    if (n < 0) n = 0;
	    if (n > 64) n = 64;
	    pan = pan/2 + n;
	}
    /* For other types of instruments, the music sounds more alive if
     * notes come from slightly different directions.  However, instruments
     * do drift around in a sometimes disconcerting way, so the following
     * might not be such a good idea.
     */
    else disturb = (voice[i].velocity/32 % 8) +
	(voice[i].note % 8); /* /16? */

    if (pan < 64) pan += disturb;
    else pan -= disturb;
    if (pan < 0) pan = 0;
    else if (pan > 127) pan = 127;
    voice[i].panning = pan;
  }

  recompute_freq(i);
  recompute_amp(i);
  if (voice[i].sample->modes & MODES_ENVELOPE)
    {
      /* Ramp up from 0 */
      voice[i].envelope_stage=ATTACK;
      voice[i].envelope_volume=0;
      voice[i].control_counter=0;
      voice[i].modulation_stage=ATTACK;
      voice[i].modulation_volume=0;
      voice[i].modulation_counter=0;
      if (voice[i].sample->cutoff_freq)
      	voice[i].bw_index = 1 + (voice[i].sample->cutoff_freq+50) / 100;
      else voice[i].bw_index = 0;
      recompute_envelope(i);
      recompute_modulation(i);
      apply_envelope_to_amp(i);
    }
  else
    {
      voice[i].envelope_increment=0;
      apply_envelope_to_amp(i);
    }
  voice[i].clone_voice = -1;
  voice[i].clone_type = NOT_CLONE;
#ifdef DEBUG_CLONE_NOTES
fprintf(stderr,"NOT_CLONE v%d vol%f pan%d\n", i, voice[i].volume, voice[i].panning);
#endif

  if (reverb_options & OPT_STEREO_VOICE) clone_voice(ip, i, e, STEREO_CLONE, variationbank);
  if (reverb_options & OPT_CHORUS_VOICE || (XG_System_chorus_type >> 3) == 3)
				         clone_voice(ip, i, e, CHORUS_CLONE, variationbank);
  if (reverb_options & OPT_REVERB_VOICE) clone_voice(ip, i, e, REVERB_CLONE, variationbank);
  ctl->note(i);
  played_notes++;
}

/* changed from VOICE_DIE: the trouble with ramping out dying
   voices in mix.c is that the dying time is just a coincidence
   of how near a buffer end we happen to be.
*/
static void kill_note(int i)
{
  voice[i].status=VOICE_DIE;
  if (voice[i].clone_voice >= 0)
	voice[ voice[i].clone_voice ].status=VOICE_DIE;
  ctl->note(i);
}

static void reduce_polyphony_by_one()
{
  int i=voices, lowest=-1; 
  int32 lv=0x7FFFFFFF, v;

  /* Look for the decaying note with the lowest volume */
  i=voices;
  while (i--)
    {
      if (voice[i].status==VOICE_FREE || !voice[i].sample->sample_rate) /* idea from Eric Welsh */
	    continue;
      if (voice[i].status & ~(VOICE_OFF | VOICE_DIE))
	{
	  v=voice[i].left_mix;
	  if ((voice[i].panned==PANNED_MYSTERY) && (voice[i].right_mix>v))
	    v=voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }

  if (lowest != -1) {
	kill_note(i);
  }

}

static void reduce_polyphony(int by)
{
  while (by--) reduce_polyphony_by_one();
}

static int check_quality()
{
#ifdef QUALITY_DEBUG
  static int debug_count = 0;
#endif
  int obf = output_buffer_full, retvalue = 1;

#ifdef QUALITY_DEBUG
if (!debug_count) {
	if (dont_cspline) fprintf(stderr,"[%d-%d]", output_buffer_full, current_polyphony);
	else fprintf(stderr,"{%d-%d}", output_buffer_full, current_polyphony);
	debug_count = 30;
}
debug_count--;
#endif

#ifdef POLYPHONY_COUNT
  if (current_polyphony < future_polyphony * 2) obf /= 2;
#endif

  if (obf < 1) voice_reserve = (4*voices) / 5;
  else if (obf <  5) voice_reserve = 3*voices / 4;
  else if (obf < 10) voice_reserve = 2*voices / 3;
  else if (obf < 20) voice_reserve = voices / 2;
  else if (obf < 30) voice_reserve = voices / 3;
  else if (obf < 40) voice_reserve = voices / 4;
  else if (obf < 50) voice_reserve = voices / 5;
  else if (obf < 60) voice_reserve = voices / 6;
  /* else voice_reserve = 0; */
  else voice_reserve = voices / 10; /* to be able to find a stereo clone */

  if (!current_interpolation) dont_cspline = 1;
  else if (obf < 10) dont_cspline = 1;
  else if (obf > 40) dont_cspline = 0;

  if (obf < 5) dont_reverb = 1;
  else if (obf > 25) dont_reverb = 0;

  if (obf < 8) dont_chorus = 1;
  else if (obf > 60) dont_chorus = 0;

  if (opt_dry || obf < 6) dont_keep_looping = 1;
  else if (obf > 22) dont_keep_looping = 0;

/*
  if (obf < 20) dont_filter = 1;
  else if (obf > 80) dont_filter = 0;
*/
  if (obf < 60) {
	reduce_polyphony(voices/10);
  }
  if (obf < 50) {
	reduce_polyphony(voices/10);
  }
  if (obf < 40) {
	reduce_polyphony(voices/9);
  }
  if (obf < 30) {
	reduce_polyphony(voices/8);
  }
  if (obf < 20) {
	reduce_polyphony(voices/6);
  }
  if (obf < 10) {
	reduce_polyphony(voices/5);
  }
  if (obf < 8) {
	reduce_polyphony(voices/4);
  }
  if (obf < 5) {
	reduce_polyphony(voices/4);
  }
  if (obf < 4) {
	reduce_polyphony(voices/4);
  }
  if (obf < 2) {
	reduce_polyphony(voices/4);
	retvalue = 0;
  }

  if (command_cutoff_allowed) dont_filter_melodic = 0;
  else dont_filter_melodic = 1;

  dont_filter_drums = 0;

  return retvalue;
}

static void note_on(MidiEvent *e)
{
  int i=voices, lowest=-1; 
  int32 lv=0x7FFFFFFF, v;

  current_polyphony = 0;

  while (i--)
    {
      if (voice[i].status == VOICE_FREE)
	lowest=i; /* Can't get a lower volume than silence */
	else current_polyphony++;
    }

  if (!check_quality())
    {
      lost_notes++;
      return;
    }


  if (voices - current_polyphony <= voice_reserve)
    lowest = -1;

  if (lowest != -1)
    {
      /* Found a free voice. */
      start_note(e,lowest);
      return;
    }
 
  /* Look for the decaying note with the lowest volume */
  i=voices;
  while (i--)
    {
      if (voice[i].status & ~(VOICE_ON | VOICE_DIE | VOICE_FREE))
	{
	  v=voice[i].left_mix;
	  if ((voice[i].panned==PANNED_MYSTERY) && (voice[i].right_mix>v))
	    v=voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }
 
  /* Look for the decaying note with the lowest volume */
  if (lowest==-1)
   {
   i=voices;
   while (i--)
    {
      if ( (voice[i].status & ~(VOICE_ON | VOICE_DIE | VOICE_FREE)) &&
	  (!voice[i].clone_type))
	{
	  v=voice[i].left_mix;
	  if ((voice[i].panned==PANNED_MYSTERY) && (voice[i].right_mix>v))
	    v=voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }
   }

  if (lowest != -1)
    {
      int cl = voice[lowest].clone_voice;
      /* This can still cause a click, but if we had a free voice to
	 spare for ramping down this note, we wouldn't need to kill it
	 in the first place... Still, this needs to be fixed. Perhaps
	 we could use a reserve of voices to play dying notes only. */
      
      cut_notes++;
      voice[lowest].status=VOICE_FREE;
      if (cl >= 0) {
	/** if (voice[lowest].velocity == voice[cl].velocity) **/
	if (voice[cl].clone_type==STEREO_CLONE || (!voice[cl].clone_type && voice[lowest].clone_type==STEREO_CLONE))
	   voice[cl].status=VOICE_FREE;
	else if (voice[cl].clone_voice==lowest) voice[cl].clone_voice=-1;
      }
      ctl->note(lowest);
      start_note(e,lowest);
    }
  else
    lost_notes++;
}

static void finish_note(int i)
{
  if (voice[i].status & (VOICE_FREE | VOICE_DIE | VOICE_OFF)) return;

  if (voice[i].sample->modes & MODES_ENVELOPE)
    {
      /* We need to get the envelope out of Sustain stage */
     if (voice[i].envelope_stage < RELEASE)
      voice[i].envelope_stage=RELEASE;
      voice[i].status=VOICE_OFF;
      recompute_envelope(i);
      apply_envelope_to_amp(i);
      ctl->note(i);
    }
  else
    {
      /* Set status to OFF so resample_voice() will let this voice out
         of its loop, if any. In any case, this voice dies when it
         hits the end of its data (ofs>=data_length). */
      voice[i].status=VOICE_OFF;
      ctl->note(i);
    }
  { int v;
    if ( (v=voice[i].clone_voice) >= 0)
      {
	voice[i].clone_voice = -1;
        finish_note(v);
      }
  }
}

static void note_off(MidiEvent *e)
{
  int i=voices, v;
  while (i--)
    if (voice[i].status==VOICE_ON &&
	voice[i].channel==e->channel &&
	voice[i].note==e->a)
      {
	if (channel[e->channel].sustain)
	  {
	    voice[i].status=VOICE_SUSTAINED;
	    ctl->note(i);
    	    if ( (v=voice[i].clone_voice) >= 0)
	      {
		if (voice[v].status == VOICE_ON)
		  voice[v].status=VOICE_SUSTAINED;
	      }
	  }
	else
	  finish_note(i);
      }
}

/* Process the All Notes Off event */
static void all_notes_off(int c)
{
  int i=voices;
  ctl->cmsg(CMSG_INFO, VERB_DEBUG, "All notes off on channel %d", c);
  while (i--)
    if (voice[i].status==VOICE_ON &&
	voice[i].channel==c)
      {
	if (channel[c].sustain) 
	  {
	    voice[i].status=VOICE_SUSTAINED;
	    ctl->note(i);
	  }
	else
	  finish_note(i);
      }
}

/* Process the All Sounds Off event */
static void all_sounds_off(int c)
{
  int i=voices;
  while (i--)
    if (voice[i].channel==c && 
	voice[i].status != VOICE_FREE &&
	voice[i].status != VOICE_DIE)
      {
	kill_note(i);
      }
}

static void adjust_pressure(MidiEvent *e)
{
  int i=voices;
  while (i--)
    if (voice[i].status==VOICE_ON &&
	voice[i].channel==e->channel &&
	voice[i].note==e->a)
      {
	voice[i].velocity=e->b;
	recompute_amp(i);
	apply_envelope_to_amp(i);
      }
}

#ifdef tplus
static void adjust_channel_pressure(MidiEvent *e)
{
#if 0
    if(opt_channel_pressure)
    {
#endif
	int i=voices;
	int ch, pressure;

	ch = e->channel;
	pressure = e->a;
        while (i--)
	    if(voice[i].status == VOICE_ON && voice[i].channel == ch)
	    {
		voice[i].velocity = pressure;
		recompute_amp(i);
		apply_envelope_to_amp(i);
	    }
#if 0
    }
#endif
}
#endif

static void adjust_panning(int c)
{
  int i=voices;
  while (i--)
    if ((voice[i].channel==c) &&
	(voice[i].status & (VOICE_ON | VOICE_SUSTAINED)) )
      {
	/* if (voice[i].clone_voice >= 0) continue; */
	if (voice[i].clone_type != NOT_CLONE) continue;
	voice[i].panning=channel[c].panning;
	recompute_amp(i);
	apply_envelope_to_amp(i);
      }
}

static void drop_sustain(int c)
{
  int i=voices;
  while (i--)
    if ( (voice[i].status & VOICE_SUSTAINED) && voice[i].channel==c)
      finish_note(i);
}

static void adjust_pitchbend(int c)
{
  int i=voices;
  while (i--)
    if (voice[i].status!=VOICE_FREE && voice[i].channel==c)
      {
	recompute_freq(i);
      }
}

static void adjust_volume(int c)
{
  int i=voices;
  while (i--)
    if (voice[i].channel==c &&
	(voice[i].status==VOICE_ON || voice[i].status==VOICE_SUSTAINED))
      {
	recompute_amp(i);
	apply_envelope_to_amp(i);
	ctl->note(i);
      }
}

#ifdef tplus
static int32 midi_cnv_vib_rate(int rate)
{
    return (int32)((double)play_mode->rate * MODULATION_WHEEL_RATE
		   / (midi_time_table[rate] *
		      2.0 * VIBRATO_SAMPLE_INCREMENTS));
}

static int midi_cnv_vib_depth(int depth)
{
    return (int)(depth * VIBRATO_DEPTH_TUNING);
}

static int32 midi_cnv_vib_delay(int delay)
{
    return (int32)(midi_time_table[delay]);
}
#endif

static int xmp_epoch = -1;
static unsigned xxmp_epoch = 0;
static unsigned time_expired = 0;
static unsigned last_time_expired = 0;
#if !defined( _UNIXWARE ) && ! defined(__hpux__) && ! defined (sun) && ! defined(_SCO_DS) && ! defined (sgi)
extern int gettimeofday(struct timeval *, struct timezone *);
#endif
static struct timeval tv;
static struct timezone tz;
static void time_sync(uint32 resync, int dosync)
{
	unsigned jiffies;

	if (dosync) {
		last_time_expired = resync;
		xmp_epoch = -1;
		xxmp_epoch = 0; 
		time_expired = 0;
		/*return;*/
	}
	gettimeofday (&tv, &tz);
	if (xmp_epoch < 0) {
		xxmp_epoch = tv.tv_sec;
		xmp_epoch = tv.tv_usec;
	}
	jiffies = (tv.tv_sec - xxmp_epoch)*100 + (tv.tv_usec - xmp_epoch)/10000;
	time_expired = (jiffies * play_mode->rate)/100 + last_time_expired;
}

static int got_a_lyric = 0;

#define META_BUF_LEN 1024

static void show_markers(uint32 until_time, int dosync)
{
    static struct meta_text_type *meta;
    char buf[META_BUF_LEN];
    int i, j, len, lyriclen;

    if (!meta_text_list) return;

    if (dosync) {
	time_sync(until_time, 1);
	for (meta = meta_text_list; meta && meta->time < until_time; meta = meta->next) ;
	return;
    }

    if (!time_expired) meta = meta_text_list;

    time_sync(0, 0);

    buf[0] = '\0';
    len = 0;
    while (meta)
	if (meta->time <= time_expired) {
	  if (meta->type == 5) got_a_lyric = 1;
	  if (!got_a_lyric || meta->type == 5) {
	    lyriclen = strlen(meta->text);
	    if (len + lyriclen + 1 < META_BUF_LEN) {
	      len += lyriclen;
	      strcat(buf, meta->text);
	    }
	    if (!meta->next) strcat(buf, " \n");
	  }
	  meta = meta->next;
	}
	else break;
    len = strlen(buf);
    for (i = 0, j = 0; j < META_BUF_LEN && j < len; j++)
	if (buf[j] == '\n') {
	    buf[j] = '\0';
	    if (j - i > 0) ctl->cmsg(CMSG_LYRIC, VERB_ALWAYS, buf + i);
	    else ctl->cmsg(CMSG_LYRIC, VERB_ALWAYS, " ");
	    i = j + 1;
	}
    if (i < j) ctl->cmsg(CMSG_LYRIC, VERB_ALWAYS, "~%s", buf + i);
}

static void seek_forward(uint32 until_time)
{
  reset_voices();
  show_markers(until_time, 1);
  while (current_event->time < until_time)
    {
      switch(current_event->type)
	{
	  /* All notes stay off. Just handle the parameter changes. */

	case ME_PITCH_SENS:
	  channel[current_event->channel].pitchsens=
	    current_event->a;
	  channel[current_event->channel].pitchfactor=0;
	  break;
	  
	case ME_PITCHWHEEL:
	  channel[current_event->channel].pitchbend=
	    current_event->a + current_event->b * 128;
	  channel[current_event->channel].pitchfactor=0;
	  break;
#ifdef tplus
	    case ME_MODULATION_WHEEL:
	      channel[current_event->channel].modulation_wheel =
		    midi_cnv_vib_depth(current_event->a);
	      break;

            case ME_PORTAMENTO_TIME_MSB:
	      channel[current_event->channel].portamento_time_msb = current_event->a;
	      break;

            case ME_PORTAMENTO_TIME_LSB:
	      channel[current_event->channel].portamento_time_lsb = current_event->a;
	      break;

            case ME_PORTAMENTO:
	      channel[current_event->channel].portamento = (current_event->a >= 64);
	      break;

            case ME_FINE_TUNING:
	      channel[current_event->channel].tuning_lsb = current_event->a;
	      break;

            case ME_COARSE_TUNING:
	      channel[current_event->channel].tuning_msb = current_event->a;
	      break;

      	    case ME_CHANNEL_PRESSURE:
	      /*adjust_channel_pressure(current_event);*/
	      break;

#endif
	  
	case ME_MAINVOLUME:
	  channel[current_event->channel].volume=current_event->a;
	  break;
	  
	case ME_MASTERVOLUME:
	  adjust_master_volume(current_event->a + (current_event->b <<7));
	  break;
	  
	case ME_PAN:
	  channel[current_event->channel].panning=current_event->a;
	  break;
	      
	case ME_EXPRESSION:
	  channel[current_event->channel].expression=current_event->a;
	  break;
	  
	case ME_PROGRAM:
  	  if (channel[current_event->channel].kit)
	    /* Change drum set */
	    channel[current_event->channel].bank=current_event->a;
	  else
	    channel[current_event->channel].program=current_event->a;
	  break;

	case ME_SUSTAIN:
	  channel[current_event->channel].sustain=current_event->a;
	  break;

	case ME_REVERBERATION:
	 if (global_reverb > current_event->a) break;
#ifdef CHANNEL_EFFECT
	 if (opt_effect)
	  effect_ctrl_change( current_event ) ;
	 else
#endif
	  channel[current_event->channel].reverberation=current_event->a;
	  break;

	case ME_CHORUSDEPTH:
	 if (global_chorus > current_event->a) break;
#ifdef CHANNEL_EFFECT
	 if (opt_effect)
	  effect_ctrl_change( current_event ) ;
	 else
#endif
	  channel[current_event->channel].chorusdepth=current_event->a;
	  break;

#ifdef CHANNEL_EFFECT
	case ME_CELESTE:
	 if (opt_effect)
	  effect_ctrl_change( current_event ) ;
	  break;
	case ME_PHASER:
	 if (opt_effect)
	  effect_ctrl_change( current_event ) ;
	  break;
#endif

	case ME_HARMONICCONTENT:
	  channel[current_event->channel].harmoniccontent=current_event->a;
	  break;

	case ME_RELEASETIME:
	  channel[current_event->channel].releasetime=current_event->a;
	  break;

	case ME_ATTACKTIME:
	  channel[current_event->channel].attacktime=current_event->a;
	  break;
#ifdef tplus
	case ME_VIBRATO_RATE:
	  channel[current_event->channel].vibrato_ratio=midi_cnv_vib_rate(current_event->a);
	  break;
	case ME_VIBRATO_DEPTH:
	  channel[current_event->channel].vibrato_depth=midi_cnv_vib_depth(current_event->a);
	  break;
	case ME_VIBRATO_DELAY:
	  channel[current_event->channel].vibrato_delay=midi_cnv_vib_delay(current_event->a);
	  break;
#endif
	case ME_BRIGHTNESS:
	  channel[current_event->channel].brightness=current_event->a;
	  break;

	case ME_RESET_CONTROLLERS:
	  reset_controllers(current_event->channel);
	  break;
	      
	case ME_TONE_BANK:
	  channel[current_event->channel].bank=current_event->a;
	  break;
	  
	case ME_TONE_KIT:
	  if (current_event->a==SFX_BANKTYPE)
		{
		    channel[current_event->channel].sfx=SFXBANK;
		    channel[current_event->channel].kit=0;
		}
	  else
		{
		    channel[current_event->channel].sfx=0;
		    channel[current_event->channel].kit=current_event->a;
		}
	  break;
	  
	case ME_EOT:
	  current_sample=current_event->time;
	  return;
	}
      current_event++;
    }
  /*current_sample=current_event->time;*/
  if (current_event != event_list)
    current_event--;
  current_sample=until_time;
}

int skip_to(uint32 until_time)
{
  if (current_sample > until_time)
    current_sample=0;

  if (!event_list) return 0;

  reset_midi();
  buffered_count=0;
  buffer_pointer=common_buffer;
  current_event=event_list;

  if (until_time)
    seek_forward(until_time);
  else show_markers(until_time, 1);
  ctl->reset();
  return 1;
}


static int apply_controls(void)
{
  int rc, i, did_skip=0;
  int32 val;
  /* ASCII renditions of CD player pictograms indicate approximate effect */
  do
    switch(rc=ctl->read(&val))
      {
      case RC_QUIT: /* [] */
      case RC_LOAD_FILE:	  
      case RC_NEXT: /* >>| */
      case RC_REALLY_PREVIOUS: /* |<< */
      case RC_PATCHCHANGE:
      case RC_CHANGE_VOICES:
      case RC_STOP:
	play_mode->purge_output();
	return rc;
	
      case RC_CHANGE_VOLUME:
	if (val>0 || amplification > -val)
	  amplification += val;
	else 
	  amplification=0;
	if (amplification > MAX_AMPLIFICATION)
	  amplification=MAX_AMPLIFICATION;
	adjust_amplification();
	for (i=0; i<voices; i++)
	  if (voice[i].status != VOICE_FREE)
	    {
	      recompute_amp(i);
	      apply_envelope_to_amp(i);
	    }
	ctl->master_volume(amplification);
	break;

      case RC_PREVIOUS: /* |<< */
	play_mode->purge_output();
	if (current_sample < 2*play_mode->rate)
	  return RC_REALLY_PREVIOUS;
	return RC_RESTART;

      case RC_RESTART: /* |<< */
	skip_to(0);
	play_mode->purge_output();
	did_skip=1;
	break;
	
      case RC_JUMP:
	play_mode->purge_output();
	if (val >= (int32)sample_count)
	  return RC_NEXT;
	skip_to((uint32)val);
	return rc;
	
      case RC_FORWARD: /* >> */
	/*play_mode->purge_output();*/
	if (val+current_sample >= sample_count)
	  return RC_NEXT;
	skip_to(val+current_sample);
	did_skip=1;
	break;
	
      case RC_BACK: /* << */
	/*play_mode->purge_output();*/
	if (current_sample > (uint32)val)
	  skip_to(current_sample-(uint32)val);
	else
	  skip_to(0); /* We can't seek to end of previous song. */
	did_skip=1;
	break;
      }
  while (rc!= RC_NONE);

  /* Advertise the skip so that we stop computing the audio buffer */
  if (did_skip)
    return RC_JUMP; 
  else
    return rc;
}

#ifdef CHANNEL_EFFECT
extern void (*do_compute_data)(uint32) ;
#else
static void do_compute_data(uint32 count)
{
  int i;
  if (!count) return; /* (gl) */
  memset(buffer_pointer, 0, 
	 (play_mode->encoding & PE_MONO) ? (count * 4) : (count * 8));
  for (i=0; i<voices; i++)
    {
      if(voice[i].status != VOICE_FREE)
	{
	  if (!voice[i].sample_offset && voice[i].echo_delay_count)
	    {
		if (voice[i].echo_delay_count >= count) voice[i].echo_delay_count -= count;
		else
		  {
	            mix_voice(buffer_pointer+voice[i].echo_delay_count, i, count-voice[i].echo_delay_count);
		    voice[i].echo_delay_count = 0;
		  }
	    }
	  else mix_voice(buffer_pointer, i, count);
	}
    }
  current_sample += count;
}
#endif /*CHANNEL_EFFECT*/

static int super_buffer_count = 0;

/* count=0 means flush remaining buffered data to output device, then
   flush the device itself */
static int compute_data(uint32 count)
{
  int rc;
  super_buffer_count = 0;
  if (!count)
    {
      if (buffered_count)
	play_mode->output_data(common_buffer, buffered_count);
      play_mode->flush_output();
      buffer_pointer=common_buffer;
      buffered_count=0;
      return RC_NONE;
    }

  while ((count+buffered_count) >= AUDIO_BUFFER_SIZE)
    {
      if (bbcount + AUDIO_BUFFER_SIZE * 2 >= BB_SIZE/2) {
	      super_buffer_count = count;
	      return 0;
      }
      if (flushing_output_device && bbcount >= output_fragsize * 2) {
	      super_buffer_count = count;
	      return 0;
      }
      do_compute_data(AUDIO_BUFFER_SIZE-buffered_count);
      count -= AUDIO_BUFFER_SIZE-buffered_count;
      play_mode->output_data(common_buffer, AUDIO_BUFFER_SIZE);
      buffer_pointer=common_buffer;
      buffered_count=0;
      
      ctl->current_time(current_sample);
      show_markers(0, 0);
      if ((rc=apply_controls())!=RC_NONE)
	return rc;
    }
  if (count>0)
    {
      do_compute_data(count);
      buffered_count += count;
      buffer_pointer += (play_mode->encoding & PE_MONO) ? count : count*2;
    }
  return RC_NONE;
}

#ifdef tplus
static void update_modulation_wheel(int ch, uint32 val)
{
    int i, uv = /*upper_*/voices;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE && voice[i].channel == ch && voice[i].modulation_wheel != val)
	{
	    /* Set/Reset mod-wheel */
	    voice[i].modulation_wheel = val;
	    voice[i].vibrato_delay = 0;
	    recompute_freq(i);
	}
}

static void drop_portamento(int ch)
{
    int i, uv = /*upper_*/voices;

    channel[ch].porta_control_ratio = 0;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_control_ratio)
	{
	    voice[i].porta_control_ratio = 0;
	    recompute_freq(i);
	}
    channel[ch].last_note_fine = -1;
}

static void update_portamento_controls(int ch)
{
    if(!channel[ch].portamento ||
       (channel[ch].portamento_time_msb | channel[ch].portamento_time_lsb)
       == 0)
	drop_portamento(ch);
    else
    {
	double mt, dc;
	int d;

	mt = midi_time_table[channel[ch].portamento_time_msb & 0x7F] *
	    midi_time_table2[channel[ch].portamento_time_lsb & 0x7F] *
		PORTAMENTO_TIME_TUNING;
	dc = play_mode->rate * mt;
	d = (int)(1.0 / (mt * PORTAMENTO_CONTROL_RATIO));
	d++;
	channel[ch].porta_control_ratio = (int)(d * dc + 0.5);
	channel[ch].porta_dpb = d;
    }
}

static void update_portamento_time(int ch)
{
    int i, uv = /*upper_*/voices;
    int dpb;
    int32 ratio;

    update_portamento_controls(ch);
    dpb = channel[ch].porta_dpb;
    ratio = channel[ch].porta_control_ratio;

    for(i = 0; i < uv; i++)
    {
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_control_ratio)
	{
	    voice[i].porta_control_ratio = ratio;
	    voice[i].porta_dpb = dpb;
	    recompute_freq(i);
	}
    }
}

static void update_channel_freq(int ch)
{
    int i, uv = /*upper_*/voices;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE && voice[i].channel == ch)
	    recompute_freq(i);
}
#endif

static uint32 event_count = 0;

int play_midi(MidiEvent *eventlist, uint32 events, uint32 samples)
{

  adjust_amplification();

  sample_count=samples;
  event_list=eventlist;
  event_count = events;
  lost_notes=cut_notes=played_notes=output_clips=0;
  skip_to(0);
  return 0;
}


int play_some_midi()
{
  int rc;

  for (;;)
    {
      if (super_buffer_count) {
	      compute_data(super_buffer_count);
	      return 0;
      }
      if (flushing_output_device && bbcount >= output_fragsize) {
	      return 0;
      }
      if (flushing_output_device) {
	      compute_data(0);
	      return RC_TUNE_END;
      }

      /* Handle all events that should happen at this time */
      while (current_event->time <= current_sample)
	{
	#ifdef POLYPHONY_COUNT
	  future_polyphony = current_event->polyphony;
	#endif
	  switch(current_event->type)
	    {

	      /* Effects affecting a single note */

	    case ME_NOTEON:

#ifdef tplus
#if 0
	      if (channel[current_event->channel].portamento &&
		    current_event->b &&
			(channel[current_event->channel].portamento_time_msb|
			 channel[current_event->channel].portamento_time_lsb )) break;
#endif
#endif
	      current_event->a += channel[current_event->channel].transpose;
	      if (!(current_event->b)) /* Velocity 0? */
		note_off(current_event);
	      else
		note_on(current_event);
	      break;

	    case ME_NOTEOFF:
	      current_event->a += channel[current_event->channel].transpose;
	      note_off(current_event);
	      break;

	    case ME_KEYPRESSURE:
	      adjust_pressure(current_event);
	      break;

	      /* Effects affecting a single channel */

	    case ME_PITCH_SENS:
	      channel[current_event->channel].pitchsens=
		current_event->a;
	      channel[current_event->channel].pitchfactor=0;
	      break;
	      
	    case ME_PITCHWHEEL:
	      channel[current_event->channel].pitchbend=
		current_event->a + current_event->b * 128;
	      channel[current_event->channel].pitchfactor=0;
	      /* Adjust pitch for notes already playing */
	      adjust_pitchbend(current_event->channel);
	      ctl->pitch_bend(current_event->channel, 
			      channel[current_event->channel].pitchbend);
	      break;
#ifdef tplus
	    case ME_MODULATION_WHEEL:
	      channel[current_event->channel].modulation_wheel =
		    midi_cnv_vib_depth(current_event->a);
	      update_modulation_wheel(current_event->channel, channel[current_event->channel].modulation_wheel);
		/*ctl_mode_event(CTLE_MOD_WHEEL, 1, current_event->channel, channel[current_event->channel].modulation_wheel);*/
		/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~m%u", midi_cnv_vib_depth(current_event->a));*/
	      break;

            case ME_PORTAMENTO_TIME_MSB:
	      channel[current_event->channel].portamento_time_msb = current_event->a;
	      update_portamento_time(current_event->channel);
	      break;

            case ME_PORTAMENTO_TIME_LSB:
	      channel[current_event->channel].portamento_time_lsb = current_event->a;
	      update_portamento_time(current_event->channel);
	      break;

            case ME_PORTAMENTO:
	      channel[current_event->channel].portamento = (current_event->a >= 64);
	      if(!channel[current_event->channel].portamento) drop_portamento(current_event->channel);
		/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~P%d",current_event->a);*/
	      break;

            case ME_FINE_TUNING:
	      channel[current_event->channel].tuning_lsb = current_event->a;
		/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~t");*/
	      break;

            case ME_COARSE_TUNING:
	      channel[current_event->channel].tuning_msb = current_event->a;
		/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~T");*/
	      break;

      	    case ME_CHANNEL_PRESSURE:
	      adjust_channel_pressure(current_event);
	      break;

#endif
	    case ME_MAINVOLUME:
	      channel[current_event->channel].volume=current_event->a;
	      adjust_volume(current_event->channel);
	      ctl->volume(current_event->channel, current_event->a);
	      break;
	      
	    case ME_MASTERVOLUME:
	      adjust_master_volume(current_event->a + (current_event->b <<7));
	      break;
	      
	    case ME_REVERBERATION:
	 if (global_reverb > current_event->a) break;
#ifdef CHANNEL_EFFECT
	 if (opt_effect)
	      effect_ctrl_change( current_event ) ;
	 else
#endif
	      channel[current_event->channel].reverberation=current_event->a;
	      break;

	    case ME_CHORUSDEPTH:
	 if (global_chorus > current_event->a) break;
#ifdef CHANNEL_EFFECT
	 if (opt_effect)
	      effect_ctrl_change( current_event ) ;
	 else
#endif
	      channel[current_event->channel].chorusdepth=current_event->a;
	      break;

#ifdef CHANNEL_EFFECT
	case ME_CELESTE:
	 if (opt_effect)
	  effect_ctrl_change( current_event ) ;
	  break;
	case ME_PHASER:
	 if (opt_effect)
	  effect_ctrl_change( current_event ) ;
	  break;
#endif

	    case ME_PAN:
	      channel[current_event->channel].panning=current_event->a;
	      if (adjust_panning_immediately)
		adjust_panning(current_event->channel);
	      ctl->panning(current_event->channel, current_event->a);
	      break;
	      
	    case ME_EXPRESSION:
	      channel[current_event->channel].expression=current_event->a;
	      adjust_volume(current_event->channel);
	      ctl->expression(current_event->channel, current_event->a);
	      break;

	    case ME_PROGRAM:
  	      if (channel[current_event->channel].kit)
		{
		  /* Change drum set */
		  /* if (channel[current_event->channel].kit==126)
		  	channel[current_event->channel].bank=57;
		  else */
		  channel[current_event->channel].bank=current_event->a;
		/*
	          ctl->program(current_event->channel, current_event->a,
			channel[current_event->channel].name);
		*/
	          ctl->program(current_event->channel, current_event->a,
      			drumset[channel[current_event->channel].bank]->name);
		}
	      else
		{
		  channel[current_event->channel].program=current_event->a;
	          ctl->program(current_event->channel, current_event->a,
      			tonebank[channel[current_event->channel].bank]->
				tone[current_event->a].name);
		}
	      break;

	    case ME_SUSTAIN:
	      channel[current_event->channel].sustain=current_event->a;
	      if (!current_event->a)
		drop_sustain(current_event->channel);
	      ctl->sustain(current_event->channel, current_event->a);
	      break;
	      
	    case ME_RESET_CONTROLLERS:
	      reset_controllers(current_event->channel);
	      redraw_controllers(current_event->channel);
	      break;

	    case ME_ALL_NOTES_OFF:
	      all_notes_off(current_event->channel);
	      break;
	      
	    case ME_ALL_SOUNDS_OFF:
	      all_sounds_off(current_event->channel);
	      break;

	    case ME_HARMONICCONTENT:
	      channel[current_event->channel].harmoniccontent=current_event->a;
	      break;

	    case ME_RELEASETIME:
	      channel[current_event->channel].releasetime=current_event->a;
/*
if (current_event->a != 64)
printf("release time %d on channel %d\n", channel[current_event->channel].releasetime,
current_event->channel);
*/
	      break;

	    case ME_ATTACKTIME:
	      channel[current_event->channel].attacktime=current_event->a;
/*
if (current_event->a != 64)
printf("attack time %d on channel %d\n", channel[current_event->channel].attacktime,
current_event->channel);
*/
	      break;
#ifdef tplus
	case ME_VIBRATO_RATE:
	  channel[current_event->channel].vibrato_ratio=midi_cnv_vib_rate(current_event->a);
	  update_channel_freq(current_event->channel);
	  break;
	case ME_VIBRATO_DEPTH:
	  channel[current_event->channel].vibrato_depth=midi_cnv_vib_depth(current_event->a);
	  update_channel_freq(current_event->channel);
	/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~v%u", midi_cnv_vib_depth(current_event->a));*/
	  break;
	case ME_VIBRATO_DELAY:
	  channel[current_event->channel].vibrato_delay=midi_cnv_vib_delay(current_event->a);
	  update_channel_freq(current_event->channel);
	  break;
#endif

	    case ME_BRIGHTNESS:
	      channel[current_event->channel].brightness=current_event->a;
	      break;
	      
	    case ME_TONE_BANK:
	      channel[current_event->channel].bank=current_event->a;
	      break;

	    case ME_TONE_KIT:
	      if (current_event->a==SFX_BANKTYPE)
		{
		    channel[current_event->channel].sfx=SFXBANK;
		    channel[current_event->channel].kit=0;
		}
	      else
		{
		    channel[current_event->channel].sfx=0;
		    channel[current_event->channel].kit=current_event->a;
		}
	      break;

	    case ME_EOT:
	      /*
	  fprintf(stderr,"midi end of tune\n");
	       */
	  flushing_output_device = TRUE;
	      /* Give the last notes a couple of seconds to decay  */
	      compute_data(play_mode->rate * 2);
	      /*
	  fprintf(stderr,"filled 2 sec\n");
	       */
	      if (!super_buffer_count) compute_data(0); /* flush buffer to device */
	      /*
	  fprintf(stderr,"flushed\n");
	       */
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Playing time: ~%d seconds",
		   current_sample/play_mode->rate+2);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Notes played: %d", played_notes);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Samples clipped: %d", output_clips);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Notes cut: %d", cut_notes);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Notes lost totally: %d", lost_notes);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Current memory used: %d", current_patch_memory);
	      /*
	  fprintf(stderr,"end of tune\n");
	       */
	      if (super_buffer_count) return 0;
	      return RC_TUNE_END;
	    }
	  current_event++;
	  event_count--;
	}

      rc=compute_data(current_event->time - current_sample);

/*
fprintf(stderr,"current_sample=%d current_event=%x, %ld left, bbcount=%d of %d\n",
  	current_sample, current_event, event_count, bbcount, BB_SIZE);
*/
/*fprintf(stderr,"Z\n");*/

      ctl->refresh();
      if ( (rc!=RC_NONE) && (rc!=RC_JUMP)) {
	  fprintf(stderr,"play_some returning %d\n", rc);
	  return rc;
      }
      if (bbcount > output_fragsize && !flushing_output_device && current_sample < sample_count) return 0;
    }
}



int play_midi_file(const char *fn)
{
  MidiEvent *event;
  uint32 events, samples;
  int rc;
  int32 val;
  FILE *fp;

  ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MIDI file: %s", fn);

  if (!strcmp(fn, "-"))
    {
      fp=stdin;
      strcpy(current_filename, "(stdin)");
    }
  else if (!(fp=open_file(fn, 1, OF_VERBOSE, 0)))
    return RC_ERROR;

  ctl->file_name(current_filename);

  event=read_midi_file(fp, &events, &samples);

  if (fp != stdin)
      close_file(fp);
  
  if (!event)
      return RC_ERROR;

  ctl->cmsg(CMSG_INFO, VERB_NOISY, 
	    "%d supported events, %d samples", events, samples);

  ctl->total_time(samples);
  ctl->master_volume(amplification);

  load_missing_instruments();
  if (check_for_rc()) return ctl->read(&val);
#ifdef tplus
  dont_cspline = 0;
#endif
  if (command_cutoff_allowed) dont_filter_melodic = 0;
  else dont_filter_melodic = 1;

  got_a_lyric = 0;
#ifdef POLYPHONY_COUNT
  future_polyphony = 0;
#endif
  rc=play_midi(event, events, samples);
/*
  if (free_instruments_afterwards)
      free_instruments();
  
  free(event);
*/
  return rc;
}

void play_midi_finish()
{
  compute_data(0);
  if (free_instruments_afterwards)
      free_instruments();
  if (event_list) free(event_list);
  event_list = 0;
  sample_count = 0;
}

void dumb_pass_playing_list(int number_of_files, const char *list_of_files[])
{
    int i=0;

    if (number_of_files == 0) return;

    for (;;)
	{
	  switch(play_midi_file(list_of_files[i]))
	    {
	    case RC_REALLY_PREVIOUS:
		if (i>0)
		    i--;
		break;

	    case RC_PATCHCHANGE:
	  	free_instruments();
	  	end_soundfont();
	  	clear_config();
			/* what if read_config fails?? */
          	if (read_config_file(current_config_file, 0))
  		  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Could not read patchset %d", cfg_select);
		break;
			
	    default: /* An error or something */
	    case RC_NEXT:
		if (i<number_of_files-1)
		    {
			i++;
			break;
		    }
		/* else fall through */
		
	    case RC_QUIT:
		play_mode->close_output();
		ctl->close();
		return;
	    }
	}
}
