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
#include <unistd.h>
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
#include "effects.h"
#include "md.h"
#include "readmidi.h"
#include <sys/time.h>
#include "output.h"
#include "mix.h"
#include "controls.h"
#include "resample.h"

#include "tables.h"

/*<95GUI_MODIF_BEGIN>*/
#ifdef CHANNEL_EFFECT
extern void effect_ctrl_change( MidiEvent* pCurrentEvent, struct md *d );
extern void effect_ctrl_reset( int idChannel, struct md *d ) ;
int opt_effect = FALSE;
int opt_effect_reverb = FALSE;
#endif /* CHANNEL_EFFECT */
/*<95GUI_MODIF_END>*/

#ifdef tplus
#define PORTAMENTO_TIME_TUNING		(1.0 / 5000.0)
#define PORTAMENTO_CONTROL_RATIO	256	/* controls per sec */
#endif

int opt_dry = 0;
int opt_expression_curve = 1;
int opt_volume_curve = 1;
int opt_stereo_surround = 1;
int dont_filter_melodic=1;
int dont_filter_drums=1;
int config_interpolation=DO_CSPLINE_INTERPOLATION;

int32 control_ratio=0;

static int32 /*lost_notes,*/ cut_notes, played_notes;

#ifdef tplus
static void update_portamento_controls(int ch, struct md *d);
#endif

static void kill_note(int i, struct md *d);

static void adjust_amplification(struct md *d)
{ 
  d->master_volume = (double)(d->amplification) / 100.0L;
  d->master_volume /= 4;
}

static void adjust_master_volume(int32 vol, struct md *d)
{ 
  d->master_volume = (double)(vol*d->amplification) / 1638400.0L;
  d->master_volume /= 4;
}

static void reset_voices(struct md *d)
{
  int i;
  for (i=0; i<MAX_VOICES; i++)
    d->voice[i].status=VOICE_FREE;
}

/* Process the Reset All Controllers event */
static void reset_controllers(int c, struct md *d)
{
  d->channel[c].volume=90; /* Some standard says, although the SCC docs say 0. */
  d->channel[c].expression=127; /* SCC-1 does this. */
  d->channel[c].sustain=0;
  d->channel[c].pitchbend=0x2000;
  d->channel[c].pitchfactor=0; /* to be computed */
  d->channel[c].reverberation = global_reverb;
  d->channel[c].chorusdepth = global_chorus;
#ifdef tplus
  d->channel[c].modulation_wheel = 0;
  d->channel[c].portamento_time_lsb = 0;
  d->channel[c].portamento_time_msb = 0;
  d->channel[c].tuning_lsb = 64;
  d->channel[c].tuning_msb = 64;
  d->channel[c].porta_control_ratio = 0;
  d->channel[c].portamento = 0;
  d->channel[c].last_note_fine = -1;

  d->channel[c].vibrato_ratio = 0;
  d->channel[c].vibrato_depth = 0;
  d->channel[c].vibrato_delay = 0;
/*
  memset(d->channel[c].envelope_rate, 0, sizeof(d->channel[c].envelope_rate));
*/
  update_portamento_controls(c, d);
#endif
#ifdef CHANNEL_EFFECT
  if (opt_effect) effect_ctrl_reset( c, d ) ;
#endif /* CHANNEL_EFFECT */
}

static void redraw_controllers(int c, struct md *d)
{
  ctl->volume(c, d->channel[c].volume);
  ctl->expression(c, d->channel[c].expression);
  ctl->sustain(c, d->channel[c].sustain);
  ctl->pitch_bend(c, d->channel[c].pitchbend);
}

static void reset_midi(struct md *d)
{
  int i;
  for (i=0; i<MAXCHAN; i++)
    {
      reset_controllers(i, d);
      /* The rest of these are unaffected by the Reset All Controllers event */
      d->channel[i].program=default_program;
      d->channel[i].panning=NO_PANNING;
      d->channel[i].pitchsens=2;
      d->channel[i].bank=0; /* tone bank or drum set */
      d->channel[i].harmoniccontent=64,
      d->channel[i].releasetime=64,
      d->channel[i].attacktime=64,
      d->channel[i].brightness=64,
      /*d->channel[i].kit=0;*/
      d->channel[i].sfx=0;
      /* d->channel[i].transpose and .kit are initialized in readmidi.c */
    }
  reset_voices(d);
}

static void select_sample(int v, Instrument *ip, struct md *d)
{
  int32 f, cdiff, diff, midfreq;
  int s,i;
  Sample *sp, *closest;

  s=ip->samples;
  sp=ip->sample;

  if (s==1)
    {
      d->voice[v].sample=sp;
      return;
    }

  f=d->voice[v].orig_frequency;
#if 0
/*
    Even if in range, a later patch may be closer. */

  for (i=0; i<s; i++)
    {
      if (sp->low_freq <= f && sp->high_freq >= f)
	{
	  d->voice[v].sample=sp;
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
  d->voice[v].sample=closest;
  return;
}

static void select_stereo_samples(int v, InstrumentLayer *lp, struct md *d)
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
	else if (d->voice[v].velocity < nlp->lo || d->voice[v].velocity > nlp->hi)
		diffvel = 200;
	else diffvel = d->voice[v].velocity - midvel;
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
    select_sample(v, ip, d);
    d->voice[v].right_sample = d->voice[v].sample;
  }
  else d->voice[v].right_sample = 0;

  ip->sample = ip->left_sample;
  ip->samples = ip->left_samples;
  select_sample(v, ip, d);
}

#ifndef tplus
static
#endif
void recompute_freq(int v, struct md *d)
{
  int 
    sign=(d->voice[v].sample_increment < 0), /* for bidirectional loops */
    pb=d->channel[d->voice[v].channel].pitchbend;
  double a;
  int32 tuning = 0;
  
  if (!d->voice[v].sample->sample_rate)
    return;

#ifdef tplus
#if 0
  if(!opt_modulation_wheel)
      d->voice[v].modulation_wheel = 0;
  if(!opt_portamento)
      d->voice[v].porta_control_ratio = 0;
#endif
  d->voice[v].vibrato_control_ratio = d->voice[v].orig_vibrato_control_ratio;
 
  if(d->voice[v].modulation_wheel > 0)
      {
	  d->voice[v].vibrato_control_ratio =
	      (int32)(play_mode->rate * MODULATION_WHEEL_RATE
		      / (2.0 * VIBRATO_SAMPLE_INCREMENTS));
	  d->voice[v].vibrato_delay = 0;
      }
#endif

#ifdef tplus
  if(d->voice[v].vibrato_control_ratio || d->voice[v].modulation_wheel > 0)
#else
  if (d->voice[v].vibrato_control_ratio)
#endif
    {
      /* This instrument has vibrato. Invalidate any precomputed
         sample_increments. */

      int i=VIBRATO_SAMPLE_INCREMENTS;
      while (i--)
	d->voice[v].vibrato_sample_increment[i]=0;
#ifdef tplus29
      if(d->voice[v].modulation_wheel > 0)
      {
	  d->voice[v].vibrato_control_ratio =
	      (int32)(play_mode->rate * MODULATION_WHEEL_RATE
		      / (2.0 * VIBRATO_SAMPLE_INCREMENTS));
	  d->voice[v].vibrato_delay = 0;
      }
#endif
    }


#ifdef tplus
  /* fine: [0..128] => [-256..256]
   * 1 coarse = 256 fine (= 1 note)
   * 1 fine = 2^5 tuning
   */
  tuning = (((int32)d->channel[d->voice[v].channel].tuning_lsb - 0x40)
	    + 64 * ((int32)d->channel[d->voice[v].channel].tuning_msb - 0x40)) << 7;
#endif

#ifdef tplus
  if(!d->voice[v].porta_control_ratio)
  {
#endif
#ifdef tplus29
  if(tuning == 0 && pb == 0x2000)
#else
  if (pb==0x2000 || pb<0 || pb>0x3FFF)
#endif
    d->voice[v].frequency=d->voice[v].orig_frequency;
  else
    {
      pb-=0x2000;
      if (!(d->channel[d->voice[v].channel].pitchfactor))
	{
	  /* Damn. Somebody bent the pitch. */
	  int32 i=pb*d->channel[d->voice[v].channel].pitchsens + tuning;
#ifdef tplus29
tuning;
	      if(i >= 0)
		  d->channel[ch].pitchfactor =
		      bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F];
	      else
	      {
		  i = -i;
		  d->channel[ch].pitchfactor = 1.0 /
		      (bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F]);
	      }
#else
	  if (pb<0)
	    i=-i;
	  d->channel[d->voice[v].channel].pitchfactor=
	    bend_fine[(i>>5) & 0xFF] * bend_coarse[i>>13];
#endif
	}
#ifndef tplus29
      if (pb>0)
#endif
	d->voice[v].frequency=
	  (int32)(d->channel[d->voice[v].channel].pitchfactor *
		  (double)(d->voice[v].orig_frequency));
#ifndef tplus29
      else
	d->voice[v].frequency=
	  (int32)((double)(d->voice[v].orig_frequency) /
		  d->channel[d->voice[v].channel].pitchfactor);
#endif
    }
#ifdef tplus
  }
  else /* Portament */
  {
      int32 i;
      FLOAT_T pf;

      pb -= 0x2000;

      i = pb * d->channel[d->voice[v].channel].pitchsens + (d->voice[v].porta_pb << 5) + tuning;

      if(i >= 0)
	  pf = bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F];
      else
      {
	  i = -i;
	  pf = 1.0 / (bend_fine[(i>>5) & 0xFF] * bend_coarse[(i>>13) & 0x7F]);
      }
      d->voice[v].frequency = (int32)((double)(d->voice[v].orig_frequency) * pf);
      /*d->voice[v].cache = NULL;*/
  }
#endif

  a = FRSCALE(((double)(d->voice[v].sample->sample_rate) *
	      (double)(d->voice[v].frequency)) /
	     ((double)(d->voice[v].sample->root_freq) *
	      (double)(play_mode->rate)),
	     FRACTION_BITS);

  /* what to do if incr is 0? --gl */
  /* if (!a) a++; */
  a += 0.5;
  if ((int32)a < 1) a = 1;

  if (sign) 
    a = -a; /* need to preserve the loop direction */

  d->voice[v].sample_increment = (int32)(a);
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

static void recompute_amp(int v, struct md *d)
{
  int32 tempamp;
  int chan = d->voice[v].channel;
  int vol = d->channel[chan].volume;
  int expr = d->channel[chan].expression;
  int vel = vcurve[d->voice[v].velocity];
  FLOAT_T curved_expression, curved_volume;

  /* TODO: use fscale */

  if (d->channel[chan].kit)
   {
    int note = d->voice[v].sample->note_to_use;
    if (note>0 && d->drumvolume[chan][note]>=0) vol = d->drumvolume[chan][note];
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
      /*if (d->voice[v].panning > 60 && d->voice[v].panning < 68)*/
      if (d->voice[v].panning > 62 && d->voice[v].panning < 66)
	{
	  d->voice[v].panned=PANNED_CENTER;

	  d->voice[v].left_amp=
	    FRSCALENEG((double)(tempamp) * d->voice[v].volume * d->master_volume,
		      21);
	}
      else if (d->voice[v].panning<5)
	{
	  d->voice[v].panned = PANNED_LEFT;

	  d->voice[v].left_amp=
	    FRSCALENEG((double)(tempamp) * d->voice[v].volume * d->master_volume,
		      20);
	}
      else if (d->voice[v].panning>123)
	{
	  d->voice[v].panned = PANNED_RIGHT;

	  d->voice[v].left_amp= /* left_amp will be used */
	    FRSCALENEG((double)(tempamp) * d->voice[v].volume * d->master_volume,
		      20);
	}
      else
	{
	  d->voice[v].panned = PANNED_MYSTERY;

	  d->voice[v].left_amp=
	    FRSCALENEG((double)(tempamp) * d->voice[v].volume * d->master_volume,
		      27);
	  d->voice[v].right_amp=d->voice[v].left_amp * (d->voice[v].panning);
	  d->voice[v].left_amp *= (double)(127-d->voice[v].panning);
	}
    }
  else
    {
      d->voice[v].panned=PANNED_CENTER;

      d->voice[v].left_amp=
	FRSCALENEG((double)(tempamp) * d->voice[v].volume * d->master_volume,
		  21);
    }
}

#define NOT_CLONE 0
#define STEREO_CLONE 1
#define REVERB_CLONE 2
#define CHORUS_CLONE 3


/* just a variant of note_on() */
static int vc_alloc(int j, int clone_type, struct md *d)
{
  int i=d->voices; 
  int cpoly = 0, vc_ret = -1;

  while (i--)
    {
      if (i == j) continue;
      if (d->voice[i].status & VOICE_FREE) {
	vc_ret = i;
      }
      else if (d->voice[i].status & ~VOICE_DIE) cpoly++;
    }
  /*if (cpoly < d->voices - d->voice_reserve  || clone_type == STEREO_CLONE)*/ return vc_ret;
  return -1;
}

static void kill_others(MidiEvent *e, int i, struct md *d)
{
  int j=d->voices; 

  /*if (!d->voice[i].sample->exclusiveClass) return; */

  while (j--)
    {
      if (d->voice[j].status & (VOICE_FREE|VOICE_OFF|VOICE_DIE)) continue;
      if (i == j) continue;
      if (d->voice[i].channel != d->voice[j].channel) continue;
      if (d->voice[j].sample->note_to_use)
      {
    	/* if (d->voice[j].sample->note_to_use != d->voice[i].sample->note_to_use) continue; */
	if (!d->voice[i].sample->exclusiveClass) continue;
    	if (d->voice[j].sample->exclusiveClass != d->voice[i].sample->exclusiveClass) continue;
        kill_note(j, d);
      }
     /*
      else if (d->voice[j].note != (e->a &0x07f)) continue;
      kill_note(j);
      */
    }
}


static void clone_voice(Instrument *ip, int v, MidiEvent *e, uint8 clone_type, int variationbank, struct md *d)
{
  int w, k, played_note, chorus, reverb, milli;
  int chan = d->voice[v].channel;


  if (clone_type == STEREO_CLONE) {
	if (!d->voice[v].right_sample && variationbank != 3) return;
	if (variationbank == 6) return;
  }
  chorus = ip->sample->chorusdepth;
  reverb = ip->sample->reverberation;

  if (d->channel[chan].kit) {
	if ((k=d->drumreverberation[chan][d->voice[v].note]) >= 0) reverb = (reverb>k)? reverb : k;
	if ((k=d->drumchorusdepth[chan][d->voice[v].note]) >= 0) chorus = (chorus>k)? chorus : k;
  }
  else {
	if ((k=d->channel[chan].chorusdepth) >= 0) chorus = (chorus>k)? chorus : k;
	if ((k=d->channel[chan].reverberation) >= 0) reverb = (reverb>k)? reverb : k;
  }

  if (clone_type == REVERB_CLONE) chorus = 0;
  else if (clone_type == CHORUS_CLONE) reverb = 0;
  else if (clone_type == STEREO_CLONE) reverb = chorus = 0;

  if (reverb > 127) reverb = 127;
  if (chorus > 127) chorus = 127;

  if (clone_type == REVERB_CLONE) {
	 if ( (reverb_options & OPT_REVERB_EXTRA) && reverb < global_echo)
		reverb = global_echo;
	 if (/*reverb < 8 ||*/ d->dont_reverb) return;
  }
  if (clone_type == CHORUS_CLONE) {
	 if (variationbank == 32) chorus = 30;
	 else if (variationbank == 33) chorus = 60;
	 else if (variationbank == 34) chorus = 90;
	 if ( (reverb_options & OPT_CHORUS_EXTRA) && chorus < global_detune)
		chorus = global_detune;
	 if (/*chorus < 4 ||*/ d->dont_chorus) return;
  }

  if (!d->voice[v].right_sample) {
	d->voice[v].right_sample = d->voice[v].sample;
  }
  else if (clone_type == STEREO_CLONE) variationbank = 0;
	/* don't try to fake a second patch if we have a real one */

  if ( (w = vc_alloc(v, clone_type, d)) < 0 ) return;

  if (clone_type==STEREO_CLONE) d->voice[v].clone_voice = w;
  d->voice[w].clone_voice = v;
  d->voice[w].clone_type = clone_type;

  d->voice[w].status = d->voice[v].status;
  d->voice[w].channel = d->voice[v].channel;
  d->voice[w].note = d->voice[v].note;
  d->voice[w].sample = d->voice[v].right_sample;
  /*d->voice[w].velocity= (e->b * (127 - d->voice[w].sample->attenuation)) / 127;*/
  d->voice[w].velocity= e->b;
  d->voice[w].left_sample = d->voice[v].left_sample;
  d->voice[w].right_sample = d->voice[v].right_sample;
  d->voice[w].orig_frequency = d->voice[v].orig_frequency;
  d->voice[w].frequency = d->voice[v].frequency;
  d->voice[w].sample_offset = d->voice[v].sample_offset;
  d->voice[w].sample_increment = d->voice[v].sample_increment;
  d->voice[w].current_x0 =
    d->voice[w].current_x1 =
    d->voice[w].current_y0 =
    d->voice[w].current_y1 = 0;
  d->voice[w].echo_delay = d->voice[v].echo_delay;
  d->voice[w].starttime = d->voice[v].starttime;
  d->voice[w].envelope_volume = d->voice[v].envelope_volume;
  d->voice[w].envelope_target = d->voice[v].envelope_target;
  d->voice[w].envelope_increment = d->voice[v].envelope_increment;
  d->voice[w].modulation_volume = d->voice[v].modulation_volume;
  d->voice[w].modulation_target = d->voice[v].modulation_target;
  d->voice[w].modulation_increment = d->voice[v].modulation_increment;
  d->voice[w].tremolo_sweep = d->voice[w].sample->tremolo_sweep_increment;
  d->voice[w].tremolo_sweep_position = d->voice[v].tremolo_sweep_position;
  d->voice[w].tremolo_phase = d->voice[v].tremolo_phase;
  d->voice[w].tremolo_phase_increment = d->voice[w].sample->tremolo_phase_increment;
  d->voice[w].lfo_sweep = d->voice[w].sample->lfo_sweep_increment;
  d->voice[w].lfo_sweep_position = d->voice[v].lfo_sweep_position;
  d->voice[w].lfo_phase = d->voice[v].lfo_phase;
  d->voice[w].lfo_phase_increment = d->voice[w].sample->lfo_phase_increment;
  d->voice[w].modLfoToFilterFc=d->voice[w].sample->modLfoToFilterFc;
  d->voice[w].modEnvToFilterFc=d->voice[w].sample->modEnvToFilterFc;
  d->voice[w].vibrato_sweep = d->voice[w].sample->vibrato_sweep_increment;
  d->voice[w].vibrato_sweep_position = d->voice[v].vibrato_sweep_position;
  d->voice[w].left_mix = d->voice[v].left_mix;
  d->voice[w].right_mix = d->voice[v].right_mix;
  d->voice[w].left_amp = d->voice[v].left_amp;
  d->voice[w].right_amp = d->voice[v].right_amp;
  d->voice[w].tremolo_volume = d->voice[v].tremolo_volume;
  d->voice[w].lfo_volume = d->voice[v].lfo_volume;
  for (k = 0; k < VIBRATO_SAMPLE_INCREMENTS; k++)
    d->voice[w].vibrato_sample_increment[k] =
      d->voice[v].vibrato_sample_increment[k];
  for (k=ATTACK; k<MAXPOINT; k++)
    {
	d->voice[w].envelope_rate[k]=d->voice[v].envelope_rate[k];
	d->voice[w].envelope_offset[k]=d->voice[v].envelope_offset[k];
    }
  d->voice[w].vibrato_phase = d->voice[v].vibrato_phase;
  d->voice[w].vibrato_depth = d->voice[w].sample->vibrato_depth;
  d->voice[w].vibrato_control_ratio = d->voice[w].sample->vibrato_control_ratio;
  d->voice[w].vibrato_control_counter = d->voice[v].vibrato_control_counter;
  d->voice[w].envelope_stage = d->voice[v].envelope_stage;
  d->voice[w].modulation_stage = d->voice[v].modulation_stage;
  d->voice[w].control_counter = d->voice[v].control_counter;

  d->voice[w].panning = d->voice[v].panning;

  if (d->voice[w].right_sample)
  	d->voice[w].panning = d->voice[w].right_sample->panning;
  else d->voice[w].panning = 127;

  if (clone_type == STEREO_CLONE) {
    int left, right;
    int panrequest = d->voice[v].panning;
    if (variationbank == 3) {
	d->voice[v].panning = 0;
	d->voice[w].panning = 127;
    }
    else {
	if (d->voice[v].sample->panning > d->voice[w].sample->panning) {
	  left = w;
	  right = v;
	}
	else {
	  left = v;
	  right = w;
	}
	if (panrequest < 64) {
	  if (opt_stereo_surround) {
	    d->voice[left].panning = 0;
	    d->voice[right].panning = panrequest;
	  }
	  else {
	    if (d->voice[left].sample->panning < panrequest)
		d->voice[left].panning = d->voice[left].sample->panning;
	    else d->voice[left].panning = panrequest;
	    d->voice[right].panning = (d->voice[right].sample->panning + panrequest + 32) / 2;
	  }
	}
	else {
	  if (opt_stereo_surround) {
	    d->voice[left].panning = panrequest;
	    d->voice[right].panning = 127;
	  }
	  else {
	    if (d->voice[right].sample->panning > panrequest)
		d->voice[right].panning = d->voice[right].sample->panning;
	    else d->voice[right].panning = panrequest;
	    d->voice[left].panning = (d->voice[left].sample->panning + panrequest - 32) / 2;
	  }
	}
    }
#ifdef DEBUG_CLONE_NOTES
fprintf(stderr,"STEREO_CLONE v%d vol%f pan%d\n", w, d->voice[w].volume, d->voice[w].panning);
#endif
  }

#ifdef tplus
  d->voice[w].porta_control_ratio = d->voice[v].porta_control_ratio;
  d->voice[w].porta_dpb = d->voice[v].porta_dpb;
  d->voice[w].porta_pb = d->voice[v].porta_pb;
  /* ?? d->voice[w].vibrato_delay = 0; */
  d->voice[w].vibrato_delay = d->voice[w].sample->vibrato_delay;
  d->voice[w].modulation_delay = d->voice[w].sample->modulation_rate[DELAY];
#endif
  d->voice[w].volume = d->voice[w].sample->volume;

  milli = play_mode->rate/1000;

  if (reverb) {

#if 0
	if (d->voice[w].panning < 64) d->voice[w].panning = 127;
	else d->voice[w].panning = 0;
#endif
    if (opt_stereo_surround) {
	if (d->voice[w].panning > 64) d->voice[w].panning = 127;
	else d->voice[w].panning = 0;
    }
    else {
	if (d->voice[v].panning < 64) d->voice[w].panning = 64 + reverb/2;
	else d->voice[w].panning = 64 - reverb/2;
    }

#ifdef DEBUG_REVERBERATION
printf("r=%d vol %f", reverb, d->voice[w].volume);
#endif

/* try 98->99 for melodic instruments ? (bit much for percussion) */
	d->voice[w].volume *= d->vol_table[(127-reverb)/8 + 98];

#ifdef DEBUG_REVERBERATION
printf(" -> vol %f", d->voice[w].volume);

printf(" delay %d", d->voice[w].echo_delay);
#endif
	/*d->voice[w].echo_delay += (reverb>>1) * milli;*/
	d->voice[w].echo_delay += reverb * milli;
#ifdef DEBUG_REVERBERATION
printf(" -> delay %d\n", d->voice[w].echo_delay);
#endif


	d->voice[w].envelope_rate[DECAY] *= 2;
	d->voice[w].envelope_rate[RELEASE] /= 2;

	if (d->XG_System_reverb_type >= 0) {
	    int subtype = d->XG_System_reverb_type & 0x07;
	    int rtype = d->XG_System_reverb_type >>3;
	    switch (rtype) {
		case 0: /* no effect */
		  break;
		case 1: /* hall */
		  if (subtype) d->voice[w].echo_delay += 100 * milli;
		  break;
		case 2: /* room */
		  d->voice[w].echo_delay /= 2;
		  break;
		case 3: /* stage */
		  d->voice[w].velocity = d->voice[v].velocity;
		  break;
		case 4: /* plate */
		  d->voice[w].panning = d->voice[v].panning;
		  break;
		case 16: /* white room */
		  d->voice[w].echo_delay = 0;
		  break;
		case 17: /* tunnel */
		  d->voice[w].echo_delay *= 2;
		  d->voice[w].velocity /= 2;
		  break;
		case 18: /* canyon */
		  d->voice[w].echo_delay *= 2;
		  break;
		case 19: /* basement */
		  d->voice[w].velocity /= 2;
		  break;
	        default: break;
	    }
	}
#ifdef DEBUG_CLONE_NOTES
fprintf(stderr,"REVERB_CLONE v%d vol=%f pan=%d reverb=%d delay=%dms\n", w, d->voice[w].volume, d->voice[w].panning, reverb,
	d->voice[w].echo_delay / milli);
#endif
  }
  played_note = d->voice[w].sample->note_to_use;
  if (!played_note) {
	played_note = e->a & 0x7f;
	if (variationbank == 35) played_note += 12;
	else if (variationbank == 36) played_note -= 12;
	else if (variationbank == 37) played_note += 7;
	else if (variationbank == 36) played_note -= 7;
  }
    played_note = ( (played_note - d->voice[w].sample->freq_center) * d->voice[w].sample->freq_scale ) / 1024 +
		d->voice[w].sample->freq_center;
  d->voice[w].note = played_note;
  d->voice[w].orig_frequency = freq_table[played_note];

  if (chorus) {
	/*d->voice[w].orig_frequency += (d->voice[w].orig_frequency/128) * chorus;*/
/*fprintf(stderr, "voice %d v sweep from %ld (cr %d, depth %d)", w, d->voice[w].vibrato_sweep,
	 d->voice[w].vibrato_control_ratio, d->voice[w].vibrato_depth);*/

	if (opt_stereo_surround) {
	  if (d->voice[v].panning < 64) d->voice[w].panning = d->voice[v].panning + 32;
	  else d->voice[w].panning = d->voice[v].panning - 32;
	}

	if (!d->voice[w].vibrato_control_ratio) {
		d->voice[w].vibrato_control_ratio = 100;
		d->voice[w].vibrato_depth = 6;
		d->voice[w].vibrato_sweep = 74;
	}
	/*d->voice[w].velocity = (d->voice[w].velocity * chorus) / 128;*/
#if 0
	d->voice[w].velocity = (d->voice[w].velocity * chorus) / (128+96);
	d->voice[v].velocity = d->voice[w].velocity;
	d->voice[w].volume = (d->voice[w].volume * chorus) / (128+96);
	d->voice[v].volume = d->voice[w].volume;
#endif
	/* d->voice[w].volume *= 0.9; */
	d->voice[w].volume *= 0.40;
	d->voice[v].volume = d->voice[w].volume;
	recompute_amp(v, d);
        apply_envelope_to_amp(v, d);
	d->voice[w].vibrato_sweep = chorus/2;
	d->voice[w].vibrato_depth /= 2;
	if (!d->voice[w].vibrato_depth) d->voice[w].vibrato_depth = 2;
	d->voice[w].vibrato_control_ratio /= 2;
	/*d->voice[w].vibrato_control_ratio += chorus;*/
/*fprintf(stderr, " to %ld (cr %d, depth %d).\n", d->voice[w].vibrato_sweep,
	 d->voice[w].vibrato_control_ratio, d->voice[w].vibrato_depth);
	d->voice[w].vibrato_phase = 20;*/

	d->voice[w].echo_delay += 30 * milli;

	if (d->XG_System_chorus_type >= 0) {
	    int subtype = d->XG_System_chorus_type & 0x07;
	    int chtype = 0x0f & (d->XG_System_chorus_type >> 3);
	    switch (chtype) {
		case 0: /* no effect */
		  break;
		case 1: /* chorus */
		  chorus /= 3;
		  if(d->channel[ d->voice[w].channel ].pitchbend + chorus < 0x2000)
            		d->voice[w].orig_frequency =
				(uint32)( (FLOAT_T)d->voice[w].orig_frequency * bend_fine[chorus] );
        	  else d->voice[w].orig_frequency =
			(uint32)( (FLOAT_T)d->voice[w].orig_frequency / bend_fine[chorus] );
		  if (subtype) d->voice[w].vibrato_depth *= 2;
		  break;
		case 2: /* celeste */
		  d->voice[w].orig_frequency += (d->voice[w].orig_frequency/128) * chorus;
		  break;
		case 3: /* flanger */
		  d->voice[w].vibrato_control_ratio = 10;
		  d->voice[w].vibrato_depth = 100;
		  d->voice[w].vibrato_sweep = 8;
		  d->voice[w].echo_delay += 200 * milli;
		  break;
		case 4: /* symphonic : cf Children of the Night /128 bad, /1024 ok */
		  d->voice[w].orig_frequency += (d->voice[w].orig_frequency/512) * chorus;
		  d->voice[v].orig_frequency -= (d->voice[v].orig_frequency/512) * chorus;
		  recompute_freq(v, d);
		  break;
		case 8: /* phaser */
		  break;
	      default:
		  break;
	    }
	}
	else {
	    chorus /= 3;
	    if(d->channel[ d->voice[w].channel ].pitchbend + chorus < 0x2000)
          	d->voice[w].orig_frequency =
			(uint32)( (FLOAT_T)d->voice[w].orig_frequency * bend_fine[chorus] );
            else d->voice[w].orig_frequency =
		(uint32)( (FLOAT_T)d->voice[w].orig_frequency / bend_fine[chorus] );
	}
#ifdef DEBUG_CLONE_NOTES
fprintf(stderr,"CHORUS_CLONE v%d vol%f pan%d chorus%d\n", w, d->voice[w].volume, d->voice[w].panning, chorus);
#endif
  }
  d->voice[w].loop_start = d->voice[w].sample->loop_start;
  d->voice[w].loop_end = d->voice[w].sample->loop_end;
  d->voice[w].echo_delay_count = d->voice[w].echo_delay;
  if (reverb) d->voice[w].echo_delay *= 2;

  recompute_freq(w, d);
  recompute_amp(w, d);
  if (d->voice[w].sample->modes & MODES_ENVELOPE)
    {
      /* Ramp up from 0 */
      d->voice[w].envelope_stage=ATTACK;
      d->voice[w].modulation_stage=ATTACK;
      d->voice[w].envelope_volume=0;
      d->voice[w].modulation_volume=0;
      d->voice[w].control_counter=0;
      d->voice[w].modulation_counter=0;
      recompute_envelope(w, d);
      if (d->voice[w].sample->cutoff_freq)
      	d->voice[w].bw_index = 1 + (d->voice[w].sample->cutoff_freq+50) / 100;
      else d->voice[w].bw_index = 0;
      recompute_modulation(w, d);
      apply_envelope_to_amp(w, d);
    }
  else
    {
      d->voice[w].envelope_increment=0;
      d->voice[w].modulation_increment=0;
      apply_envelope_to_amp(w, d);
    }
}

static void xremap(int *banknumpt, int *this_notept, int this_kit) {
	int i, newmap;
	int banknum = *banknumpt;
	int this_note = *this_notept;
	int newbank, newnote;

	if (!this_kit) {
		if (banknum == SFXBANK && tonebank[120]) *banknumpt = 120;
		return;
	}

	if (this_kit != 127 && this_kit != 126) return;

	for (i = 0; i < XMAPMAX; i++) {
		newmap = xmap[i][0];
		if (!newmap) return;
		if (this_kit == 127 && newmap != XGDRUM) continue;
		if (this_kit == 126 && newmap != SFXDRUM1) continue;
		if (xmap[i][1] != banknum) continue;
		if (xmap[i][3] != this_note) continue;
		newbank = xmap[i][2];
		newnote = xmap[i][4];
		if (newbank == banknum && newnote == this_note) return;
		if (!drumset[newbank]) return;
		if (!drumset[newbank]->tone[newnote].layer) return;
		if (drumset[newbank]->tone[newnote].layer == MAGIC_LOAD_INSTRUMENT) return;
		*banknumpt = newbank;
		*this_notept = newnote;
		return;
	}
}

static void start_note(MidiEvent *e, int i, struct md *d)
{
  InstrumentLayer *lp;
  Instrument *ip;
  int j, banknum, ch=e->channel;
  int played_note, drumpan=NO_PANNING;
  int32 rt;
  int attacktime, releasetime, decaytime, variationbank;
  int brightness = d->channel[ch].brightness;
  int harmoniccontent = d->channel[ch].harmoniccontent;
  int this_note = e->a;
  int this_velocity = e->b;
  int drumsflag = d->channel[ch].kit;

  if (check_for_rc()) return;

  if (d->channel[ch].sfx) banknum=d->channel[ch].sfx;
  else banknum=d->channel[ch].bank;

  d->voice[i].velocity=this_velocity;

  if (d->XG_System_On) xremap(&banknum, &this_note, d->channel[ch].kit);

  if (current_config_pc42b) pcmap(&banknum, &this_note, &drumsflag);

  if (d->channel[ch].kit)
    {
      if (!(lp=drumset[banknum]->tone[this_note].layer))
	{
	  if (!(lp=drumset[0]->tone[this_note].layer))
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
	  d->voice[i].orig_frequency=freq_table[(int)(ip->sample->note_to_use)];
	  drumpan=d->drumpanpot[ch][(int)ip->sample->note_to_use];
	}
      else
	d->voice[i].orig_frequency=freq_table[this_note & 0x7F];
    }
  else
    {
      if (d->channel[ch].program==SPECIAL_PROGRAM)
	lp=default_instrument;
      else if (!(lp=tonebank[banknum]->tone[d->channel[ch].program].layer))
	{
	  if (!(lp=tonebank[0]->tone[d->channel[ch].program].layer))
	    return; /* No instrument? Then we can't play. */
	}
      ip = lp->instrument;

      if (ip->sample->note_to_use) /* Fixed-pitch instrument? */
	d->voice[i].orig_frequency=freq_table[(int)(ip->sample->note_to_use)];
      else
	d->voice[i].orig_frequency=freq_table[this_note & 0x7F];
    } /* not drum channel */

    select_stereo_samples(i, lp, d);
    kill_others(e, i, d);

    d->voice[i].starttime = e->time;
    played_note = d->voice[i].sample->note_to_use;

/* for non-percussion, always use the note in the music (may be sfx bank) */ 
    if (!played_note || !d->channel[ch].kit) played_note = this_note & 0x7f;
    played_note = ( (played_note - d->voice[i].sample->freq_center) * d->voice[i].sample->freq_scale ) / 1024 +
		d->voice[i].sample->freq_center;
    d->voice[i].note = played_note;
    d->voice[i].orig_frequency = freq_table[played_note];

  d->voice[i].status=VOICE_ON;
  d->voice[i].channel=ch;
/* Check this! */
  /*d->voice[i].velocity= (this_velocity * (127 - d->voice[i].sample->attenuation)) / 127;*/
  d->voice[i].velocity= this_velocity;
  d->voice[i].sample_offset=0;
  d->voice[i].sample_increment=0; /* make sure it isn't negative */
  /* why am I copying loop points? */
  d->voice[i].loop_start = d->voice[i].sample->loop_start;
  d->voice[i].loop_end = d->voice[i].sample->loop_end;
  d->voice[i].volume = d->voice[i].sample->volume;
  d->voice[i].current_x0 =
    d->voice[i].current_x1 =
    d->voice[i].current_y0 =
    d->voice[i].current_y1 = 0;
#ifdef tplus
  d->voice[i].modulation_wheel=d->channel[ch].modulation_wheel;
#endif

  d->voice[i].tremolo_phase=0;
  d->voice[i].tremolo_phase_increment=d->voice[i].sample->tremolo_phase_increment;
  d->voice[i].tremolo_sweep=d->voice[i].sample->tremolo_sweep_increment;
  d->voice[i].tremolo_sweep_position=0;

  d->voice[i].lfo_phase=0;
  d->voice[i].lfo_phase_increment=d->voice[i].sample->lfo_phase_increment;
  d->voice[i].lfo_sweep=d->voice[i].sample->lfo_sweep_increment;
  d->voice[i].lfo_sweep_position=0;
  d->voice[i].modLfoToFilterFc=d->voice[i].sample->modLfoToFilterFc;
  d->voice[i].modEnvToFilterFc=d->voice[i].sample->modEnvToFilterFc;
  d->voice[i].modulation_delay = d->voice[i].sample->modulation_rate[DELAY];

  d->voice[i].vibrato_sweep=d->voice[i].sample->vibrato_sweep_increment;
  d->voice[i].vibrato_depth=d->voice[i].sample->vibrato_depth;
  d->voice[i].vibrato_sweep_position=0;
  d->voice[i].vibrato_control_ratio=d->voice[i].sample->vibrato_control_ratio;
  d->voice[i].vibrato_delay = d->voice[i].sample->vibrato_delay;
#ifdef tplus
  if(d->channel[ch].vibrato_ratio && d->voice[i].vibrato_depth > 0)
  {
      d->voice[i].vibrato_control_ratio = d->channel[ch].vibrato_ratio;
      d->voice[i].vibrato_depth = d->channel[ch].vibrato_depth;
      d->voice[i].vibrato_delay = d->channel[ch].vibrato_delay;
  }
  else d->voice[i].vibrato_delay = 0;
#endif
  d->voice[i].vibrato_control_counter=d->voice[i].vibrato_phase=0;
  for (j=0; j<VIBRATO_SAMPLE_INCREMENTS; j++)
    d->voice[i].vibrato_sample_increment[j]=0;

  attacktime = d->channel[ch].attacktime;
  releasetime = d->channel[ch].releasetime;
  decaytime = 64;
  variationbank = d->channel[ch].variationbank;



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
		d->voice[i].modEnvToFilterFc=2.0;
      		d->voice[i].sample->cutoff_freq = 800;
		break;
	case 25:
		d->voice[i].modEnvToFilterFc=-2.0;
      		d->voice[i].sample->cutoff_freq = 800;
		break;
	case 27:
		d->voice[i].modLfoToFilterFc=2.0;
		d->voice[i].lfo_phase_increment=109;
		d->voice[i].lfo_sweep=122;
      		d->voice[i].sample->cutoff_freq = 800;
		break;
	case 28:
		d->voice[i].modLfoToFilterFc=-2.0;
		d->voice[i].lfo_phase_increment=109;
		d->voice[i].lfo_sweep=122;
      		d->voice[i].sample->cutoff_freq = 800;
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
	d->voice[i].envelope_rate[j]=d->voice[i].sample->envelope_rate[j];
	d->voice[i].envelope_offset[j]=d->voice[i].sample->envelope_offset[j];
#ifdef RATE_ADJUST_DEBUG
if (f_debug) printf("\trate %d = %ld; offset = %ld\n", j,
d->voice[i].envelope_rate[j],
d->voice[i].envelope_offset[j]);
#endif
    }

  if (d->voice[i].sample->keyToVolEnvHold) {
	FLOAT_T rate_adjust;
	rate_adjust = pow(2.0, (60 - d->voice[i].note) * d->voice[i].sample->keyToVolEnvHold / 1200.0);
	d->voice[i].envelope_rate[HOLD] =
		(uint32)( (FLOAT_T)d->voice[i].envelope_rate[HOLD] / rate_adjust );
  }
  if (d->voice[i].sample->keyToVolEnvDecay) {
	FLOAT_T rate_adjust;
	rate_adjust = pow(2.0, (60 - d->voice[i].note) * d->voice[i].sample->keyToVolEnvDecay / 1200.0);
	d->voice[i].envelope_rate[DECAY] =
		(uint32)( (FLOAT_T)d->voice[i].envelope_rate[DECAY] / rate_adjust );
  }

  d->voice[i].echo_delay=d->voice[i].envelope_rate[DELAY];
  d->voice[i].echo_delay_count = d->voice[i].echo_delay;

#ifdef RATE_ADJUST_DEBUG
if (e_debug) {
printf("(old rel time = %ld)\n",
(d->voice[i].envelope_offset[2] - d->voice[i].envelope_offset[3]) / d->voice[i].envelope_rate[3]);
r = d->voice[i].envelope_rate[3];
r = r + ( (64-releasetime)*r ) / 100;
d->voice[i].envelope_rate[3] = r;
printf("(new rel time = %ld)\n",
(d->voice[i].envelope_offset[2] - d->voice[i].envelope_offset[3]) / d->voice[i].envelope_rate[3]);
}
}
#endif

  if (attacktime!=64)
    {
	rt = d->voice[i].envelope_rate[ATTACK];
	rt = rt + ( (64-attacktime)*rt ) / 100;
	if (rt > 1000) d->voice[i].envelope_rate[ATTACK] = rt;
    }
  if (releasetime!=64)
    {
	rt = d->voice[i].envelope_rate[RELEASE];
	rt = rt + ( (64-releasetime)*rt ) / 100;
	if (rt > 1000) d->voice[i].envelope_rate[RELEASE] = rt;
    }
  if (decaytime!=64)
    {
	rt = d->voice[i].envelope_rate[DECAY];
	rt = rt + ( (64-decaytime)*rt ) / 100;
	if (rt > 1000) d->voice[i].envelope_rate[DECAY] = rt;
    }

  if (d->channel[ch].panning != NO_PANNING)
    d->voice[i].panning=d->channel[ch].panning;
  else
    d->voice[i].panning=d->voice[i].sample->panning;
  if (drumpan != NO_PANNING)
    d->voice[i].panning=drumpan;

#ifdef tplus
  d->voice[i].porta_control_counter = 0;
  if(d->channel[ch].portamento && !d->channel[ch].porta_control_ratio)
      update_portamento_controls(ch, d);
  if(d->channel[ch].porta_control_ratio)
  {
      if(d->channel[ch].last_note_fine == -1)
      {
	  /* first on */
	  d->channel[ch].last_note_fine = d->voice[i].note * 256;
	  d->channel[ch].porta_control_ratio = 0;
      }
      else
      {
	  d->voice[i].porta_control_ratio = d->channel[ch].porta_control_ratio;
	  d->voice[i].porta_dpb = d->channel[ch].porta_dpb;
	  d->voice[i].porta_pb = d->channel[ch].last_note_fine -
	      d->voice[i].note * 256;
	  if(d->voice[i].porta_pb == 0)
	      d->voice[i].porta_control_ratio = 0;
      }
  }

  /* What "cnt"? if(cnt == 0) 
      d->channel[ch].last_note_fine = d->voice[i].note * 256; */
#endif

  if (d->voice[i].sample->sample_rate)
  {

		if (brightness >= 0 && brightness != 64) {
			int32 fq = d->voice[i].sample->cutoff_freq;
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
			d->voice[i].sample->cutoff_freq = fq;

		}
		if (harmoniccontent >= 0 && harmoniccontent != 64) {
			d->voice[i].sample->resonance = harmoniccontent / 256.0;
		}
  }


  if (reverb_options & OPT_STEREO_EXTRA || variationbank == 1) {
    int pan = d->voice[i].panning;
    int disturb = 0;
    /* If they're close up (no reverb) and you are behind the pianist,
     * high notes come from the right, so we'll spread piano etc. notes
     * out horizontally according to their pitches.
     */
    if (d->channel[ch].program < 21) {
	    int n = d->voice[i].velocity - 32;
	    if (n < 0) n = 0;
	    if (n > 64) n = 64;
	    pan = pan/2 + n;
	}
    /* For other types of instruments, the music sounds more alive if
     * notes come from slightly different directions.  However, instruments
     * do drift around in a sometimes disconcerting way, so the following
     * might not be such a good idea.
     */
    else disturb = (d->voice[i].velocity/32 % 8) +
	(d->voice[i].note % 8); /* /16? */

    if (pan < 64) pan += disturb;
    else pan -= disturb;
    if (pan < 0) pan = 0;
    else if (pan > 127) pan = 127;
    d->voice[i].panning = pan;
  }

  recompute_freq(i, d);
  recompute_amp(i, d);
  if (d->voice[i].sample->modes & MODES_ENVELOPE)
    {
      /* Ramp up from 0 */
      d->voice[i].envelope_stage=ATTACK;
      d->voice[i].envelope_volume=0;
      d->voice[i].control_counter=0;
      d->voice[i].modulation_stage=ATTACK;
      d->voice[i].modulation_volume=0;
      d->voice[i].modulation_counter=0;
      if (d->voice[i].sample->cutoff_freq)
      	d->voice[i].bw_index = 1 + (d->voice[i].sample->cutoff_freq+50) / 100;
      else d->voice[i].bw_index = 0;
      recompute_envelope(i, d);
      recompute_modulation(i, d);
      apply_envelope_to_amp(i, d);
    }
  else
    {
      d->voice[i].envelope_increment=0;
      apply_envelope_to_amp(i, d);
    }
  d->voice[i].clone_voice = -1;
  d->voice[i].clone_type = NOT_CLONE;
#ifdef DEBUG_CLONE_NOTES
fprintf(stderr,"NOT_CLONE v%d vol%f pan%d\n", i, d->voice[i].volume, d->voice[i].panning);
#endif

  if (reverb_options & OPT_STEREO_VOICE) clone_voice(ip, i, e, STEREO_CLONE, variationbank, d);
  if (reverb_options & OPT_CHORUS_VOICE || (d->XG_System_chorus_type >> 3) == 3)
				         clone_voice(ip, i, e, CHORUS_CLONE, variationbank, d);
  if (reverb_options & OPT_REVERB_VOICE) clone_voice(ip, i, e, REVERB_CLONE, variationbank, d);
  ctl->note(i);
  played_notes++;
}

/* changed from VOICE_DIE: the trouble with ramping out dying
   voices in mix.c is that the dying time is just a coincidence
   of how near a buffer end we happen to be.
*/
static void kill_note(int i, struct md *d)
{
  d->voice[i].status=VOICE_DIE;
  if (d->voice[i].clone_voice >= 0)
	d->voice[ d->voice[i].clone_voice ].status=VOICE_DIE;
  ctl->note(i);
}

#if 0
static int reduce_polyphony_by_one(struct md *d)
{
  int i=d->voices, lowest=-1; 
  int32 lv=0x7FFFFFFF, v;

  /* Look for the decaying note with the lowest volume */
  i=d->voices;
  while (i--)
    {
      if (d->voice[i].status==VOICE_FREE || !d->voice[i].sample->sample_rate) /* idea from Eric Welsh */
	    continue;
      if (d->voice[i].status & ~(VOICE_ON | VOICE_DIE))
	{
	  v=d->voice[i].left_mix;
	  if ((d->voice[i].panned==PANNED_MYSTERY) && (d->voice[i].right_mix>v))
	    v=d->voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }

  if (lowest != -1) {
	kill_note(lowest, d);
	return 1;
  }
  return 0;
}
#else

static int reduce_polyphony_by_one(struct md *d)
{
  int i=d->voices, lowest=-1; 
  int32 lv=0x7FFFFFFF, v;

  /* Look for the decaying note with the lowest volume */
/* Leave drums, sustains. */
  i=d->voices;
  while (i--)
    {
      if (d->voice[i].status==VOICE_FREE ||
	 !d->voice[i].sample->sample_rate /* idea from Eric Welsh */ ||
	  d->voice[i].sample->note_to_use)
	    continue;
      if (d->voice[i].status & ~(VOICE_ON | VOICE_DIE | VOICE_SUSTAINED))
	{
	  v=d->voice[i].left_mix;
	  if ((d->voice[i].panned==PANNED_MYSTERY) && (d->voice[i].right_mix>v))
	    v=d->voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }
  if (lowest != -1) {
	kill_note(lowest, d);
	return 1;
  }

/* Leave drums. */
  i=d->voices;
  while (i--)
    {
      if (d->voice[i].status==VOICE_FREE ||
	 !d->voice[i].sample->sample_rate /* idea from Eric Welsh */ ||
	  d->voice[i].sample->note_to_use)
	    continue;
      if (d->voice[i].status & ~(VOICE_ON | VOICE_DIE))
	{
	  v=d->voice[i].left_mix;
	  if ((d->voice[i].panned==PANNED_MYSTERY) && (d->voice[i].right_mix>v))
	    v=d->voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }

  if (lowest != -1) {
	kill_note(lowest, d);
	return 1;
  }
/* Kill drums. */
  i=d->voices;
  while (i--)
    {
      if (d->voice[i].status==VOICE_FREE ||
	 !d->voice[i].sample->sample_rate /* idea from Eric Welsh */ ||
	  d->voice[i].sample->note_to_use)
	    continue;
      if (d->voice[i].status & ~(VOICE_ON | VOICE_DIE))
	{
	  v=d->voice[i].left_mix;
	  if ((d->voice[i].panned==PANNED_MYSTERY) && (d->voice[i].right_mix>v))
	    v=d->voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }

  if (lowest != -1) {
	kill_note(lowest, d);
	return 1;
  }
/* Kill ON. */
  i=d->voices;
  while (i--)
    {
      if (d->voice[i].status==VOICE_FREE ||
	 !d->voice[i].sample->sample_rate /* idea from Eric Welsh */ ||
	  d->voice[i].sample->note_to_use)
	    continue;
      if (d->voice[i].status & ~VOICE_DIE)
	{
	  v=d->voice[i].left_mix;
	  if ((d->voice[i].panned==PANNED_MYSTERY) && (d->voice[i].right_mix>v))
	    v=d->voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }

  if (lowest != -1) {
	kill_note(lowest, d);
	return 1;
  }
/* Kill All. */
  i=d->voices;
  while (i--)
    {
      if (d->voice[i].status==VOICE_FREE)
	    continue;
      if (d->voice[i].status & ~VOICE_DIE)
	{
	  v=d->voice[i].left_mix;
	  if ((d->voice[i].panned==PANNED_MYSTERY) && (d->voice[i].right_mix>v))
	    v=d->voice[i].right_mix;
	  if (v<lv)
	    {
	      lv=v;
	      lowest=i;
	    }
	}
    }

  if (lowest != -1) {
	kill_note(lowest, d);
	return 1;
  }
  return 0;
}

#endif

static int reduce_polyphony(int by, struct md *d)
{ int ret = 0;
  while (by--) if (reduce_polyphony_by_one(d)) ret++; else break;
  return ret;
}

static int check_quality(struct md *d)
{
#ifdef QUALITY_DEBUG
  static int debug_count = 0;
#endif
  int obf = d->output_buffer_full, retvalue = 1;
  int vcs = d->voices;
  int cpoly = d->current_polyphony;
  int rsv = 0, permitted, polyreduction;

#ifdef QUALITY_DEBUG
if (!debug_count) {
	if (d->dont_cspline) fprintf(stderr,"[%d-%d]", d->output_buffer_full, d->current_polyphony);
	else fprintf(stderr,"{%d-%d}", d->output_buffer_full, d->current_polyphony);
	debug_count = 30;
}
debug_count--;
#endif

#if 0
#ifdef POLYPHONY_COUNT
  if (d->current_polyphony < d->future_polyphony * 2) obf /= 2;
#endif
#endif

  permitted = vcs - 16;
  if (permitted < 0) permitted = 0;
  if (obf > 0) permitted = permitted * obf / 100;
  else permitted = 0;
  permitted += 16;

  rsv = vcs - permitted;

  d->voice_reserve = rsv;

#if 0
  if (!d->current_interpolation) d->dont_cspline = 1;
  else if (obf < 10) d->dont_cspline = 1;
  else if (obf > 40) d->dont_cspline = 0;

  if (obf < 5) d->dont_reverb = 1;
  else if (obf > 25) d->dont_reverb = 0;

  if (obf < 8) d->dont_chorus = 1;
  else if (obf > 60) d->dont_chorus = 0;

  if (opt_dry || obf < 6) d->dont_keep_looping = 1;
  else if (obf > 22) d->dont_keep_looping = 0;
#endif

/*
  if (obf < 20) dont_filter = 1;
  else if (obf > 80) dont_filter = 0;
*/
/* pool should be permitted/10
 * present pool is permitted - cpoly
 * reduction = permitted/10 - (permitted - cpoly)
 */

  polyreduction = cpoly - permitted;
  polyreduction += permitted / 5;

/*
fprintf(stderr,"\tred %d = cpoly %d - permitted %d + p/4 %d\n", polyreduction, cpoly, permitted,
		permitted / 4);
*/
/*
  polyreduction = permitted / 4 - (permitted - cpoly);
  if (polyreduction < 0) polyreduction = 0;
*/
/*
 * extra reduction if buffer is too low
 * (100 - obf)% of cpoly
 */
/*
  polyreduction += cpoly * (100 - obf) / 100;
*/

  if (polyreduction>0) retvalue = reduce_polyphony(polyreduction, d);

/*
if (polyreduction>0) fprintf(stderr,"\tcpoly=%d: red ?%d->%d\n", cpoly, polyreduction, retvalue);
*/
  retvalue = 1;

  if (command_cutoff_allowed) dont_filter_melodic = 0;
  else dont_filter_melodic = 1;

  dont_filter_drums = 0;

  return retvalue;
}

static void note_on(MidiEvent *e, struct md *d)
{
  int i=d->voices, lowest=-1; 
  int32 lv=0x7FFFFFFF, v;
  int cpoly = 0, cdie = 0, cfree = 0;

  while (i--)
    {
      if (d->voice[i].status == VOICE_FREE) {
	lowest=i; /* Can't get a lower volume than silence */
	cfree++;
      }
      else if (d->voice[i].status == VOICE_DIE) cdie++;
      else cpoly++;
    }

  d->current_polyphony = cpoly;
  d->current_free_voices = cfree;
  d->current_dying_voices = cdie;

  if (!check_quality(d))
    {
      d->lost_notes++;
      return;
    }
  if (lowest != -1)
    {
      /* Found a free voice. */
      start_note(e,lowest, d);
      return;
    }
 
  /* Look for the decaying note with the lowest volume */
  i=d->voices;
  while (i--)
    {
      if (d->voice[i].status & ~(VOICE_ON | VOICE_DIE | VOICE_FREE))
	{
	  v=d->voice[i].left_mix;
	  if ((d->voice[i].panned==PANNED_MYSTERY) && (d->voice[i].right_mix>v))
	    v=d->voice[i].right_mix;
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
   i=d->voices;
   while (i--)
    {
      if ( (d->voice[i].status & ~(VOICE_ON | VOICE_DIE | VOICE_FREE)) &&
	  (!d->voice[i].clone_type))
	{
	  v=d->voice[i].left_mix;
	  if ((d->voice[i].panned==PANNED_MYSTERY) && (d->voice[i].right_mix>v))
	    v=d->voice[i].right_mix;
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
      int cl = d->voice[lowest].clone_voice;
      /* This can still cause a click, but if we had a free voice to
	 spare for ramping down this note, we wouldn't need to kill it
	 in the first place... Still, this needs to be fixed. Perhaps
	 we could use a reserve of d->voices to play dying notes only. */
      
      cut_notes++;
      d->voice[lowest].status=VOICE_FREE;
      if (cl >= 0) {
	/** if (d->voice[lowest].velocity == d->voice[cl].velocity) **/
	if (d->voice[cl].clone_type==STEREO_CLONE || (!d->voice[cl].clone_type && d->voice[lowest].clone_type==STEREO_CLONE))
	   d->voice[cl].status=VOICE_FREE;
	else if (d->voice[cl].clone_voice==lowest) d->voice[cl].clone_voice=-1;
      }
      ctl->note(lowest);
      start_note(e,lowest, d);
    }
  else
    d->lost_notes++;
}

static void finish_note(int i, struct md *d)
{
  if (d->voice[i].status & (VOICE_FREE | VOICE_DIE | VOICE_OFF)) return;

  if (d->voice[i].sample->modes & MODES_ENVELOPE)
    {
      /* We need to get the envelope out of Sustain stage */
     if (d->voice[i].envelope_stage < RELEASE)
      d->voice[i].envelope_stage=RELEASE;
      d->voice[i].status=VOICE_OFF;
      recompute_envelope(i, d);
      apply_envelope_to_amp(i, d);
      ctl->note(i);
    }
  else
    {
      /* Set status to OFF so resample_voice() will let this voice out
         of its loop, if any. In any case, this voice dies when it
         hits the end of its data (ofs>=data_length). */
      d->voice[i].status=VOICE_OFF;
      ctl->note(i);
    }
  { int v;
    if ( (v=d->voice[i].clone_voice) >= 0)
      {
	d->voice[i].clone_voice = -1;
        finish_note(v, d);
      }
  }
}

static void note_off(MidiEvent *e, struct md *d)
{
  int i=d->voices, v;
  while (i--)
    if (d->voice[i].status==VOICE_ON &&
	d->voice[i].channel==e->channel &&
	d->voice[i].note==e->a)
      {
	if (d->channel[e->channel].sustain)
	  {
	    d->voice[i].status=VOICE_SUSTAINED;
	    ctl->note(i);
    	    if ( (v=d->voice[i].clone_voice) >= 0)
	      {
		if (d->voice[v].status == VOICE_ON)
		  d->voice[v].status=VOICE_SUSTAINED;
	      }
	  }
	else
	  finish_note(i, d);
      }
}

/* Process the All Notes Off event */
static void all_notes_off(int c, struct md *d)
{
  int i=d->voices;
  ctl->cmsg(CMSG_INFO, VERB_DEBUG, "All notes off on channel %d", c);
  while (i--)
    if (d->voice[i].status==VOICE_ON &&
	d->voice[i].channel==c)
      {
	if (d->channel[c].sustain) 
	  {
	    d->voice[i].status=VOICE_SUSTAINED;
	    ctl->note(i);
	  }
	else
	  finish_note(i, d);
      }
}

/* Process the All Sounds Off event */
static void all_sounds_off(int c, struct md *d)
{
  int i=d->voices;
  while (i--)
    if (d->voice[i].channel==c && 
	d->voice[i].status != VOICE_FREE &&
	d->voice[i].status != VOICE_DIE)
      {
	kill_note(i, d);
      }
}

static void adjust_pressure(MidiEvent *e, struct md *d)
{
  int i=d->voices;
  while (i--)
    if (d->voice[i].status==VOICE_ON &&
	d->voice[i].channel==e->channel &&
	d->voice[i].note==e->a)
      {
	d->voice[i].velocity=e->b;
	recompute_amp(i, d);
	apply_envelope_to_amp(i, d);
      }
}

#ifdef tplus
static void adjust_channel_pressure(MidiEvent *e, struct md *d)
{
#if 0
    if(opt_channel_pressure)
    {
#endif
	int i=d->voices;
	int ch, pressure;

	ch = e->channel;
	pressure = e->a;
        while (i--)
	    if(d->voice[i].status == VOICE_ON && d->voice[i].channel == ch)
	    {
		d->voice[i].velocity = pressure;
		recompute_amp(i, d);
		apply_envelope_to_amp(i, d);
	    }
#if 0
    }
#endif
}
#endif

static void adjust_panning(int c, struct md *d)
{
  int i=d->voices;
  while (i--)
    if ((d->voice[i].channel==c) &&
	(d->voice[i].status & (VOICE_ON | VOICE_SUSTAINED)) )
      {
	/* if (d->voice[i].clone_voice >= 0) continue; */
	if (d->voice[i].clone_type != NOT_CLONE) continue;
	d->voice[i].panning=d->channel[c].panning;
	recompute_amp(i, d);
	apply_envelope_to_amp(i, d);
      }
}

static void drop_sustain(int c, struct md *d)
{
  int i=d->voices;
  while (i--)
    if ( (d->voice[i].status & VOICE_SUSTAINED) && d->voice[i].channel==c)
      finish_note(i, d);
}

static void adjust_pitchbend(int c, struct md *d)
{
  int i=d->voices;
  while (i--)
    if (d->voice[i].status!=VOICE_FREE && d->voice[i].channel==c)
      {
	recompute_freq(i, d);
      }
}

static void adjust_volume(int c, struct md *d)
{
  int i=d->voices;
  while (i--)
    if (d->voice[i].channel==c &&
	(d->voice[i].status==VOICE_ON || d->voice[i].status==VOICE_SUSTAINED))
      {
	recompute_amp(i, d);
	apply_envelope_to_amp(i, d);
	ctl->note(i);
      }
}

#ifdef tplus
static int32 midi_cnv_vib_rate(int rate, struct md *d)
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

#ifdef INFO_ONLY
static int xmp_epoch = -1;
static unsigned xxmp_epoch = 0;
static unsigned time_expired = 0;
static unsigned last_time_expired = 0;
extern int gettimeofday(struct timeval *, struct timezone *);
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
#endif

static void seek_forward(uint32 until_time, struct md *d)
{
  reset_voices(d);
#ifdef INFO_ONLY
  show_markers(until_time, 1);
#endif
  while (d->current_event->time < until_time)
    {
      switch(d->current_event->type)
	{
	  /* All notes stay off. Just handle the parameter changes. */

	case ME_PITCH_SENS:
	  d->channel[d->current_event->channel].pitchsens=
	    d->current_event->a;
	  d->channel[d->current_event->channel].pitchfactor=0;
	  break;
	  
	case ME_PITCHWHEEL:
	  d->channel[d->current_event->channel].pitchbend=
	    d->current_event->a + d->current_event->b * 128;
	  d->channel[d->current_event->channel].pitchfactor=0;
	  break;
#ifdef tplus
	    case ME_MODULATION_WHEEL:
	      d->channel[d->current_event->channel].modulation_wheel =
		    midi_cnv_vib_depth(d->current_event->a);
	      break;

            case ME_PORTAMENTO_TIME_MSB:
	      d->channel[d->current_event->channel].portamento_time_msb = d->current_event->a;
	      break;

            case ME_PORTAMENTO_TIME_LSB:
	      d->channel[d->current_event->channel].portamento_time_lsb = d->current_event->a;
	      break;

            case ME_PORTAMENTO:
	      d->channel[d->current_event->channel].portamento = (d->current_event->a >= 64);
	      break;

            case ME_FINE_TUNING:
	      d->channel[d->current_event->channel].tuning_lsb = d->current_event->a;
	      break;

            case ME_COARSE_TUNING:
	      d->channel[d->current_event->channel].tuning_msb = d->current_event->a;
	      break;

      	    case ME_CHANNEL_PRESSURE:
	      /*adjust_channel_pressure(d->current_event);*/
	      break;

#endif
	  
	case ME_MAINVOLUME:
	  d->channel[d->current_event->channel].volume=d->current_event->a;
	  break;
	  
	case ME_MASTERVOLUME:
	  adjust_master_volume(d->current_event->a + (d->current_event->b <<7), d);
	  break;
	  
	case ME_PAN:
	  d->channel[d->current_event->channel].panning=d->current_event->a;
	  break;
	      
	case ME_EXPRESSION:
	  d->channel[d->current_event->channel].expression=d->current_event->a;
	  break;
	  
	case ME_PROGRAM:
  	  if (d->channel[d->current_event->channel].kit)
	    /* Change drum set */
	    d->channel[d->current_event->channel].bank=d->current_event->a;
	  else
	    d->channel[d->current_event->channel].program=d->current_event->a;
	  break;

	case ME_SUSTAIN:
	  d->channel[d->current_event->channel].sustain=d->current_event->a;
	  break;

	case ME_REVERBERATION:
	 if (global_reverb > d->current_event->a) break;
#ifdef CHANNEL_EFFECT
	 if (opt_effect_reverb)
	  effect_ctrl_change( d->current_event, d ) ;
	 else
#endif
	  d->channel[d->current_event->channel].reverberation=d->current_event->a;
	  break;

	case ME_CHORUSDEPTH:
	 if (global_chorus > d->current_event->a) break;
#ifdef CHANNEL_EFFECT
	 if (opt_effect)
	  effect_ctrl_change( d->current_event, d ) ;
	 else
#endif
	  d->channel[d->current_event->channel].chorusdepth=d->current_event->a;
	  break;

#ifdef CHANNEL_EFFECT
	case ME_CELESTE:
	 if (opt_effect)
	  effect_ctrl_change( d->current_event, d ) ;
	  break;
	case ME_PHASER:
	 if (opt_effect)
	  effect_ctrl_change( d->current_event, d ) ;
	  break;
#endif

	case ME_HARMONICCONTENT:
	  d->channel[d->current_event->channel].harmoniccontent=d->current_event->a;
	  break;

	case ME_RELEASETIME:
	  d->channel[d->current_event->channel].releasetime=d->current_event->a;
	  break;

	case ME_ATTACKTIME:
	  d->channel[d->current_event->channel].attacktime=d->current_event->a;
	  break;
#ifdef tplus
	case ME_VIBRATO_RATE:
	  d->channel[d->current_event->channel].vibrato_ratio=midi_cnv_vib_rate(d->current_event->a, d);
	  break;
	case ME_VIBRATO_DEPTH:
	  d->channel[d->current_event->channel].vibrato_depth=midi_cnv_vib_depth(d->current_event->a);
	  break;
	case ME_VIBRATO_DELAY:
	  d->channel[d->current_event->channel].vibrato_delay=midi_cnv_vib_delay(d->current_event->a);
	  break;
#endif
	case ME_BRIGHTNESS:
	  d->channel[d->current_event->channel].brightness=d->current_event->a;
	  break;

	case ME_RESET_CONTROLLERS:
	  reset_controllers(d->current_event->channel, d);
	  break;
	      
	case ME_TONE_BANK:
	  d->channel[d->current_event->channel].bank=d->current_event->a;
	  break;
	  
	case ME_TONE_KIT:
	  if (d->current_event->a==SFX_BANKTYPE)
		{
		    d->channel[d->current_event->channel].sfx=SFXBANK;
		    d->channel[d->current_event->channel].kit=0;
		}
	  else
		{
		    d->channel[d->current_event->channel].sfx=0;
		    d->channel[d->current_event->channel].kit=d->current_event->a;
		}
	  break;
	  
	case ME_EOT:
	  d->current_sample=d->current_event->time;
	  return;
	}
      d->current_event++;
    }
  /*d->current_sample=d->current_event->time;*/
  if (d->current_event != d->event)
    d->current_event--;
  d->current_sample=until_time;
}

int skip_to(uint32 until_time, struct md *d)
{
  if (d->current_sample > until_time)
    d->current_sample=0;

  if (!d->event) return 0;

  reset_midi(d);
  d->buffered_count=0;
  d->buffer_pointer=d->common_buffer;
  d->current_event=d->event;

  if (until_time)
    seek_forward(until_time, d);
#ifdef INFO_ONLY
  else show_markers(until_time, 1);
#endif
  ctl->reset();
  return 1;
}


static int apply_controls(struct md *d)
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
	play_mode->purge_output(d);
	return rc;
	
      case RC_CHANGE_VOLUME:
	if (val>0 || d->amplification > -val)
	  d->amplification += val;
	else 
	  d->amplification=0;
	if (d->amplification > MAX_AMPLIFICATION)
	  d->amplification=MAX_AMPLIFICATION;
	adjust_amplification(d);
	for (i=0; i<d->voices; i++)
	  if (d->voice[i].status != VOICE_FREE)
	    {
	      recompute_amp(i, d);
	      apply_envelope_to_amp(i, d);
	    }
	ctl->master_volume(d->amplification);
	break;

      case RC_PREVIOUS: /* |<< */
	play_mode->purge_output(d);
	if (d->current_sample < 2*play_mode->rate)
	  return RC_REALLY_PREVIOUS;
	return RC_RESTART;

      case RC_RESTART: /* |<< */
	skip_to(0, d);
	play_mode->purge_output(d);
	did_skip=1;
	break;
	
      case RC_JUMP:
	play_mode->purge_output(d);
	if (val >= (int32)d->sample_count)
	  return RC_NEXT;
	skip_to((uint32)val, d);
	return rc;
	
      case RC_FORWARD: /* >> */
	/*play_mode->purge_output();*/
	if (val+d->current_sample >= d->sample_count)
	  return RC_NEXT;
	skip_to(val+d->current_sample, d);
	did_skip=1;
	break;
	
      case RC_BACK: /* << */
	/*play_mode->purge_output();*/
	if (d->current_sample > (uint32)val)
	  skip_to(d->current_sample-(uint32)val, d);
	else
	  skip_to(0, d); /* We can't seek to end of previous song. */
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
extern void (*do_compute_data)(uint32, struct md *) ;
#else
static void do_compute_data(uint32 count, struct md *d)
{
  int i;
  if (!count) return; /* (gl) */
  memset(d->buffer_pointer, 0, 
	 (play_mode->encoding & PE_MONO) ? (count * 4) : (count * 8));
  for (i=0; i<d->voices; i++)
    {
      if(d->voice[i].status != VOICE_FREE)
	{
	  if (!d->voice[i].sample_offset && d->voice[i].echo_delay_count)
	    {
		if (d->voice[i].echo_delay_count >= count) d->voice[i].echo_delay_count -= count;
		else
		  {
	            mix_voice(d->buffer_pointer+d->voice[i].echo_delay_count, i, count-d->voice[i].echo_delay_count, d);
		    d->voice[i].echo_delay_count = 0;
		  }
	    }
	  else mix_voice(d->buffer_pointer, i, count, d);
	}
    }
  d->current_sample += count;
}
#endif /*CHANNEL_EFFECT*/


/* count=0 means flush remaining buffered data to output device, then
   flush the device itself */
static int compute_data(uint32 count, struct md *d)
{
  int rc;
  d->super_buffer_count = 0;
  if (!count)
    {
      if (d->buffered_count)
	play_mode->output_data(d->common_buffer, d->buffered_count, d);
      play_mode->flush_output(d);
      d->buffer_pointer=d->common_buffer;
      d->buffered_count=0;
      return RC_NONE;
    }

  while ((count+d->buffered_count) >= AUDIO_BUFFER_SIZE)
    {
      if (d->bbcount + AUDIO_BUFFER_SIZE * 2 >= BB_SIZE - output_fragsize) {
	      d->super_buffer_count = count;
	      return 0;
      }
      if (d->flushing_output_device && d->bbcount >= output_fragsize * 2) {
	      d->super_buffer_count = count;
	      return 0;
      }
      do_compute_data(AUDIO_BUFFER_SIZE-d->buffered_count, d);
      count -= AUDIO_BUFFER_SIZE-d->buffered_count;
      play_mode->output_data(d->common_buffer, AUDIO_BUFFER_SIZE, d);
      d->buffer_pointer=d->common_buffer;
      d->buffered_count=0;
      
      ctl->current_time(d->current_sample);
#ifdef INFO_ONLY
      show_markers(0, 0);
#endif
      if ((rc=apply_controls(d))!=RC_NONE)
	return rc;
    }
  if (count>0)
    {
      do_compute_data(count, d);
      d->buffered_count += count;
      d->buffer_pointer += (play_mode->encoding & PE_MONO) ? count : count*2;
    }
  return RC_NONE;
}

#ifdef tplus
static void update_modulation_wheel(int ch, uint32 val, struct md *d)
{
    int i, uv = /*upper_*/d->voices;
    for(i = 0; i < uv; i++)
	if(d->voice[i].status != VOICE_FREE && d->voice[i].channel == ch && d->voice[i].modulation_wheel != val)
	{
	    /* Set/Reset mod-wheel */
	    d->voice[i].modulation_wheel = val;
	    d->voice[i].vibrato_delay = 0;
	    recompute_freq(i, d);
	}
}

static void drop_portamento(int ch, struct md *d)
{
    int i, uv = /*upper_*/d->voices;

    d->channel[ch].porta_control_ratio = 0;
    for(i = 0; i < uv; i++)
	if(d->voice[i].status != VOICE_FREE &&
	   d->voice[i].channel == ch &&
	   d->voice[i].porta_control_ratio)
	{
	    d->voice[i].porta_control_ratio = 0;
	    recompute_freq(i, d);
	}
    d->channel[ch].last_note_fine = -1;
}

static void update_portamento_controls(int ch, struct md *d)
{
    if(!d->channel[ch].portamento ||
       (d->channel[ch].portamento_time_msb | d->channel[ch].portamento_time_lsb)
       == 0)
	drop_portamento(ch, d);
    else
    {
	double mt, dc;
	int di;

	mt = midi_time_table[d->channel[ch].portamento_time_msb & 0x7F] *
	    midi_time_table2[d->channel[ch].portamento_time_lsb & 0x7F] *
		PORTAMENTO_TIME_TUNING;
	dc = play_mode->rate * mt;
	di = (int)(1.0 / (mt * PORTAMENTO_CONTROL_RATIO));
	di++;
	d->channel[ch].porta_control_ratio = (int)(di * dc + 0.5);
	d->channel[ch].porta_dpb = di;
    }
}

static void update_portamento_time(int ch, struct md *d)
{
    int i, uv = /*upper_*/d->voices;
    int dpb;
    int32 ratio;

    update_portamento_controls(ch, d);
    dpb = d->channel[ch].porta_dpb;
    ratio = d->channel[ch].porta_control_ratio;

    for(i = 0; i < uv; i++)
    {
	if(d->voice[i].status != VOICE_FREE &&
	   d->voice[i].channel == ch &&
	   d->voice[i].porta_control_ratio)
	{
	    d->voice[i].porta_control_ratio = ratio;
	    d->voice[i].porta_dpb = dpb;
	    recompute_freq(i, d);
	}
    }
}

static void update_channel_freq(int ch, struct md *d)
{
    int i, uv = /*upper_*/d->voices;
    for(i = 0; i < uv; i++)
	if(d->voice[i].status != VOICE_FREE && d->voice[i].channel == ch)
	    recompute_freq(i, d);
}
#endif

/*static uint32 event_count = 0;*/

static int play_midi(struct md *d)
{

  adjust_amplification(d);
  d->lost_notes=cut_notes=played_notes=output_clips=0;
  skip_to(0, d);
  return 0;
}


int play_some_midi(struct md *d)
{
  int rc, basecount;

  basecount = d->bbcount;

  for (;;)
    {
      if (d->super_buffer_count) {
	      compute_data(d->super_buffer_count, d);
	      return 0;
      }
      /*
      if (d->flushing_output_device && d->bbcount >= output_fragsize) {
	      return 0;
      }
      */
      if (d->flushing_output_device) {
	      compute_data(0, d);
	      return RC_TUNE_END;
      }

      /* Handle all events that should happen at this time */
      while (d->current_event->time <= d->current_sample)
	{
	#ifdef POLYPHONY_COUNT
	  d->future_polyphony = d->current_event->polyphony;
	#endif
	  switch(d->current_event->type)
	    {

	      /* Effects affecting a single note */

	    case ME_NOTEON:

#ifdef tplus
#if 0
	      if (d->channel[d->current_event->channel].portamento &&
		    d->current_event->b &&
			(d->channel[d->current_event->channel].portamento_time_msb|
			 d->channel[d->current_event->channel].portamento_time_lsb )) break;
#endif
#endif
	      d->current_event->a += d->channel[d->current_event->channel].transpose;
	      if (!(d->current_event->b)) /* Velocity 0? */
		note_off(d->current_event, d);
	      else
		note_on(d->current_event, d);
	      break;

	    case ME_NOTEOFF:
	      d->current_event->a += d->channel[d->current_event->channel].transpose;
	      note_off(d->current_event, d);
	      break;

	    case ME_KEYPRESSURE:
	      adjust_pressure(d->current_event, d);
	      break;

	      /* Effects affecting a single channel */

	    case ME_PITCH_SENS:
	      d->channel[d->current_event->channel].pitchsens=
		d->current_event->a;
	      d->channel[d->current_event->channel].pitchfactor=0;
	      break;
	      
	    case ME_PITCHWHEEL:
	      d->channel[d->current_event->channel].pitchbend=
		d->current_event->a + d->current_event->b * 128;
	      d->channel[d->current_event->channel].pitchfactor=0;
	      /* Adjust pitch for notes already playing */
	      adjust_pitchbend(d->current_event->channel, d);
	      ctl->pitch_bend(d->current_event->channel, 
			      d->channel[d->current_event->channel].pitchbend);
	      break;
#ifdef tplus
	    case ME_MODULATION_WHEEL:
	      d->channel[d->current_event->channel].modulation_wheel =
		    midi_cnv_vib_depth(d->current_event->a);
	      update_modulation_wheel(d->current_event->channel, d->channel[d->current_event->channel].modulation_wheel, d);
		/*ctl_mode_event(CTLE_MOD_WHEEL, 1, d->current_event->channel, d->channel[d->current_event->channel].modulation_wheel);*/
		/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~m%u", midi_cnv_vib_depth(d->current_event->a));*/
	      break;

            case ME_PORTAMENTO_TIME_MSB:
	      d->channel[d->current_event->channel].portamento_time_msb = d->current_event->a;
	      update_portamento_time(d->current_event->channel, d);
	      break;

            case ME_PORTAMENTO_TIME_LSB:
	      d->channel[d->current_event->channel].portamento_time_lsb = d->current_event->a;
	      update_portamento_time(d->current_event->channel, d);
	      break;

            case ME_PORTAMENTO:
	      d->channel[d->current_event->channel].portamento = (d->current_event->a >= 64);
	      if(!d->channel[d->current_event->channel].portamento) drop_portamento(d->current_event->channel, d);
		/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~P%d",d->current_event->a);*/
	      break;

            case ME_FINE_TUNING:
	      d->channel[d->current_event->channel].tuning_lsb = d->current_event->a;
		/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~t");*/
	      break;

            case ME_COARSE_TUNING:
	      d->channel[d->current_event->channel].tuning_msb = d->current_event->a;
		/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~T");*/
	      break;

      	    case ME_CHANNEL_PRESSURE:
	      adjust_channel_pressure(d->current_event, d);
	      break;

#endif
	    case ME_MAINVOLUME:
	      d->channel[d->current_event->channel].volume=d->current_event->a;
	      adjust_volume(d->current_event->channel, d);
	      ctl->volume(d->current_event->channel, d->current_event->a);
	      break;
	      
	    case ME_MASTERVOLUME:
	      adjust_master_volume(d->current_event->a + (d->current_event->b <<7), d);
	      break;
	      
	    case ME_REVERBERATION:
	 if (global_reverb > d->current_event->a) break;
#ifdef CHANNEL_EFFECT
	 if (opt_effect_reverb)
	      effect_ctrl_change( d->current_event, d ) ;
	 else
#endif
	      d->channel[d->current_event->channel].reverberation=d->current_event->a;
	      break;

	    case ME_CHORUSDEPTH:
	 if (global_chorus > d->current_event->a) break;
#ifdef CHANNEL_EFFECT
	 if (opt_effect) {
	  if (XG_effect_chorus_is_celeste_flag) d->current_event->type = ME_CELESTE;
	  else if (XG_effect_chorus_is_phaser_flag) d->current_event->type = ME_PHASER;
	  effect_ctrl_change( d->current_event, d ) ;
	 }
	 else
#endif
	      d->channel[d->current_event->channel].chorusdepth=d->current_event->a;
	      break;

#ifdef CHANNEL_EFFECT
	case ME_CELESTE:
	 if (opt_effect)
	  effect_ctrl_change( d->current_event, d ) ;
	  break;
	case ME_PHASER:
	 if (opt_effect)
	  effect_ctrl_change( d->current_event, d ) ;
	  break;
#endif

	    case ME_PAN:
	      d->channel[d->current_event->channel].panning=d->current_event->a;
	      if (d->adjust_panning_immediately)
		adjust_panning(d->current_event->channel, d);
	      ctl->panning(d->current_event->channel, d->current_event->a);
	      break;
	      
	    case ME_EXPRESSION:
	      d->channel[d->current_event->channel].expression=d->current_event->a;
	      adjust_volume(d->current_event->channel, d);
	      ctl->expression(d->current_event->channel, d->current_event->a);
	      break;

	    case ME_PROGRAM:
  	      if (d->channel[d->current_event->channel].kit)
		{
		  /* Change drum set */
		  /* if (d->channel[d->current_event->channel].kit==126)
		  	d->channel[d->current_event->channel].bank=57;
		  else */
		  d->channel[d->current_event->channel].bank=d->current_event->a;
		/*
	          ctl->program(d->current_event->channel, d->current_event->a,
			d->channel[d->current_event->channel].name);
		*/
	          ctl->program(d->current_event->channel, d->current_event->a,
      			drumset[d->channel[d->current_event->channel].bank]->name);
		}
	      else
		{
		  d->channel[d->current_event->channel].program=d->current_event->a;
	          ctl->program(d->current_event->channel, d->current_event->a,
      			tonebank[d->channel[d->current_event->channel].bank]->
				tone[d->current_event->a].name);
		}
	      break;

	    case ME_SUSTAIN:
	      d->channel[d->current_event->channel].sustain=d->current_event->a;
	      if (!d->current_event->a)
		drop_sustain(d->current_event->channel, d);
	      ctl->sustain(d->current_event->channel, d->current_event->a);
	      break;
	      
	    case ME_RESET_CONTROLLERS:
	      reset_controllers(d->current_event->channel, d);
	      redraw_controllers(d->current_event->channel, d);
	      break;

	    case ME_ALL_NOTES_OFF:
	      all_notes_off(d->current_event->channel, d);
	      break;
	      
	    case ME_ALL_SOUNDS_OFF:
	      all_sounds_off(d->current_event->channel, d);
	      break;

	    case ME_HARMONICCONTENT:
	      d->channel[d->current_event->channel].harmoniccontent=d->current_event->a;
	      break;

	    case ME_RELEASETIME:
	      d->channel[d->current_event->channel].releasetime=d->current_event->a;
/*
if (d->current_event->a != 64)
printf("release time %d on channel %d\n", d->channel[d->current_event->channel].releasetime,
d->current_event->channel);
*/
	      break;

	    case ME_ATTACKTIME:
	      d->channel[d->current_event->channel].attacktime=d->current_event->a;
/*
if (d->current_event->a != 64)
printf("attack time %d on channel %d\n", d->channel[d->current_event->channel].attacktime,
d->current_event->channel);
*/
	      break;
#ifdef tplus
	case ME_VIBRATO_RATE:
	  d->channel[d->current_event->channel].vibrato_ratio=midi_cnv_vib_rate(d->current_event->a, d);
	  update_channel_freq(d->current_event->channel, d);
	  break;
	case ME_VIBRATO_DEPTH:
	  d->channel[d->current_event->channel].vibrato_depth=midi_cnv_vib_depth(d->current_event->a);
	  update_channel_freq(d->current_event->channel, d);
	/*ctl->cmsg(CMSG_INFO, VERB_NORMAL, "~v%u", midi_cnv_vib_depth(d->current_event->a));*/
	  break;
	case ME_VIBRATO_DELAY:
	  d->channel[d->current_event->channel].vibrato_delay=midi_cnv_vib_delay(d->current_event->a);
	  update_channel_freq(d->current_event->channel, d);
	  break;
#endif

	    case ME_BRIGHTNESS:
	      d->channel[d->current_event->channel].brightness=d->current_event->a;
	      break;
	      
	    case ME_TONE_BANK:
	      d->channel[d->current_event->channel].bank=d->current_event->a;
	      break;

	    case ME_TONE_KIT:
	      if (d->current_event->a==SFX_BANKTYPE)
		{
		    d->channel[d->current_event->channel].sfx=SFXBANK;
		    d->channel[d->current_event->channel].kit=0;
		}
	      else
		{
		    d->channel[d->current_event->channel].sfx=0;
		    d->channel[d->current_event->channel].kit=d->current_event->a;
		}
	      break;

	    case ME_EOT:
	      /*
	  fprintf(stderr,"midi end of tune\n");
	       */
	  d->flushing_output_device = TRUE;
	      /* Give the last notes a couple of seconds to decay  */
	      compute_data(play_mode->rate * 2, d);
	      /*
	  fprintf(stderr,"filled 2 sec\n");
	       */
	      if (!d->super_buffer_count) compute_data(0, d); /* flush buffer to device */
	      /*
	  fprintf(stderr,"flushed\n");
	       */
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Playing time: ~%d seconds",
		   d->current_sample/play_mode->rate+2);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Notes played: %d", played_notes);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Samples clipped: %d", output_clips);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Notes cut: %d", cut_notes);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Notes lost totally: %d", d->lost_notes);
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		   "Current memory used: %d", current_patch_memory);
	      /*
	  fprintf(stderr,"end of tune\n");
	       */
	      if (d->super_buffer_count) return 0;
	      return RC_TUNE_END;
	    }
	  d->current_event++;
	  /*event_count--;*/
	}

      rc=compute_data(d->current_event->time - d->current_sample, d);

/*
fprintf(stderr,"d->current_sample=%d d->current_event=%x, %ld left, d->bbcount=%d of %d\n",
  	d->current_sample, d->current_event, event_count, d->bbcount, BB_SIZE);
*/
/*fprintf(stderr,"Z\n");*/

      ctl->refresh();
      if ( (rc!=RC_NONE) && (rc!=RC_JUMP)) {
	  fprintf(stderr,"play_some returning %d\n", rc);
	  return rc;
      }
      if (d->bbcount - basecount > output_fragsize &&
		      !d->flushing_output_device &&
		      d->current_sample < d->sample_count) return 0;
    }
}



int play_midi_file(struct md *d)
{
  int rc;
  int32 val;

  ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MIDI file: %s", d->midi_path_name);

/*
  if (!(d->fp=open_file(d->midi_path_name, 1, OF_VERBOSE, 0))) return RC_ERROR;
*/
  if (!(d->fp=fopen(d->midi_path_name, "r"))) return RC_ERROR;

  ctl->file_name(d->midi_name);

  d->is_open = TRUE;
  read_midi_file(d);

  close_file(d->fp);
  
  if (!d->event)
      return RC_ERROR;

  ctl->cmsg(CMSG_INFO, VERB_NOISY, 
	    "%d supported events, %d samples", d->event_count, d->sample_count);

  ctl->total_time(d->sample_count);
  ctl->master_volume(d->amplification);

  load_missing_instruments();
  if (check_for_rc()) return ctl->read(&val);
#ifdef tplus
  d->dont_cspline = 0;
#endif
  if (command_cutoff_allowed) dont_filter_melodic = 0;
  else dont_filter_melodic = 1;

#ifdef INFO_ONLY
  got_a_lyric = 0;
#endif
#ifdef POLYPHONY_COUNT
  d->future_polyphony = 0;
#endif
  d->current_polyphony = 0;
  rc=play_midi(d);
/*
  if (free_instruments_afterwards)
      free_instruments();
  
  free(event);
*/
  if (!rc) d->is_playing = TRUE;
  return rc;
}

void play_midi_finish(struct md *d)
{
  int i;
  compute_data(0, d);
#ifdef CHANNEL_EFFECT
  if (opt_effect)
  for (i=0; i<MAXCHAN; i++)
    {
	effect_ctrl_kill( i, d ) ;
    }
#endif
  if (free_instruments_afterwards)
      free_instruments();
  if (d->event) free(d->event);
  d->event = 0;
  d->sample_count = 0;
}

