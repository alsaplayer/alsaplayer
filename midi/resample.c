/*
	$Id$

    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

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

#include <math.h>
#include <stdio.h>
#ifdef __FreeBSD__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "effects.h"
#include "md.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "resample.h"


#define LINEAR_INTERPOLATION
#define CSPLINE_INTERPOLATION
#define ENVELOPE_PITCH_MODULATION


#ifdef LOOKUP_HACK
#define MAX_DATAVAL 127
#define MIN_DATAVAL -128
#else
#define MAX_DATAVAL 32767
#define MIN_DATAVAL -32768
#endif

#define OVERSHOOT_STEP 50


/*sample_t resample_buffer[AUDIO_BUFFER_SIZE+100];*/
/*uint32 resample_buffer_offset = 0; */

static sample_t *vib_resample_voice(int, uint32 *, int, struct md *);
static sample_t *normal_resample_voice(int, uint32 *, int, struct md *);

/* Returns 1 if envelope runs out */
int recompute_modulation(int v, struct md *d)
{
  int stage;

  stage = d->voice[v].modulation_stage;

  if (stage>5)
    {
      /* Envelope ran out. */
      d->voice[v].modulation_increment = 0;
      return 1;
    }

  if ((d->voice[v].sample->modes & MODES_ENVELOPE) && (d->voice[v].sample->modes & MODES_SUSTAIN))
    {
      if (d->voice[v].status & (VOICE_ON | VOICE_SUSTAINED))
	{
	  if (stage>2)
	    {
	      /* Freeze modulation until note turns off. Trumpets want this. */
	      d->voice[v].modulation_increment=0;
	      return 0;
	    }
	}
    }
  d->voice[v].modulation_stage=stage+1;

#ifdef tplus
  if (d->voice[v].modulation_volume==(int)d->voice[v].sample->modulation_offset[stage] ||
      (stage > 2 && d->voice[v].modulation_volume < (int)d->voice[v].sample->modulation_offset[stage]))
#else
  if (d->voice[v].modulation_volume==d->voice[v].sample->modulation_offset[stage])
#endif
    return recompute_modulation(v, d);
  d->voice[v].modulation_target=d->voice[v].sample->modulation_offset[stage];
  d->voice[v].modulation_increment = d->voice[v].sample->modulation_rate[stage];
  if ((int)d->voice[v].modulation_target<d->voice[v].modulation_volume)
    d->voice[v].modulation_increment = -d->voice[v].modulation_increment;
  return 0;
}

int update_modulation(int v, struct md *d)
{

  if(d->voice[v].modulation_delay > 0)
  {
      /* d->voice[v].modulation_delay -= control_ratio; I think units are already
	 in terms of control_ratio */
      d->voice[v].modulation_delay -= 1;
      if(d->voice[v].modulation_delay > 0)
	  return 0;
  }


  d->voice[v].modulation_volume += d->voice[v].modulation_increment;
  if (d->voice[v].modulation_volume < 0) d->voice[v].modulation_volume = 0;
  /* Why is there no ^^ operator?? */
  if (((d->voice[v].modulation_increment < 0) &&
       (d->voice[v].modulation_volume <= (int)d->voice[v].modulation_target)) ||
      ((d->voice[v].modulation_increment > 0) &&
	   (d->voice[v].modulation_volume >= (int)d->voice[v].modulation_target)))
    {
      d->voice[v].modulation_volume = d->voice[v].modulation_target;
      if (recompute_modulation(v, d))
	return 1;
    }
  return 0;
}

/* Returns 1 if the note died */
int update_modulation_signal(int v, struct md *d)
{
  if (d->voice[v].modulation_increment && update_modulation(v, d))
    return 1;
  return 0;
}



/* modulation_volume has been set by above routine */
int32 calc_mod_freq(int v, int32 incr, struct md *d)
{
  FLOAT_T mod_amount;
  int32 freq;
  /* already done in update_vibrato ? */
  if (d->voice[v].vibrato_control_ratio) return incr;
  if ((mod_amount=d->voice[v].sample->modEnvToPitch)<0.02) return incr;
  if (incr < 0) return incr;
  freq = d->voice[v].frequency;
  freq = (int32)( (double)freq*(1.0 + (mod_amount - 1.0) * (d->voice[v].modulation_volume>>22) / 255.0) );

  return (int32) FRSCALE(((double)(d->voice[v].sample->sample_rate) *
		  (double)(freq)) /
		 ((double)(d->voice[v].sample->root_freq) *
		  (double)(play_mode->rate)),
		 FRACTION_BITS);
}

/*************** resampling with fixed increment *****************/

static sample_t *rs_plain(int v, uint32 *countptr, struct md *d)
{
  /* Play sample until end, then free the voice. */
  Voice
    *vp=&d->voice[v];
  int32   ofsd, v0, v1, v2, v3, temp, overshoot;
  int offset;
  uint32 cc_count=vp->modulation_counter;
  sample_t
    *dest=d->resample_buffer+d->resample_buffer_offset,
    *src=vp->sample->data;
  int32
    incr=vp->sample_increment;
  uint32
    ofs=vp->sample_offset;
#if defined(LAGRANGE_INTERPOLATION) || defined(CSPLINE_INTERPOLATION)
  uint32
    ls=0,
    le=vp->sample->data_length;
#endif /* LAGRANGE_INTERPOLATION */
  uint32
    se=vp->sample->data_length;
  uint32
    count=*countptr;

  if (!incr) return d->resample_buffer+d->resample_buffer_offset; /* --gl */

  overshoot = src[(se>>FRACTION_BITS)-1] / OVERSHOOT_STEP;
  if (overshoot < 0) overshoot = -overshoot;

    while (count--)
    {

	offset = ofs >> FRACTION_BITS;

	if (ofs >= se) {
		int32 delta = (int32)((ofs - se)>>FRACTION_BITS) ;
        	v1 = (int32)src[(int)(se>>FRACTION_BITS)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }else  v1 = (int32)src[offset];
	if (ofs + (1L<<FRACTION_BITS) >= se) {
		v2 = v1;
        }else  v2 = (int32)src[offset+1];
	if(d->dont_cspline ||
	   ((ofs-(1L<<FRACTION_BITS))<ls)||((ofs+(2L<<FRACTION_BITS))>le)){
                *dest++ = (sample_t)(v1 + ((int32)((v2-v1) * (int32)(ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}else{
		ofsd=ofs;
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofs &= FRACTION_MASK;
                temp=v2;
		v2 = (6*v2 +
		      (((( ( (5*v3 - 11*v2 + 7*v1 - v0)*
		       (int32)ofs) >>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
                ofs = (1L << FRACTION_BITS) - ofs;
		v1 = (6*v1 +
		      ((((((5*v0 - 11*v1 + 7*temp - v3)*
		       (int32)ofs)>>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
		v1 = (v1 + v2)/(6L<<FRACTION_BITS);
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
		ofs=ofsd;
	}
		if (!cc_count--) {
		    cc_count = control_ratio - 1;
		    if (!update_modulation_signal(v, d))
		        incr = calc_mod_freq(v, incr, d);
		}
      ofs += incr;
      if (ofs >= se + (overshoot << FRACTION_BITS))
	{
	  if (!(vp->status&VOICE_FREE))
	    {
	      vp->status=VOICE_FREE;
 	      ctl->note(v);
	    }
	  *countptr-=count+1;
	  break;
	}
    }

  vp->sample_offset=ofs; /* Update offset */
  vp->modulation_counter=cc_count;
  return d->resample_buffer+d->resample_buffer_offset;
}

static sample_t *rs_loop(int v, Voice *vp, uint32 *countptr, struct md *d)
{
  /* Play sample until end-of-loop, skip back and continue. */
  int32   ofsd, v0, v1, v2, v3, temp, overshoot;
  int offset;
  uint32 cc_count=vp->modulation_counter;
  int32
    incr=vp->sample_increment;
  uint32
    ofs=vp->sample_offset;
  uint32
    le=vp->loop_end,
#if defined(LAGRANGE_INTERPOLATION) || defined(CSPLINE_INTERPOLATION)
    ls=vp->loop_start,
#endif /* LAGRANGE_INTERPOLATION */
    ll=le - vp->loop_start;
  sample_t
    *dest=d->resample_buffer+d->resample_buffer_offset,
    *src=vp->sample->data;
  uint32
    se=vp->sample->data_length,
    count = *countptr;
  int
    flag_exit_loop;


  flag_exit_loop =
	(vp->status & (VOICE_FREE | VOICE_DIE)) ||
	((vp->status & VOICE_OFF) && (vp->sample->modes & MODES_FAST_RELEASE) ) ||
	((vp->status & VOICE_OFF) && d->dont_keep_looping ) ;

  overshoot = src[(se>>FRACTION_BITS)-1] / OVERSHOOT_STEP;
  if (overshoot < 0) overshoot = -overshoot;

  while (count--)
    {

	offset = ofs >> FRACTION_BITS;

	if (ofs >= se) {
		int32 delta = (int32)((ofs - se)>>FRACTION_BITS) ;
        	v1 = (int32)src[(int)(se>>FRACTION_BITS)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }else  v1 = (int32)src[offset];
	if (ofs + (1L<<FRACTION_BITS) >= se) {
		v2 = v1;
        }else  v2 = (int32)src[offset+1];
	if(d->dont_cspline ||
	   ((ofs-(1L<<FRACTION_BITS))<ls)||((ofs+(2L<<FRACTION_BITS))>le)){
                *dest++ = (sample_t)(v1 + ((int32)((v2-v1) * (int32)(ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}else{
		ofsd=ofs;
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofs &= FRACTION_MASK;
                temp=v2;
		v2 = (6*v2 +
		      (((( ( (5*v3 - 11*v2 + 7*v1 - v0)*
		       (int32)ofs) >>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
                ofs = (1L << FRACTION_BITS) - ofs;
		v1 = (6*v1 +
		      ((((((5*v0 - 11*v1 + 7*temp - v3)*
		       (int32)ofs)>>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
		v1 = (v1 + v2)/(6L<<FRACTION_BITS);
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
		ofs=ofsd;
	}
		if (!cc_count--) {
		    cc_count = control_ratio - 1;
		    if (!update_modulation_signal(v, d))
		        incr = calc_mod_freq(v, incr, d);
		}
      ofs += incr;
      if (ofs>=le)
	{
	  if (flag_exit_loop)
	    {
	    	vp->echo_delay -= ll >> FRACTION_BITS;
	  	if (vp->echo_delay >= 0) ofs -= ll;
	    }
	  else ofs -= ll; /* Hopefully the loop is longer than an increment. */
	}
      if (ofs >= se + (overshoot << FRACTION_BITS))
	{
	  if (!(vp->status&VOICE_FREE))
	    {
	      vp->status=VOICE_FREE;
 	      ctl->note(v);
	    }
	  *countptr-=count+1;
	  break;
	}
    }

  vp->sample_offset=ofs; /* Update offset */
  vp->modulation_counter=cc_count;
  return d->resample_buffer+d->resample_buffer_offset;
}

static sample_t *rs_bidir(int v, Voice *vp, uint32 count, struct md *d)
{
  int32   ofsd, v0, v1, v2, v3, temp, overshoot;
  int offset;
  int32
    incr=vp->sample_increment;
  uint32
    le=vp->loop_end,
    ls=vp->loop_start;
  uint32
    ofs=vp->sample_offset,
    se=vp->sample->data_length;
  sample_t
    *dest=d->resample_buffer+d->resample_buffer_offset,
    *src=vp->sample->data;


#ifdef USE_BIDIR_OVERSHOOT
  int32
    le2 = le<<1,
    ls2 = ls<<1;
#endif
  uint32
    i, j;
  /* Play normally until inside the loop region */

  overshoot = src[(se>>FRACTION_BITS)-1] / OVERSHOOT_STEP;
  if (overshoot < 0) overshoot = -overshoot;

  if (ofs <= ls)
    {
      /* NOTE: Assumes that incr > 0, which is NOT always the case
	 when doing bidirectional looping.  I have yet to see a case
	 where both ofs <= ls AND incr < 0, however. */
      if (incr < 0) i = ls - ofs;
	else
      i = (ls - ofs) / incr + 1;
      if (i > count)
	{
	  i = count;
	  count = 0;
	}
      else count -= i;
      for(j = 0; j < i; j++)
	{

	offset = ofs >> FRACTION_BITS;

	if (ofs >= se) {
		int32 delta = (int32)((ofs - se)>>FRACTION_BITS) ;
        	v1 = (int32)src[(int)(se>>FRACTION_BITS)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }else  v1 = (int32)src[offset];
	if (ofs + (1L<<FRACTION_BITS) >= se) {
		v2 = v1;
        }else  v2 = (int32)src[offset+1];
	if(d->dont_cspline ||
	   ((ofs-(1L<<FRACTION_BITS))<ls)||((ofs+(2L<<FRACTION_BITS))>le)){
                *dest++ = (sample_t)(v1 + ((int32)((v2-v1) * (int32)(ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}else{
		ofsd=ofs;
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofs &= FRACTION_MASK;
                temp=v2;
		v2 = (6*v2 +
		      (((( ( (5*v3 - 11*v2 + 7*v1 - v0)*
		       (int32)ofs) >>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
                ofs = (1L << FRACTION_BITS) - ofs;
		v1 = (6*v1 +
		      ((((((5*v0 - 11*v1 + 7*temp - v3)*
		       (int32)ofs)>>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
		v1 = (v1 + v2)/(6L<<FRACTION_BITS);
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
		ofs=ofsd;
	}
	  ofs += incr;
	}
    }

  /* Then do the bidirectional looping */

  while(count)
    {
      /* Precalc how many times we should go through the loop */
#if 1
      i = ((incr > 0 ? le : ls) - ofs) / incr + 1;
#else
/* fix from M. Izumo */
      i = ((incr > 0 ? le : ls) - ofs + incr - 1) / incr;
#endif
      if (i > count)
	{
	  i = count;
	  count = 0;
	}
      else count -= i;
      for(j = 0; j < i && ofs < se; j++)
	{

	offset = ofs >> FRACTION_BITS;

	if (ofs >= se) {
		int32 delta = (int32)((ofs - se)>>FRACTION_BITS) ;
        	v1 = (int32)src[(int)(se>>FRACTION_BITS)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }else  v1 = (int32)src[offset];
	if (ofs + (1L<<FRACTION_BITS) >= se) {
		v2 = v1;
        }else  v2 = (int32)src[offset+1];
	if(d->dont_cspline ||
	   ((ofs-(1L<<FRACTION_BITS))<ls)||((ofs+(2L<<FRACTION_BITS))>le)){
                *dest++ = (sample_t)(v1 + ((int32)((v2-v1) * (int32)(ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}else{
		ofsd=ofs;
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofs &= FRACTION_MASK;
                temp=v2;
		v2 = (6*v2 +
		      (((( ( (5*v3 - 11*v2 + 7*v1 - v0)*
		       (int32)ofs) >>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
                ofs = (1L << FRACTION_BITS) - ofs;
		v1 = (6*v1 +
		      ((((((5*v0 - 11*v1 + 7*temp - v3)*
		       (int32)ofs)>>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
		v1 = (v1 + v2)/(6L<<FRACTION_BITS);
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
		ofs=ofsd;
	}
	  ofs += incr;
	}
#ifdef USE_BIDIR_OVERSHOOT
      if (ofs>=le)
	{
	  /* fold the overshoot back in */
	  ofs = le2 - ofs;
	  incr *= -1;
	}
      else if (ofs <= ls)
	{
	  ofs = ls2 - ofs;
	  incr *= -1;
	}
#else
	  incr *= -1;
#endif
    }

  vp->sample_increment=incr;
  vp->sample_offset=ofs; /* Update offset */
  return d->resample_buffer+d->resample_buffer_offset;
}

/*********************** vibrato versions ***************************/

/* We only need to compute one half of the vibrato sine cycle */
static uint32 vib_phase_to_inc_ptr(uint32 phase, struct md *d)
{
  if (phase < VIBRATO_SAMPLE_INCREMENTS/2)
    return VIBRATO_SAMPLE_INCREMENTS/2-1-phase;
  else if (phase >= 3*VIBRATO_SAMPLE_INCREMENTS/2)
    return 5*VIBRATO_SAMPLE_INCREMENTS/2-1-phase;
  else
    return phase-VIBRATO_SAMPLE_INCREMENTS/2;
}

static int32 update_vibrato(Voice *vp, int sign, struct md *d)
{
  uint32 depth, freq=vp->frequency;
#ifdef ENVELOPE_PITCH_MODULATION
  FLOAT_T mod_amount=vp->sample->modEnvToPitch;
#endif
  uint32 phase;
  int pb;
  double a;

  if(vp->vibrato_delay > 0)
  {
      vp->vibrato_delay -= vp->vibrato_control_ratio;
      if(vp->vibrato_delay > 0)
	  return vp->sample_increment;
  }

  if (vp->vibrato_phase++ >= 2*VIBRATO_SAMPLE_INCREMENTS-1)
    vp->vibrato_phase=0;
  phase=vib_phase_to_inc_ptr(vp->vibrato_phase, d);

  if (vp->vibrato_sample_increment[phase])
    {
      if (sign)
	return -vp->vibrato_sample_increment[phase];
      else
	return vp->vibrato_sample_increment[phase];
    }

  /* Need to compute this sample increment. */

  depth = vp->vibrato_depth;
  if(depth < vp->modulation_wheel)
      depth = vp->modulation_wheel;
  depth <<= 7;

  if (vp->vibrato_sweep && !vp->modulation_wheel)
    {
      /* Need to update sweep */
      vp->vibrato_sweep_position += vp->vibrato_sweep;
      if (vp->vibrato_sweep_position >= (1<<SWEEP_SHIFT))
	vp->vibrato_sweep=0;
      else
	{
	  /* Adjust depth */
	  depth *= vp->vibrato_sweep_position;
	  depth >>= SWEEP_SHIFT;
	}
    }

#ifdef ENVELOPE_PITCH_MODULATION
#ifndef FILTER_INTERPOLATION
  if (update_modulation_signal(0, d)) mod_amount = 0;
  else
#endif
  if (mod_amount>0.02)
   freq = (int32)( (double)freq*(1.0 + (mod_amount - 1.0) * (vp->modulation_volume>>22) / 255.0) );
#endif

  pb=(int)((sine(vp->vibrato_phase *
			(SINE_CYCLE_LENGTH/(2*VIBRATO_SAMPLE_INCREMENTS)))
	    * (double)(depth) * VIBRATO_AMPLITUDE_TUNING));

  a = FRSCALE(((double)(vp->sample->sample_rate) *
		  (double)(freq)) /
		 ((double)(vp->sample->root_freq) *
		  (double)(play_mode->rate)),
		 FRACTION_BITS);
  if(pb<0)
  {
      pb = -pb;
      a /= bend_fine[(pb>>5) & 0xFF] * bend_coarse[pb>>13];
  }
  else
      a *= bend_fine[(pb>>5) & 0xFF] * bend_coarse[pb>>13];
  a += 0.5;

  /* If the sweep's over, we can store the newly computed sample_increment */
  if (!vp->vibrato_sweep || vp->modulation_wheel)
    vp->vibrato_sample_increment[phase]=(int32) a;

  if (sign)
    a = -a; /* need to preserve the loop direction */

  return (int32) a;
}

static sample_t *rs_vib_plain(int v, uint32 *countptr, struct md *d)
{

  /* Play sample until end, then free the voice. */

  Voice *vp=&d->voice[v];
  int32   ofsd, v0, v1, v2, v3, temp, overshoot;
  int offset;
  sample_t
    *dest=d->resample_buffer+d->resample_buffer_offset,
    *src=vp->sample->data;
  int32
    incr=vp->sample_increment;
/*WHY int32??*/
#if defined(LAGRANGE_INTERPOLATION) || defined(CSPLINE_INTERPOLATION)
  uint32
    ls=0,
    le=vp->sample->data_length;
#endif /* LAGRANGE_INTERPOLATION */
  uint32
    ofs=vp->sample_offset,
    se=vp->sample->data_length,
    count=*countptr;
  uint32
    cc=vp->vibrato_control_counter;

  /* This has never been tested */

  if (incr<0) incr = -incr; /* In case we're coming out of a bidir loop */

  overshoot = src[(se>>FRACTION_BITS)-1] / OVERSHOOT_STEP;
  if (overshoot < 0) overshoot = -overshoot;
  while (count--)
    {
      if (!cc--)
	{
	  cc=vp->vibrato_control_ratio;
	  incr=update_vibrato(vp, 0, d);
	}

	offset = ofs >> FRACTION_BITS;

	if (ofs >= se) {
		int32 delta = (int32)((ofs - se)>>FRACTION_BITS) ;
        	v1 = (int32)src[(int)(se>>FRACTION_BITS)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }else  v1 = (int32)src[offset];
	if (ofs + (1L<<FRACTION_BITS) >= se) {
		v2 = v1;
        }else  v2 = (int32)src[offset+1];
	if(d->dont_cspline ||
	   ((ofs-(1L<<FRACTION_BITS))<ls)||((ofs+(2L<<FRACTION_BITS))>le)){
                *dest++ = (sample_t)(v1 + ((int32)((v2-v1) * (int32)(ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}else{
		ofsd=ofs;
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofs &= FRACTION_MASK;
                temp=v2;
		v2 = (6*v2 +
		      (((( ( (5*v3 - 11*v2 + 7*v1 - v0)*
		       (int32)ofs) >>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
                ofs = (1L << FRACTION_BITS) - ofs;
		v1 = (6*v1 +
		      ((((((5*v0 - 11*v1 + 7*temp - v3)*
		       (int32)ofs)>>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
		v1 = (v1 + v2)/(6L<<FRACTION_BITS);
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
		ofs=ofsd;
	}
      ofs += incr;
      if (ofs >= se + (overshoot << FRACTION_BITS))
	{
	  if (!(vp->status&VOICE_FREE))
	    {
	      vp->status=VOICE_FREE;
 	      ctl->note(v);
	    }
	  *countptr-=count+1;
	  break;
	}
    }

  vp->vibrato_control_counter=cc;
  vp->sample_increment=incr;
  vp->sample_offset=ofs; /* Update offset */
  return d->resample_buffer+d->resample_buffer_offset;
}

static sample_t *rs_vib_loop(int v, Voice *vp, uint32 *countptr, struct md *d)
{
  /* Play sample until end-of-loop, skip back and continue. */
  int32   ofsd, v0, v1, v2, v3, temp, overshoot;
  int offset;
  int32
    incr=vp->sample_increment;
/*WHY int32??*/
  uint32
#if defined(LAGRANGE_INTERPOLATION) || defined(CSPLINE_INTERPOLATION)
    ls=vp->loop_start,
#endif /* LAGRANGE_INTERPOLATION */
    le=vp->loop_end,
    ll=le - vp->loop_start;
  sample_t
    *dest=d->resample_buffer+d->resample_buffer_offset,
    *src=vp->sample->data;
  uint32
    ofs=vp->sample_offset,
    se=vp->sample->data_length,
    count = *countptr;
  uint32
    cc=vp->vibrato_control_counter;
  int
    flag_exit_loop;


  flag_exit_loop =
	(vp->status & (VOICE_FREE | VOICE_DIE)) ||
	((vp->status & VOICE_OFF) && (vp->sample->modes & MODES_FAST_RELEASE) ) ||
	((vp->status & VOICE_OFF) && d->dont_keep_looping ) ;

  overshoot = src[(se>>FRACTION_BITS)-1] / OVERSHOOT_STEP;
  if (overshoot < 0) overshoot = -overshoot;
  while (count--)
    {
      if (!cc--)
	{
	  cc=vp->vibrato_control_ratio;
	  incr=update_vibrato(vp, 0, d);
	}

	offset = ofs >> FRACTION_BITS;

	if (ofs >= se) {
		int32 delta = (int32)((ofs - se)>>FRACTION_BITS) ;
        	v1 = (int32)src[(int)(se>>FRACTION_BITS)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }else  v1 = (int32)src[offset];
	if (ofs + (1L<<FRACTION_BITS) >= se) {
		v2 = v1;
        }else  v2 = (int32)src[offset+1];
	if(d->dont_cspline ||
	   ((ofs-(1L<<FRACTION_BITS))<ls)||((ofs+(2L<<FRACTION_BITS))>le)){
                *dest++ = (sample_t)(v1 + ((int32)((v2-v1) * (int32)(ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}else{
		ofsd=ofs;
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofs &= FRACTION_MASK;
                temp=v2;
		v2 = (6*v2 +
		      (((( ( (5*v3 - 11*v2 + 7*v1 - v0)*
		       (int32)ofs) >>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
                ofs = (1L << FRACTION_BITS) - ofs;
		v1 = (6*v1 +
		      ((((((5*v0 - 11*v1 + 7*temp - v3)*
		       (int32)ofs)>>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
		v1 = (v1 + v2)/(6L<<FRACTION_BITS);
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
		ofs=ofsd;
	}
      ofs += incr;
      if (ofs>=le)
	{
	  if (flag_exit_loop)
	    {
	    	vp->echo_delay -= ll >> FRACTION_BITS;
	  	if (vp->echo_delay >= 0) ofs -= ll;
	    }
	  else ofs -= ll; /* Hopefully the loop is longer than an increment. */
	}
      if (ofs >= se + (overshoot << FRACTION_BITS))
	{
	  if (!(vp->status&VOICE_FREE))
	    {
	      vp->status=VOICE_FREE;
 	      ctl->note(v);
	    }
	  *countptr-=count+1;
	  break;
	}
    }

  vp->vibrato_control_counter=cc;
  vp->sample_increment=incr;
  vp->sample_offset=ofs; /* Update offset */
  return d->resample_buffer+d->resample_buffer_offset;
}

static sample_t *rs_vib_bidir(int v, Voice *vp, uint32 count, struct md *d)
{
  int32   ofsd, v0, v1, v2, v3, temp, overshoot;
  int offset;
  int32
    incr=vp->sample_increment;
/*WHY int32??*/
  uint32
    le=vp->loop_end,
    ls=vp->loop_start;
  uint32
    ofs=vp->sample_offset,
    se=vp->sample->data_length;
  sample_t
    *dest=d->resample_buffer+d->resample_buffer_offset,
    *src=vp->sample->data;
  uint32
    cc=vp->vibrato_control_counter;


#ifdef USE_BIDIR_OVERSHOOT
  uint32
    le2=le<<1,
    ls2=ls<<1;
#endif
  uint32
    i, j;
  int
    vibflag = 0;


  overshoot = src[(se>>FRACTION_BITS)-1] / OVERSHOOT_STEP;
  if (overshoot < 0) overshoot = -overshoot;
  /* Play normally until inside the loop region */
  while (count && (ofs <= ls))
    {
      i = (ls - ofs) / incr + 1;
      if (i > count) i = count;
      if (i > cc)
	{
	  i = cc;
	  vibflag = 1;
	}
      else cc -= i;
      count -= i;
      for(j = 0; j < i; j++)
	{

	offset = ofs >> FRACTION_BITS;

	if (ofs >= se) {
		int32 delta = (int32)((ofs - se)>>FRACTION_BITS) ;
        	v1 = (int32)src[(int)(se>>FRACTION_BITS)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }else  v1 = (int32)src[offset];
	if (ofs + (1L<<FRACTION_BITS) >= se) {
		v2 = v1;
        }else  v2 = (int32)src[offset+1];
	if(d->dont_cspline ||
	   ((ofs-(1L<<FRACTION_BITS))<ls)||((ofs+(2L<<FRACTION_BITS))>le)){
                *dest++ = (sample_t)(v1 + ((int32)((v2-v1) * (int32)(ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}else{
		ofsd=ofs;
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofs &= FRACTION_MASK;
                temp=v2;
		v2 = (6*v2 +
		      (((( ( (5*v3 - 11*v2 + 7*v1 - v0)*
		       (int32)ofs) >>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
                ofs = (1L << FRACTION_BITS) - ofs;
		v1 = (6*v1 +
		      ((((((5*v0 - 11*v1 + 7*temp - v3)*
		       (int32)ofs)>>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
		v1 = (v1 + v2)/(6L<<FRACTION_BITS);
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
		ofs=ofsd;
	}
	  ofs += incr;
	}
      if (vibflag)
	{
	  cc = vp->vibrato_control_ratio;
	  incr = update_vibrato(vp, 0, d);
	  vibflag = 0;
	}
    }

  /* Then do the bidirectional looping */

  while (count)
    {
      /* Precalc how many times we should go through the loop */
#if 1
      i = ((incr > 0 ? le : ls) - ofs) / incr + 1;
#else
/* fix from M. Izumo */
      i = ((incr > 0 ? le : ls) - ofs + incr - 1) / incr;
#endif
      if(i > count) i = count;
      if(i > cc)
	{
	  i = cc;
	  vibflag = 1;
	}
      else cc -= i;
      count -= i;
      while (i-- && ofs < se)
	{

	offset = ofs >> FRACTION_BITS;

	if (ofs >= se) {
		int32 delta = (int32)((ofs - se)>>FRACTION_BITS) ;
        	v1 = (int32)src[(int)(se>>FRACTION_BITS)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }else  v1 = (int32)src[offset];
	if (ofs + (1L<<FRACTION_BITS) >= se) {
		v2 = v1;
        }else  v2 = (int32)src[offset+1];
	if(d->dont_cspline ||
	   ((ofs-(1L<<FRACTION_BITS))<ls)||((ofs+(2L<<FRACTION_BITS))>le)){
                *dest++ = (sample_t)(v1 + ((int32)((v2-v1) * (int32)(ofs & FRACTION_MASK)) >> FRACTION_BITS));
	}else{
		ofsd=ofs;
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofs &= FRACTION_MASK;
                temp=v2;
		v2 = (6*v2 +
		      (((( ( (5*v3 - 11*v2 + 7*v1 - v0)*
		       (int32)ofs) >>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
                ofs = (1L << FRACTION_BITS) - ofs;
		v1 = (6*v1 +
		      ((((((5*v0 - 11*v1 + 7*temp - v3)*
		       (int32)ofs)>>FRACTION_BITS)*(int32)ofs)>>(FRACTION_BITS+2))-1))*(int32)ofs;
		v1 = (v1 + v2)/(6L<<FRACTION_BITS);
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
		ofs=ofsd;
	}
	  ofs += incr;
	}
      if (vibflag)
	{
	  cc = vp->vibrato_control_ratio;
	  incr = update_vibrato(vp, (incr < 0), d);
	  vibflag = 0;
	}
      if (ofs >= le)
	{
#ifdef USE_BIDIR_OVERSHOOT
	  /* fold the overshoot back in */
	  ofs = le2 - ofs;
#endif
	  incr *= -1;
	}
      else if (ofs <= ls)
	{
#ifdef USE_BIDIR_OVERSHOOT
	  ofs = ls2 - ofs;
#endif
	  incr *= -1;
	}
    }


  vp->vibrato_control_counter=cc;
  vp->sample_increment=incr;
  vp->sample_offset=ofs; /* Update offset */
  return d->resample_buffer+d->resample_buffer_offset;
}

static int rs_update_porta(int v, struct md *d)
{
    Voice *vp=&d->voice[v];
    int32 db;

    db = vp->porta_dpb;
    if(vp->porta_pb < 0)
    {
	if(db > -vp->porta_pb)
	    db = -vp->porta_pb;
    }
    else
    {
	if(db > vp->porta_pb)
	    db = -vp->porta_pb;
	else
	    db = -db;
    }

    vp->porta_pb += db;
    if(vp->porta_pb == 0)
    {
	vp->porta_control_ratio = 0;
	vp->porta_pb = 0;
    }
    recompute_freq(v, d);
    return vp->porta_control_ratio;
}

static sample_t *porta_resample_voice(int v, uint32 *countptr, int mode, struct md *d)
{
    Voice *vp=&d->voice[v];
    uint32 n = *countptr;
    uint32 i;
    sample_t *(* resampler)(int, uint32 *, int, struct md *);
    int cc = vp->porta_control_counter;
    int loop;

    if(vp->vibrato_control_ratio)
	resampler = vib_resample_voice;
    else
	resampler = normal_resample_voice;
    if(mode != 1)
	loop = 1;
    else
	loop = 0;

    /* vp->cache = NULL; */
    d->resample_buffer_offset = 0;
    while(d->resample_buffer_offset < n)
    {
	if(cc == 0)
	{
	    if((cc = rs_update_porta(v, d)) == 0)
	    {
		i = n - d->resample_buffer_offset;
		resampler(v, &i, mode, d);
		d->resample_buffer_offset += i;
		break;
	    }
	}

	i = n - d->resample_buffer_offset;
	if(i > (uint32)cc)
	    i = (uint32)cc;
	resampler(v, &i, mode, d);
	d->resample_buffer_offset += i;

	/* if(!loop && vp->status == VOICE_FREE) */
	if(vp->status == VOICE_FREE)
	    break;
	cc -= (int)i;
    }
    *countptr = d->resample_buffer_offset;
    d->resample_buffer_offset = 0;
    vp->porta_control_counter = cc;
    return d->resample_buffer;
}

static sample_t *vib_resample_voice(int v, uint32 *countptr, int mode, struct md *d)
{
    Voice *vp = &d->voice[v];

    /* vp->cache = NULL; */
    if(mode == 0)
	return rs_vib_loop(v, vp, countptr, d);
    if(mode == 1)
	return rs_vib_plain(v, countptr, d);
    return rs_vib_bidir(v, vp, *countptr, d);
}

static sample_t *normal_resample_voice(int v, uint32 *countptr, int mode, struct md *d)
{
    Voice *vp = &d->voice[v];
    if(mode == 0)
	return rs_loop(v, vp, countptr, d);
    if(mode == 1)
	return rs_plain(v, countptr, d);
    return rs_bidir(v, vp, *countptr, d);
}

sample_t *resample_voice(int v, uint32 *countptr, struct md *d)
{
    Voice *vp=&d->voice[v];
    int mode;

    d->resample_buffer_offset = 0;

    if(!(vp->sample->sample_rate))
    {
	uint32 ofs;

	/* Pre-resampled data -- just update the offset and check if
	   we're out of data. */
	ofs=vp->sample_offset >> FRACTION_BITS; /* Kind of silly to use
						   FRACTION_BITS here... */
	if(*countptr >= (vp->sample->data_length>>FRACTION_BITS) - ofs)
	{
	    /* Note finished. Free the voice. */
	  if (!(vp->status&VOICE_FREE))
	    {
	      vp->status=VOICE_FREE;
 	      ctl->note(v);
	    }

	    /* Let the caller know how much data we had left */
	    *countptr = (vp->sample->data_length>>FRACTION_BITS) - ofs;
	}
	else
	    vp->sample_offset += *countptr << FRACTION_BITS;
	return vp->sample->data+ofs;
    }

    if (d->current_interpolation == 2)
	 return resample_voice_lagrange(v, countptr, d);
    else if (d->current_interpolation == 3)
	 return resample_voice_filter(v, countptr, d);

    mode = vp->sample->modes;
    if((mode & MODES_LOOPING) &&
       ((mode & MODES_ENVELOPE) ||
	(vp->status & (VOICE_ON | VOICE_SUSTAINED))))
    {
	if(mode & MODES_PINGPONG)
	{
	    /* vp->cache = NULL; */
	    mode = 2;
	}
	else
	    mode = 0;
    }
    else
	mode = 1;

    if(vp->porta_control_ratio)
	return porta_resample_voice(v, countptr, mode, d);

    if(vp->vibrato_control_ratio)
	return vib_resample_voice(v, countptr, mode, d);

    return normal_resample_voice(v, countptr, mode, d);
}


#ifdef TOTALLY_LOCAL
void do_lowpass(Sample *sample, uint32 srate, sample_t *buf, uint32 count, uint32 freq, FLOAT_T resonance)
{
    double a0=0, a1=0, a2=0, b0=0, b1=0;
    double x0=0, x1=0, y0=0, y1=0;
    sample_t samp;
    double outsamp, insamp, mod_amount=0;
    uint32 findex, cc;
    uint32 current_freq;

    if (freq < 20) return;

    if (freq > srate * 2) {
    	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
    		  "Lowpass: center must be < data rate*2");
    	return;
    }

    current_freq = freq;

    if (sample->modEnvToFilterFc)
	mod_amount = sample->modEnvToFilterFc;

    if (mod_amount < 0.02 && freq >= 13500) return;

    d->voice[0].sample = sample;

      /* Ramp up from 0 */
    d->voice[0].modulation_stage=ATTACK;
    d->voice[0].modulation_volume=0;
    d->voice[0].modulation_delay=sample->modulation_rate[DELAY];
    cc = d->voice[0].modulation_counter=0;
#ifdef TOTALLY_LOCAL
    recompute_modulation(0, d);
#endif

/* start modulation loop here */
    while (count--) {

	if (!cc) {
	    if (mod_amount>0.02) {
#ifdef TOTALLY_LOCAL
		if (update_modulation_signal(0, d)) mod_amount = 0;
		else
		current_freq = (uint32)( (double)freq*(1.0 + (mod_amount - 1.0) *
			 (d->voice[0].modulation_volume>>22) / 255.0) );
#endif
	    }
	    cc = control_ratio;
	    findex = 1 + (current_freq+50) / 100;
	    if (findex > 100) findex = 100;
	    a0 = butterworth[findex][0];
	    a1 = butterworth[findex][1];
	    a2 = butterworth[findex][2];
	    b0 = butterworth[findex][3];
	    b1 = butterworth[findex][4];
	}

	if (mod_amount>0.02) cc--;

    	samp = *buf;

    	insamp = (double)samp;
    	outsamp = a0 * insamp + a1 * x0 + a2 * x1 - b0 * y0 - b1 * y1;
    	x1 = x0;
    	x0 = insamp;
    	y1 = y0;
    	y0 = outsamp;
    	if (outsamp > MAX_DATAVAL) {
    		outsamp = MAX_DATAVAL;
    	}
    	else if (outsamp < MIN_DATAVAL) {
    		outsamp = MIN_DATAVAL;
    	}
    	*buf++ = (sample_t)outsamp;
    }
}
#endif

void pre_resample(Sample * sp)
{
  double a, xdiff;
  uint32 i, incr, ofs, newlen, count, overshoot;
  int16 *newdata, *dest, *src = (int16 *)sp->data, *vptr, *endptr;
  int32 v1, v2, v3, v4;
  static const char note_name[12][3] =
  {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
  };

  ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * pre-resampling for note %d (%s%d)",
	    sp->note_to_use,
	    note_name[sp->note_to_use % 12], (sp->note_to_use & 0x7F) / 12);

  if (sp->sample_rate == play_mode->rate && sp->root_freq == freq_table[(int)(sp->note_to_use)]) {
	a = 1;
  }
  else a = ((double) (sp->sample_rate) * freq_table[(int) (sp->note_to_use)]) /
    ((double) (sp->root_freq) * play_mode->rate);

  /* if (a<1.0) return; */
  if(sp->data_length / a >= 0x7fffffffL)
  {
      /* Too large to compute */
      ctl->cmsg(CMSG_INFO, VERB_DEBUG, " *** Can't pre-resampling for note %d",
		sp->note_to_use);
      return;
  }

  endptr = src + (sp->data_length >> FRACTION_BITS) - 1;
  if (*endptr < 0) overshoot = (uint32)(-(*endptr / OVERSHOOT_STEP));
  else overshoot = (uint32)(*endptr / OVERSHOOT_STEP);
  if (overshoot < 2) overshoot = 0;

  newlen = (int32)(sp->data_length / a);
  count = (newlen >> FRACTION_BITS) - 1;
  ofs = incr = (sp->data_length - (1 << FRACTION_BITS)) / count;

  if((double)newlen + incr >= 0x7fffffffL)
  {
      /* Too large to compute */
      ctl->cmsg(CMSG_INFO, VERB_DEBUG, " *** Can't pre-resampling for note %d",
		sp->note_to_use);
      return;
  }

  dest = newdata = (int16 *)safe_malloc((newlen >> (FRACTION_BITS - 1)) + 2 + 2*overshoot);

  if (--count)
    *dest++ = src[0];

  /* Since we're pre-processing and this doesn't have to be done in
     real-time, we go ahead and do the full sliding cubic interpolation. */
  count--;
  for(i = 0; i < count + overshoot; i++)
    {
      vptr = src + (ofs >> FRACTION_BITS);
      if (i < count - 2 || !overshoot)
	{
          v1 = *(vptr - 1);
          v2 = *vptr;
          v3 = *(vptr + 1);
          v4 = *(vptr + 2);
	}
      else
	{
	  if (i < count + 1) v1 = *(vptr - 1);
	  else v1 = *endptr - (count-i+2) * *endptr / overshoot;
	  if (i < count) v2 = *vptr;
	  else v2 = *endptr - (count-i+1) * *endptr / overshoot;
	  if (i < count - 1) v3 = *(vptr + 1);
	  else v3 = *endptr - (count-i) * *endptr / overshoot;
	  v4 = *endptr - (count-i-1) * *endptr / overshoot;
	}
      xdiff = FRSCALENEG((int32)(ofs & FRACTION_MASK), FRACTION_BITS);
      *dest++ = v2 + (xdiff / 6.0) * (-2 * v1 - 3 * v2 + 6 * v3 - v4 +
      xdiff * (3 * (v1 - 2 * v2 + v3) + xdiff * (-v1 + 3 * (v2 - v3) + v4)));
      ofs += incr;
    }

 if (!overshoot)
  {
  if ((int32)(ofs & FRACTION_MASK))
    {
      v1 = src[ofs >> FRACTION_BITS];
      v2 = src[(ofs >> FRACTION_BITS) + 1];
      *dest++ = (int16)(v1 + ((int32)((v2 - v1) * (ofs & FRACTION_MASK)) >> FRACTION_BITS));
    }
  else
    *dest++ = src[ofs >> FRACTION_BITS];
  *dest++ = *(dest - 1) / 2;
  *dest++ = *(dest - 1) / 2;
  }

  sp->data_length = newlen + (overshoot << FRACTION_BITS);
  sp->loop_start = (int32)(sp->loop_start / a);
  sp->loop_end = (int32)(sp->loop_end / a);
  free(sp->data);
  sp->data = (sample_t *) newdata;
  sp->sample_rate = 0;
}
