/*
	$Id$

    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

 * Copyright (C) 2002 Greg Lee <greg@ling.lll.hawaii.edu>
 *
 *  This file is part of the MIDI input plugin for AlsaPlayer.
 *
 *  The MIDI input plugin for AlsaPlayer is free software;
 *  you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software
 *  Foundation; either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  The MIDI input plugin for AlsaPlayer is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
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
#define LAGRANGE_INTERPOLATION
#define ENVELOPE_PITCH_MODULATION


#ifdef LOOKUP_HACK
#define MAX_DATAVAL 127
#define MIN_DATAVAL -128
#else
#define MAX_DATAVAL 32767
#define MIN_DATAVAL -32768
#endif

#define OVERSHOOT_STEP 50

#define WAVE_ONE (1L<<FRACTION_BITS)
#define WAVE_TWO (2L<<FRACTION_BITS)
#define WAVE_SIX (6L<<FRACTION_BITS)
#define SAMP(a) ((a)>>FRACTION_BITS)
#define WAVE(a) ((a)<<FRACTION_BITS)


static sample_t *vib_resample_voice(int, uint32 *, int, struct md *);
static sample_t *normal_resample_voice(int, uint32 *, int, struct md *);


#ifdef FILTER_INTERPOLATION
static void update_lfo(int v, struct md *d)
{
  FLOAT_T depth=d->voice[v].modLfoToFilterFc;

  if (d->voice[v].lfo_sweep)
    {
      /* Update sweep position */

      d->voice[v].lfo_sweep_position += d->voice[v].lfo_sweep;
      if (d->voice[v].lfo_sweep_position>=(1<<SWEEP_SHIFT))
	d->voice[v].lfo_sweep=0; /* Swept to max amplitude */
      else
	{
	  /* Need to adjust depth */
	  depth *= (FLOAT_T)d->voice[v].lfo_sweep_position / (FLOAT_T)(1<<SWEEP_SHIFT);
	}
    }

  d->voice[v].lfo_phase += d->voice[v].lfo_phase_increment;

  d->voice[v].lfo_volume = depth;
}
#endif


#ifdef FILTER_INTERPOLATION
static int calc_bw_index(int v, struct md *d)
{
  FLOAT_T mod_amount=d->voice[v].modEnvToFilterFc;
  int32 freq = d->voice[v].sample->cutoff_freq;
  int ix;

  if (d->voice[v].lfo_phase_increment) update_lfo(v, d);

  if (!d->voice[v].lfo_phase_increment && update_modulation_signal(v, d)) return 0;

/* printf("mod_amount %f ", mod_amount); */
  if (d->voice[v].lfo_volume>0.001) {
	if (mod_amount) mod_amount *= d->voice[v].lfo_volume;
	else mod_amount = d->voice[v].lfo_volume;
/* printf("lfo %f -> mod %f ", d->voice[v].lfo_volume, mod_amount); */
  }

  if (mod_amount > 0.001) {
    if (d->voice[v].modulation_volume)
       freq =
	(int32)( (double)freq*(1.0 + (mod_amount - 1.0) * (d->voice[v].modulation_volume>>22) / 255.0) );
    else freq = (int32)( (double)freq*mod_amount );
/*
printf("v%d freq %d (was %d), modvol %d, mod_amount %f\n", v, (int)freq, (int)d->voice[v].sample->cutoff_freq,
(int)d->voice[v].modulation_volume>>22,
mod_amount);
*/
	ix = 1 + (freq+50) / 100;
	if (ix > 100) ix = 100;
	d->voice[v].bw_index = ix;
	return 1;
  }
  d->voice[v].bw_index = 1 + (freq+50) / 100;
  return 0;
}
#endif

/*************** resampling with fixed increment *****************/

static sample_t *rs_plain(int v, uint32 *countptr, struct md *d)
{
  /* Play sample until end, then free the voice. */
  Voice
    *vp=&d->voice[v];
   int32   ofsd, v0, v1, v2, v3, overshoot;
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

	offset = SAMP(ofs);

	if (ofs >= se) {
		int32 delta = SAMP(ofs - se);
        	v1 = (int32)src[(int)SAMP(se)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }
	else  v1 = (int32)src[offset];

	if (ofs + WAVE_ONE >= se) {
		v2 = v1;
        }
	else  v2 = (int32)src[offset+1];

	if (         d->dont_cspline ||
	     ((ofs - WAVE_ONE) < ls) ||
	     ((ofs + WAVE_TWO) > le)    ) {

                *dest++ = (sample_t)(
				v1 + (
				       (int32)(
						(v2-v1) * (ofs & FRACTION_MASK)
					      ) >> FRACTION_BITS
				     )
				    );
	}
	else {
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofsd = (int32)(ofs & FRACTION_MASK) + WAVE_ONE;
                v1 = v1 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v1 = v1 * SAMP(ofsd);
                v3 = v3 * ofsd;
                ofsd -= WAVE_ONE;
                v0 = (v3 - v0 * ofsd) / WAVE_SIX;
                v1 = (v1 - v2) * ofsd>>(FRACTION_BITS+1);
		v1 += v0;
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
	}

	if (!cc_count--) {
	    cc_count = control_ratio - 1;
	    incr = update_modulation_signal(v, incr, d);
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
   int32   ofsd, v0, v1, v2, v3, overshoot;
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

	offset = SAMP(ofs);

	if (ofs >= se) {
		int32 delta = SAMP(ofs - se);
        	v1 = (int32)src[(int)SAMP(se)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }
	else  v1 = (int32)src[offset];

	if (ofs + WAVE_ONE >= se) {
		v2 = v1;
        }
	else  v2 = (int32)src[offset+1];

	if (         d->dont_cspline ||
	     ((ofs - WAVE_ONE) < ls) ||
	     ((ofs + WAVE_TWO) > le)    ) {

                *dest++ = (sample_t)(
				v1 + (
				       (int32)(
						(v2-v1) * (ofs & FRACTION_MASK)
					      ) >> FRACTION_BITS
				     )
				    );
	}
	else {
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofsd = (int32)(ofs & FRACTION_MASK) + WAVE_ONE;
                v1 = v1 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v1 = v1 * SAMP(ofsd);
                v3 = v3 * ofsd;
                ofsd -= WAVE_ONE;
                v0 = (v3 - v0 * ofsd) / WAVE_SIX;
                v1 = (v1 - v2) * ofsd>>(FRACTION_BITS+1);
		v1 += v0;
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
	}

	if (!cc_count--) {
	    cc_count = control_ratio - 1;
	    incr = update_modulation_signal(v, incr, d);
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
   int32   ofsd, v0, v1, v2, v3, overshoot;
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

	offset = SAMP(ofs);

	if (ofs >= se) {
		int32 delta = SAMP(ofs - se);
        	v1 = (int32)src[(int)SAMP(se)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }
	else  v1 = (int32)src[offset];

	if (ofs + WAVE_ONE >= se) {
		v2 = v1;
        }
	else  v2 = (int32)src[offset+1];

	if (         d->dont_cspline ||
	     ((ofs - WAVE_ONE) < ls) ||
	     ((ofs + WAVE_TWO) > le)    ) {

                *dest++ = (sample_t)(
				v1 + (
				       (int32)(
						(v2-v1) * (ofs & FRACTION_MASK)
					      ) >> FRACTION_BITS
				     )
				    );
	}
	else {
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofsd = (int32)(ofs & FRACTION_MASK) + WAVE_ONE;
                v1 = v1 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v1 = v1 * SAMP(ofsd);
                v3 = v3 * ofsd;
                ofsd -= WAVE_ONE;
                v0 = (v3 - v0 * ofsd) / WAVE_SIX;
                v1 = (v1 - v2) * ofsd>>(FRACTION_BITS+1);
		v1 += v0;
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
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

	offset = SAMP(ofs);

	if (ofs >= se) {
		int32 delta = SAMP(ofs - se);
        	v1 = (int32)src[(int)SAMP(se)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }
	else  v1 = (int32)src[offset];

	if (ofs + WAVE_ONE >= se) {
		v2 = v1;
        }
	else  v2 = (int32)src[offset+1];

	if (         d->dont_cspline ||
	     ((ofs - WAVE_ONE) < ls) ||
	     ((ofs + WAVE_TWO) > le)    ) {

                *dest++ = (sample_t)(
				v1 + (
				       (int32)(
						(v2-v1) * (ofs & FRACTION_MASK)
					      ) >> FRACTION_BITS
				     )
				    );
	}
	else {
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofsd = (int32)(ofs & FRACTION_MASK) + WAVE_ONE;
                v1 = v1 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v1 = v1 * SAMP(ofsd);
                v3 = v3 * ofsd;
                ofsd -= WAVE_ONE;
                v0 = (v3 - v0 * ofsd) / WAVE_SIX;
                v1 = (v1 - v2) * ofsd>>(FRACTION_BITS+1);
		v1 += v0;
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
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
static uint32 vib_phase_to_inc_ptr(uint32 phase)
{
  if (phase < VIBRATO_SAMPLE_INCREMENTS/2)
    return VIBRATO_SAMPLE_INCREMENTS/2-1-phase;
  else if (phase >= 3*VIBRATO_SAMPLE_INCREMENTS/2)
    return 5*VIBRATO_SAMPLE_INCREMENTS/2-1-phase;
  else
    return phase-VIBRATO_SAMPLE_INCREMENTS/2;
}

static int32 update_vibrato(int v, Voice *vp, int sign, struct md *d)
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
  phase=vib_phase_to_inc_ptr(vp->vibrato_phase);

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
  if (vp->modulation_increment && mod_amount > 0.0) {
          update_modulation(v, d);
          if (vp->modulation_volume) {
                freq = (int32)( (double)freq*(1.0 + (mod_amount - 1.0) * (double)(vp->modulation_volume>>22) / 255.0) );
          }
  }
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
   int32   ofsd, v0, v1, v2, v3, overshoot;
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
	  incr=update_vibrato(v, vp, 0, d);
	}

	offset = SAMP(ofs);

	if (ofs >= se) {
		int32 delta = SAMP(ofs - se);
        	v1 = (int32)src[(int)SAMP(se)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }
	else  v1 = (int32)src[offset];

	if (ofs + WAVE_ONE >= se) {
		v2 = v1;
        }
	else  v2 = (int32)src[offset+1];

	if (         d->dont_cspline ||
	     ((ofs - WAVE_ONE) < ls) ||
	     ((ofs + WAVE_TWO) > le)    ) {

                *dest++ = (sample_t)(
				v1 + (
				       (int32)(
						(v2-v1) * (ofs & FRACTION_MASK)
					      ) >> FRACTION_BITS
				     )
				    );
	}
	else {
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofsd = (int32)(ofs & FRACTION_MASK) + WAVE_ONE;
                v1 = v1 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v1 = v1 * SAMP(ofsd);
                v3 = v3 * ofsd;
                ofsd -= WAVE_ONE;
                v0 = (v3 - v0 * ofsd) / WAVE_SIX;
                v1 = (v1 - v2) * ofsd>>(FRACTION_BITS+1);
		v1 += v0;
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
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
   int32   ofsd, v0, v1, v2, v3, overshoot;
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
	  incr=update_vibrato(v, vp, 0, d);
	}

	offset = SAMP(ofs);

	if (ofs >= se) {
		int32 delta = SAMP(ofs - se);
        	v1 = (int32)src[(int)SAMP(se)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }
	else  v1 = (int32)src[offset];

	if (ofs + WAVE_ONE >= se) {
		v2 = v1;
        }
	else  v2 = (int32)src[offset+1];

	if (         d->dont_cspline ||
	     ((ofs - WAVE_ONE) < ls) ||
	     ((ofs + WAVE_TWO) > le)    ) {

                *dest++ = (sample_t)(
				v1 + (
				       (int32)(
						(v2-v1) * (ofs & FRACTION_MASK)
					      ) >> FRACTION_BITS
				     )
				    );
	}
	else {
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofsd = (int32)(ofs & FRACTION_MASK) + WAVE_ONE;
                v1 = v1 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v1 = v1 * SAMP(ofsd);
                v3 = v3 * ofsd;
                ofsd -= WAVE_ONE;
                v0 = (v3 - v0 * ofsd) / WAVE_SIX;
                v1 = (v1 - v2) * ofsd>>(FRACTION_BITS+1);
		v1 += v0;
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
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
   int32   ofsd, v0, v1, v2, v3, overshoot;
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

	offset = SAMP(ofs);

	if (ofs >= se) {
		int32 delta = SAMP(ofs - se);
        	v1 = (int32)src[(int)SAMP(se)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }
	else  v1 = (int32)src[offset];

	if (ofs + WAVE_ONE >= se) {
		v2 = v1;
        }
	else  v2 = (int32)src[offset+1];

	if (         d->dont_cspline ||
	     ((ofs - WAVE_ONE) < ls) ||
	     ((ofs + WAVE_TWO) > le)    ) {

                *dest++ = (sample_t)(
				v1 + (
				       (int32)(
						(v2-v1) * (ofs & FRACTION_MASK)
					      ) >> FRACTION_BITS
				     )
				    );
	}
	else {
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofsd = (int32)(ofs & FRACTION_MASK) + WAVE_ONE;
                v1 = v1 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v1 = v1 * SAMP(ofsd);
                v3 = v3 * ofsd;
                ofsd -= WAVE_ONE;
                v0 = (v3 - v0 * ofsd) / WAVE_SIX;
                v1 = (v1 - v2) * ofsd>>(FRACTION_BITS+1);
		v1 += v0;
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
	}

	  ofs += incr;
	}
      if (vibflag)
	{
	  cc = vp->vibrato_control_ratio;
	  incr = update_vibrato(v, vp, 0, d);
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

	offset = SAMP(ofs);

	if (ofs >= se) {
		int32 delta = SAMP(ofs - se);
        	v1 = (int32)src[(int)SAMP(se)-1];
		v1 -=  (delta+1) * v1 / overshoot;
        }
	else  v1 = (int32)src[offset];

	if (ofs + WAVE_ONE >= se) {
		v2 = v1;
        }
	else  v2 = (int32)src[offset+1];

	if (         d->dont_cspline ||
	     ((ofs - WAVE_ONE) < ls) ||
	     ((ofs + WAVE_TWO) > le)    ) {

                *dest++ = (sample_t)(
				v1 + (
				       (int32)(
						(v2-v1) * (ofs & FRACTION_MASK)
					      ) >> FRACTION_BITS
				     )
				    );
	}
	else {
                v0 = (int32)src[offset-1];
                v3 = (int32)src[offset+2];
                ofsd = (int32)(ofs & FRACTION_MASK) + WAVE_ONE;
                v1 = v1 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v2 = v2 * SAMP(ofsd);
                v3 = v3 * SAMP(ofsd);
                ofsd -= WAVE_ONE;
                v0 = v0 * SAMP(ofsd);
                v1 = v1 * SAMP(ofsd);
                v3 = v3 * ofsd;
                ofsd -= WAVE_ONE;
                v0 = (v3 - v0 * ofsd) / WAVE_SIX;
                v1 = (v1 - v2) * ofsd>>(FRACTION_BITS+1);
		v1 += v0;
		*dest++ = (sample_t)((v1 > MAX_DATAVAL)? MAX_DATAVAL: ((v1 < MIN_DATAVAL)? MIN_DATAVAL: v1));
	}

	  ofs += incr;
	}
      if (vibflag)
	{
	  cc = vp->vibrato_control_ratio;
	  incr = update_vibrato(v, vp, (incr < 0), d);
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

sample_t *resample_voice_lagrange(int v, uint32 *countptr, struct md *d)
{
    Voice *vp=&d->voice[v];
    int mode;

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

