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

   playmidi.h

   */

#define POLYPHONY_COUNT

#ifdef POLYPHONY_COUNT
typedef struct {
  uint32 time;
  uint8 channel, type, a, b, polyphony;
} MidiEvent;
#else
typedef struct {
  uint32 time;
  uint8 channel, type, a, b;
} MidiEvent;
#endif

/* Midi events */
#define ME_NONE 	0
#define ME_NOTEON	1
#define ME_NOTEOFF	2
#define ME_KEYPRESSURE	3
#define ME_MAINVOLUME	4
#define ME_PAN		5
#define ME_SUSTAIN	6
#define ME_EXPRESSION	7
#define ME_PITCHWHEEL	8
#define ME_PROGRAM	9
#define ME_TEMPO	10
#define ME_PITCH_SENS	11

#define ME_ALL_SOUNDS_OFF	12
#define ME_RESET_CONTROLLERS	13
#define ME_ALL_NOTES_OFF	14

#define ME_TONE_BANK	15
#define ME_LYRIC	16
#define ME_TONE_KIT	17
#define ME_MASTERVOLUME	18
#define ME_CHANNEL_PRESSURE 19

#define ME_HARMONICCONTENT	71
#define ME_RELEASETIME		72
#define ME_ATTACKTIME		73
#define ME_BRIGHTNESS		74

#define ME_REVERBERATION	91
#define ME_CHORUSDEPTH		93
#ifdef CHANNEL_EFFECT
#define ME_CELESTE		94
#define ME_PHASER		95
#endif
#define ME_EOT		99

#ifdef tplus
#define ME_PORTAMENTO_TIME_MSB	100
#define ME_PORTAMENTO_TIME_LSB	101
#define ME_PORTAMENTO		102
#define ME_MODULATION_WHEEL	103

#define ME_VIBRATO_RATE		104
#define ME_VIBRATO_DEPTH	105
#define ME_VIBRATO_DELAY	106
#define ME_FINE_TUNING		107
#define ME_COARSE_TUNING	108
#endif

#define SFX_BANKTYPE	64

typedef struct {
  int
    bank, kit, sfx, program, volume, sustain, panning, pitchbend, expression, 
    variationbank, mono, /* one note only on this channel -- not implemented yet */
#ifdef tplus
    portamento,
#endif
    reverberation, chorusdepth, harmoniccontent, releasetime, attacktime, brightness,
    pitchsens;
#ifdef tplus
  uint32 modulation_wheel;
#endif
  /* chorus, reverb... Coming soon to a 300-MHz, eight-way superscalar
     processor near you */
  FLOAT_T
    pitchfactor; /* precomputed pitch bend factor to save some fdiv's */
#ifdef tplus
  /* For portamento */
  uint8 portamento_time_msb, portamento_time_lsb;
  uint8 tuning_msb, tuning_lsb;
  int porta_control_ratio, porta_dpb;
  int32 last_note_fine;

  /* For vibrato */
  int32 vibrato_ratio, vibrato_delay;
  int vibrato_depth;
#endif

  char
    transpose;
  const char *name;
} Channel;

/* Causes the instrument's default panning to be used. */
#define NO_PANNING -1

#define MAXPOINT 7

typedef struct {
  uint8
    status, channel, note, velocity, clone_type;
  Sample *sample;
  Sample *left_sample;
  Sample *right_sample;
  int32 clone_voice;
  uint32
    orig_frequency, frequency,
    sample_offset, loop_start, loop_end;
  int32
    envelope_volume, modulation_volume;
  uint32
    envelope_target, modulation_target;
  uint32
    tremolo_sweep, tremolo_sweep_position, tremolo_phase,
    lfo_sweep, lfo_sweep_position, lfo_phase,
    vibrato_sweep, vibrato_sweep_position, vibrato_depth,
    starttime, echo_delay_count;
  int32
    echo_delay,
    sample_increment,
    envelope_increment,
    modulation_increment,
    tremolo_phase_increment,
    lfo_phase_increment;
  
  final_volume_t left_mix, right_mix;

  FLOAT_T
    volume, left_amp, right_amp, tremolo_volume, lfo_volume,
    modEnvToFilterFc, modLfoToFilterFc,
    current_x0, current_x1, current_y0, current_y1;
  int32
    vibrato_sample_increment[VIBRATO_SAMPLE_INCREMENTS];
  uint32
    envelope_rate[MAXPOINT], envelope_offset[MAXPOINT];
  uint32
    vibrato_phase, vibrato_control_ratio, vibrato_control_counter,
#ifdef tplus
    vibrato_delay, orig_vibrato_control_ratio, modulation_wheel,
#endif
    envelope_stage, modulation_stage, control_counter,
    modulation_delay, modulation_counter, bw_index, panning, panned;

#ifdef tplus
  /* for portamento */
  int porta_control_ratio, porta_control_counter, porta_dpb;
  int32 porta_pb;
#endif

} Voice;

/* Voice status options: */
#define VOICE_FREE	(1<<0)
#define VOICE_ON	(1<<1)
#define VOICE_SUSTAINED	(1<<2)
#define VOICE_OFF	(1<<3)
#define VOICE_DIE	(1<<4)

/* Voice panned options: */
#define PANNED_MYSTERY 0
#define PANNED_LEFT 1
#define PANNED_RIGHT 2
#define PANNED_CENTER 3
/* Anything but PANNED_MYSTERY only uses the left volume */


/* Envelope stages: */
#define ATTACK 0
#define HOLD 1
#define DECAY 2
#define RELEASE 3
#define RELEASEB 4
#define RELEASEC 5
#define DELAY 6

/* Voice effects options: */
#define OPT_STEREO_VOICE 1
#define OPT_REVERB_VOICE 2
#define OPT_CHORUS_VOICE 4
#define OPT_STEREO_EXTRA 16
#define OPT_REVERB_EXTRA 32
#define OPT_CHORUS_EXTRA 64

#define MAXCHAN 64
#define MAXNOTE 128

#ifndef ADAGIO
extern Channel channel[MAXCHAN];
extern signed char drumvolume[MAXCHAN][MAXNOTE];
extern signed char drumpanpot[MAXCHAN][MAXNOTE];
extern signed char drumreverberation[MAXCHAN][MAXNOTE];
extern signed char drumchorusdepth[MAXCHAN][MAXNOTE];
#else /* ADAGIO */
extern Channel channel[MAX_TONE_VOICES];
#endif /* ADAGIO */
extern Voice voice[MAX_VOICES];

extern int32 control_ratio, amp_with_poly, amplification;
extern int32 drumchannels;
extern int adjust_panning_immediately;
extern int voices;

#ifdef tplus
extern int note_key_offset;
extern FLOAT_T midi_time_ratio;
extern int opt_modulation_wheel;
extern int opt_portamento;
extern int opt_nrpn_vibrato;
extern int opt_reverb_control;
extern int opt_chorus_control;
extern int opt_channel_pressure;
extern int opt_overlap_voice_allow;
extern void recompute_freq(int v);
extern int dont_cspline;
#endif
extern int opt_dry;
extern int opt_expression_curve;
extern int opt_volume_curve;
extern int opt_stereo_surround;
extern int dont_filter_melodic;
extern int dont_filter_drums;
extern int command_cutoff_allowed;
extern int dont_chorus;
extern int dont_reverb;
extern int current_interpolation;
extern int dont_keep_looping;

extern int GM_System_On;
extern int XG_System_On;
extern int GS_System_On;

extern int XG_System_reverb_type;
extern int XG_System_chorus_type;
extern int XG_System_variation_type;

#define ISDRUMCHANNEL(c) ((drumchannels & (1<<(c%16))))

extern int play_midi(MidiEvent *el, uint32 events, uint32 samples);
extern int play_midi_file(const char *fn);
extern int play_some_midi_file();
extern void dumb_pass_playing_list(int number_of_files, const char *list_of_files[]);
extern int read_config_file(const char *name, int prescanning);
extern char *current_config_file;
extern int reverb_options;
extern int global_reverb;
extern int global_chorus;
extern int global_echo;
extern int global_detune;
extern int cfg_select;
extern void clear_config(void);
extern void effect_activate( int iSwitch );
extern int have_commandline_midis;
extern int skip_to(uint32 until_time);
