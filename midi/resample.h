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

    resample.h
*/

extern sample_t *resample_voice(int v, uint32 *countptr);
extern sample_t *resample_voice_lagrange(int v, uint32 *countptr);
extern sample_t *resample_voice_filter(int v, uint32 *countptr);
extern void pre_resample(Sample *sp);
extern void do_lowpass(Sample *sample, uint32 srate, sample_t *buf, uint32 count, uint32 freq, FLOAT_T resonance);
extern int recompute_modulation(int v);
extern sample_t resample_buffer[AUDIO_BUFFER_SIZE+100];
extern uint32 resample_buffer_offset;
extern int update_modulation(int v);
extern int update_modulation_signal(int v);
extern int32 calc_mod_freq(int v, int32 incr);
