/* 
	$Id$

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

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

    tables.h
*/

#ifdef LOOKUP_SINE
extern FLOAT_T sine(int x);
#else
#include <math.h>
#ifndef PI
#  define PI M_PI
#endif
#define sine(x) (sin((2*PI/1024.0) * (x)))
#endif

#define SINE_CYCLE_LENGTH 1024
extern uint32 freq_table[];
#ifdef tplus
/*extern FLOAT_T *vol_table;*/
extern FLOAT_T def_vol_table[];
extern FLOAT_T gs_vol_table[];
extern FLOAT_T *xg_vol_table; /* == gs_vol_table */
#else
extern FLOAT_T vol_table[];
#endif
extern FLOAT_T expr_table[];
extern FLOAT_T bend_fine[];
extern FLOAT_T bend_coarse[];
#ifdef tplus
extern FLOAT_T midi_time_table[], midi_time_table2[];
#endif
extern FLOAT_T butterworth[101][5];
extern uint8 *_l2u; /* 13-bit PCM to 8-bit u-law */
extern uint8 _l2u_[]; /* used in LOOKUP_HACK */
#ifdef LOOKUP_HACK
extern int16 _u2l[];
extern int32 *mixup;
#ifdef LOOKUP_INTERPOLATION
extern int8 *iplookup;
#endif
#endif

extern void init_tables(void);

extern struct short_voice_type {
	const char *vname;
#define SOLO_MASK 1
	char flags;
} gm_voice[];

