/*

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
    resample.h
*/

#define DO_LINEAR_INTERPOLATION 0
#define DO_CSPLINE_INTERPOLATION 1
#define DO_LAGRANGE_INTERPOLATION 2
#define DO_FILTER_INTERPOLATION 3

extern sample_t *resample_voice(int v, uint32 *countptr, struct md *d);
extern sample_t *resample_voice_lagrange(int v, uint32 *countptr, struct md *d);
extern sample_t *resample_voice_filter(int v, uint32 *countptr, struct md *d);
extern void pre_resample(Sample *sp);
extern void do_lowpass(Sample *sample, uint32 srate, sample_t *buf, uint32 count, uint32 freq, FLOAT_T resonance);
extern int recompute_modulation(int v, struct md *d);
/*extern sample_t resample_buffer[AUDIO_BUFFER_SIZE+100];*/
/*extern uint32 resample_buffer_offset;*/
extern int update_modulation(int v, struct md *d);
extern int32 update_modulation_signal(int v, int32 incr, struct md *d);
/*extern int32 calc_mod_freq(int v, int32 incr, struct md *d);*/
