
#ifdef FFF_HAS_BEEN_FIXED
/*
 * fffload - Read .fff patch-description files and load the patches
 *   they refer to.  The routine load_fff_patch is adapted from "load_instrument",
 *   which is Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>.
 *		-- Greg Lee, lee@Hawaii.edu, June, 1997.
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
 *
 * $Id$
 */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>

#ifdef __FreeBSD__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "resample.h"
#include "tables.h"
#include "filter.h"

extern int32 convert_envelope_rate(uint8);
extern int32 convert_envelope_rate_attack(uint8, uint8);
extern int32 convert_envelope_offset(uint8);
extern int32 convert_tremolo_sweep(uint8);
extern int32 convert_vibrato_sweep(uint8, int32);
extern int32 convert_tremolo_rate(uint8);
extern int32 convert_vibrato_rate(uint8);

#define FFFF 1
#define CPRT 2
#define DATA 4
#define ENVP 8
#define PROG 16
#define PTCH 32
#define LAYR 64
#define WAVE 128
#define ANY_HUNK 255


#define SIZEOF_PROG_HEADER 16
#define SIZEOF_DATA_HEADER 12

static int current_toneset = 0;
static int old_toneset = -1;
static int fsize;
static unsigned char *fff;

struct hunk_type {
	int tag;
	unsigned char *ptr;
};

struct envp_point {
    unsigned short offset, rate;
};
struct envp_rec {
    unsigned short nattack, nrelease, sustain_offset,
		sustain_rate, release_rate;
    unsigned char hirange, pad;
    struct envp_point *envp_points_attack;
    struct envp_point *envp_points_release;
};
struct envp_type {
    unsigned char num_envelopes, flags, mode, index_type;
    struct envp_rec *envp_records;
    struct envp_type *next;
};

struct wave_type {
    unsigned long size, start, loopstart, loopend, mstart, sample_ratio;
    unsigned char attenuation, low_note, high_note, format, m_format;
    struct wave_type *next_wave;
};

struct TLFO {
    unsigned short freq;
    short depth, sweep;
    unsigned char shape, delay;
};
struct layr_type {
    unsigned long id;
    unsigned char nwaves, flags, high_range, low_range, pan, pan_freq_scale;
    struct TLFO tremolo, vibrato;
    unsigned char velocity_mode, attenuation;
    unsigned short freq_scale;
    unsigned char freq_center, layer_event;
    unsigned long penv, venv;
    struct layr_type *next;
    struct wave_type *waves;
};

static struct hunk_type *hunkit(unsigned char *here, int req);

#define SHGT(a) ( (here[a+1]<<8)|here[a] )
#define LGGT(a) ( (here[a+3]<<24)|(here[a+2]<<16)|(here[a+1]<<8)|here[a] )

static struct hunk_type *global_wave_hunk;

static struct wave_type *make_wave(unsigned char *here, struct hunk_type *me) {
    struct wave_type *wave;
    struct hunk_type *wave_hunk;

    wave = (struct wave_type *)malloc( sizeof(struct wave_type) );
    wave->size = LGGT(4);
    wave->start = LGGT(8);
    wave->loopstart = LGGT(12);
    wave->loopend = LGGT(16);
    wave->mstart = LGGT(20);
    wave->sample_ratio = LGGT(24);
    wave->attenuation = here[28];
    wave->low_note = here[29];
    wave->high_note = here[30];
    wave->format = here[31];
    wave->m_format = here[32];

    if (!me->ptr) {
	wave->next_wave = 0;
	global_wave_hunk = me;
	return(wave);
    }
    global_wave_hunk = wave_hunk = hunkit(me->ptr, ANY_HUNK);
    if (wave_hunk->tag != WAVE) wave->next_wave = 0;
    else wave->next_wave = make_wave(me->ptr + 8, wave_hunk);
    return(wave);
}

static struct layr_type *make_layr(unsigned char *here, struct hunk_type *me) {
    struct layr_type *layr;
    struct hunk_type *wave_hunk;

    layr = (struct layr_type *)malloc( sizeof(struct layr_type) );
    layr->nwaves = here[4];
    layr->flags = here[5];
    layr->high_range = here[6];
    layr->low_range = here[7];
    layr->pan = here[8];
    layr->pan_freq_scale = here[9];
    layr->tremolo.freq = (unsigned short)SHGT(10);
    layr->tremolo.depth = (short)SHGT(12);
    layr->tremolo.sweep = (short)SHGT(14);
    layr->tremolo.shape = here[16];
    layr->tremolo.delay = here[17];
    layr->vibrato.freq = (unsigned short)SHGT(18);
    layr->vibrato.depth = (short)SHGT(20);
    layr->vibrato.sweep = (short)SHGT(22);
    layr->vibrato.shape = here[24];
    layr->vibrato.delay = here[25];
    layr->velocity_mode = here[26];
    layr->attenuation = here[27];
    layr->freq_scale = (unsigned short)SHGT(28);
    layr->freq_center = here[30];
    layr->layer_event = here[31];
    wave_hunk = hunkit(me->ptr, WAVE);
    layr->waves = make_wave(me->ptr + 8, wave_hunk);
    return(layr);
}

static struct envp_type *make_envp(unsigned char *here) {
    struct envp_type *envp;
    int i, j, off;
    envp = (struct envp_type *)malloc( sizeof(struct envp_type) );
    envp->num_envelopes = here[4];
    envp->flags = here[5];
    envp->mode = here[6];
    envp->index_type = here[7];
    envp->envp_records = (struct envp_rec *)malloc(
		sizeof(struct envp_rec) * envp->num_envelopes );
    off = 8;

    for (i = 0; i < envp->num_envelopes; i++) {
	int na, nr;
	na = envp->envp_records[i].nattack = SHGT(off); off += 2;
	nr = envp->envp_records[i].nrelease = SHGT(off); off += 2;
	envp->envp_records[i].sustain_offset = SHGT(off); off += 2;
	envp->envp_records[i].sustain_rate = SHGT(off); off += 2;
	envp->envp_records[i].release_rate = SHGT(off); off += 2;
	envp->envp_records[i].hirange = here[off]; off++;
	envp->envp_records[i].pad = here[off]; off++;
	envp->envp_records[i].envp_points_attack = (struct envp_point *)
		malloc( sizeof(struct envp_point) * na );
	for (j = 0; j < na; j++) {
	    envp->envp_records[i].envp_points_attack[j].offset = SHGT(off);
	    off += 2;
	    envp->envp_records[i].envp_points_attack[j].rate = SHGT(off);
	    off += 2;
	}
	envp->envp_records[i].envp_points_release = (struct envp_point *)
		malloc( sizeof(struct envp_point) * nr );
	for (j = 0; j < nr; j++) {
	    envp->envp_records[i].envp_points_release[j].offset = SHGT(off);
	    off += 2;
	    envp->envp_records[i].envp_points_release[j].rate = SHGT(off);
	    off += 2;
	}
    }

    return(envp);
}

#define MAX_FFF_FILES 40
static char fff_filename[MAX_FFF_FILES][200];
static char dat_filename[MAX_FFF_FILES][200];
static int dat_opened[MAX_FFF_FILES] = { 0 };
static int f_ix = 0;

static char ptch_data_name[80];

static struct ptch_type {
    /*unsigned long id;*/
    struct envp_type *envelope;
    unsigned short nlayers;
    unsigned char layer_mode, exclusion_mode;
    unsigned short exclusion_group;
    unsigned char effect1, effect1_depth, effect2, effect2_depth;
    unsigned char bank, program, file_index;
    unsigned long iw_layer;
    struct layr_type *layr;
    struct ptch_type *next_bank;
} *ptch[256] = { 0 };

static int layr_count = 0;

static struct ptch_type *make_ptch(unsigned char *here, struct hunk_type *patch,
		struct envp_type *envelope) {
    struct ptch_type *p;
    struct hunk_type *layr_hunk;

    p = (struct ptch_type*)malloc( sizeof(struct ptch_type) );
    p->envelope = envelope;
    layr_count = p->nlayers = (unsigned short)SHGT(4);
    p->layer_mode = here[6];
    p->exclusion_mode = here[7];
    p->exclusion_group = (unsigned short)SHGT(8);
    p->effect1 = here[10];
    p->effect1_depth = here[11];
    p->effect2 = here[12];
    p->effect2_depth = here[13];
    if ((int)here[14] == old_toneset) p->bank = (unsigned char)current_toneset;
    else p->bank = here[14];
    p->program = here[15];
    p->file_index = f_ix;
    layr_hunk = hunkit(patch->ptr, LAYR);
    p->layr = make_layr(patch->ptr + 8, layr_hunk);
    p->next_bank = ptch[p->program];
    ptch[p->program] = p;
    return(p);
}


static struct hunk_type *hunkit(unsigned char *here, int req) {
    int tag = -1;
    struct hunk_type *h;
    unsigned long offset;
    if (!strncmp(here, "FFFF", 4)) tag = FFFF;
    else if (!strncmp(here, "CPRT", 4)) tag = CPRT;
    else if (!strncmp(here, "DATA", 4)) tag = DATA;
    else if (!strncmp(here, "ENVP", 4)) tag = ENVP;
    else if (!strncmp(here, "PROG", 4)) tag = PROG;
    else if (!strncmp(here, "PTCH", 4)) tag = PTCH;
    else if (!strncmp(here, "LAYR", 4)) tag = LAYR;
    else if (!strncmp(here, "WAVE", 4)) tag = WAVE;
    else {
        int i;
        fprintf(stderr,"unknown hunk type ");
        for (i = 0; i < 4; i++) {
    	if (here[i] >= 'A' && here[i] <= 'Z') putchar(here[i]);
    	else fprintf(stderr,"[%d]", here[i]);
        }
        fprintf(stderr,"\n");
	    #ifdef ADAGIO
	    X_EXIT
	    #else
	    exit(1);
	    #endif
    }
    if (!(req & tag)) {
        fprintf(stderr,"requested %d hunk but got %d hunk\n", req, tag);
	    #ifdef ADAGIO
	    X_EXIT
	    #else
	    exit(1);
	    #endif
    }
    h = (struct hunk_type *)malloc( sizeof(struct hunk_type) );
    if (!h) {
        fprintf(stderr,"out of mem!\n");
	    #ifdef ADAGIO
	    X_EXIT
	    #else
	    exit(1);
	    #endif
    }
    h->tag = tag;
    offset = (unsigned long)LGGT(4);
    if (here - fff + offset + 8 >= (unsigned long)fsize) h->ptr = 0;
    else h->ptr = here + offset + 8;
    return h;
}

/* Following depends on ENVP hunk preceding corresponding PROG hunk.
 */
static int make_hunks(unsigned char *start) {
    int cnt = 0;
    unsigned char *next, *prev;
    struct hunk_type *patch, *hunk;
    struct envp_type *envelope, *next_envelope, *last_envelope=0;
    struct ptch_type *p;
    struct layr_type *dangle, *last_layr=0;

    envelope = 0;
    next = start;

    while (next) {
        hunk = hunkit(next, DATA|ENVP|PROG|LAYR|WAVE);
        prev = next;
        next = hunk->ptr;
        if (hunk->tag == PROG) {
	    patch = hunkit(prev + SIZEOF_PROG_HEADER, PTCH);
	    if (!envelope) {
		fprintf(stderr,"strange hunk order!\n");
	    #ifdef ADAGIO
	    X_EXIT
	    #else
	    exit(1);
	    #endif
	    }
	    p = make_ptch(prev + SIZEOF_PROG_HEADER + 8, patch, envelope);
	    last_layr = p->layr;
	    last_layr->next = 0;
	    envelope = 0;
        }
        else if (hunk->tag == DATA)
    	    strcpy(ptch_data_name, prev + SIZEOF_DATA_HEADER);
        else if (hunk->tag == ENVP) {
		last_layr = 0;
		next_envelope = make_envp(prev + 8);
		next_envelope->next = 0;
		if (!envelope) {
			envelope = next_envelope;
			last_envelope = envelope;
		}
		else {
			last_envelope->next = next_envelope;
			last_envelope = next_envelope;
		}
	}
        else if (hunk->tag == LAYR) {
    		dangle = make_layr(prev + 8, hunk);
		dangle->next = 0;
		if (last_layr && layr_count > 1) {
			last_layr->next = dangle;
			last_layr = dangle;
			layr_count--;
		}
		else fprintf(stderr,"\nFound dangling LAYR in .fff file.\n\n");
		next = global_wave_hunk->ptr;
	}
        else if (hunk->tag == WAVE) {
		fprintf(stderr,"Found stray WAVE in .fff file.\n");
	}
        cnt++;
    }
    return(cnt);
}



int loadfff(char * filename, int bank, int oldbank) {
    int cnt;
    struct stat info;
    struct hunk_type *ffff_hunk, *cprt_hunk;
    static int f = 0;
    char tmpnm[200];

#ifdef ADAGIO
    if (tracing) printf("loadfff(%s,bank=%d)\n", filename, bank);
#else
  ctl->cmsg(CMSG_INFO, VERB_NOISY, "Loading fff file %s", filename);
#endif

    if (!filename) return(1);
    if (filename[0] == '/' || (filename[0] == '.' && filename[1] == '/'))
	 strcpy(tmpnm, filename);
    else {
	strcpy(tmpnm, TIMID_DIR);
	strcat(tmpnm, "/");
	strcat(tmpnm, filename);
    }
    if (strcmp(".fff", tmpnm + strlen(tmpnm) - 4))
    	strcat(tmpnm,".fff");

    for (cnt = f_ix - 1; cnt >= 0; cnt--) if (!strcmp(tmpnm, fff_filename[cnt]))
	return(0);

    if (stat(tmpnm, &info)) {
    	perror(tmpnm);
	f = -1;
    	return (1);
    }
    f = open(tmpnm, O_RDONLY, 0);
    if (f < 0) {
    	perror(tmpnm);
    	return(1);
    }
    fff = (unsigned char *)malloc(info.st_size);
    if (!fff) {
    	perror("malloc");
	f = -1;
    	return(1);
    }
    fsize = read(f, fff, info.st_size);
    if (fsize < info.st_size) {
    	perror(tmpnm);
        close(f);
    	free(fff);
    	return(1);
    }
    close(f);

    ffff_hunk = hunkit(fff, FFFF);
    if (!ffff_hunk) {
    	fprintf(stderr,"Couldn't even get FFFF hunk.\n");
	f = -1;
    	return(1);
    }
    if (ffff_hunk->tag != FFFF) {
    	fprintf(stderr,"First hunk is wrong type (%d).\n", ffff_hunk->tag);
	f = -1;
    	return(1);
    }
    if (ffff_hunk->ptr) {
    	fprintf(stderr,"Looks like multiple FFFF hunks here ...\n");
    	fprintf(stderr,"  I'll just do the first one.\n");
    }

    current_toneset = bank & 0x7f;
    old_toneset = oldbank;

    cprt_hunk = hunkit(fff + 8, CPRT);
    cnt = make_hunks(cprt_hunk->ptr);

    strcpy(fff_filename[f_ix], tmpnm);

    { int i = strlen(tmpnm);
      while ( i && tmpnm[--i] != '/' ) ;
      tmpnm[++i] = '\0';
      strcat(tmpnm, ptch_data_name);
    }

    strcpy(dat_filename[f_ix], tmpnm);

    f_ix++;
    free(fff);
    return(0);
}

#ifdef ADAGIO
int fff_test(int prog, int bank)
{
    struct ptch_type *p;
    if (!(p=ptch[prog])) return(0);
    bank &= 0x7f;
    while (p->bank != bank && p->next_bank) p = p->next_bank;
    return (p->bank == bank);
}
#endif

#define LEFT_SIDE 0
#define RIGHT_SIDE 1
#ifndef USEENV_FLAG
#define USEENV_FLAG 0x16
#endif

#ifdef ADAGIO
InstrumentLayer *load_fff_patch(int prog, int tpgm, int reverb, int main_volume) {
    extern int next_wave_prog;
    int amp=-1, note_to_use, strip_loop, strip_envelope, strip_tail, bank, newmode;
    int percussion;
    char *name;
#else
InstrumentLayer *load_fff_patch(char *name, int prog, int bank, int percussion,
			   int panning, int amp, int note_to_use,
			   int strip_loop, int strip_envelope,
			   int strip_tail) {
#endif
    InstrumentLayer *lp;
    Instrument *ip;
    Sample *sp;
    int i;
    uint32 cnt;
    unsigned char mode;
    struct ptch_type *p;
    struct envp_type *e;
    struct layr_type *l;
    struct wave_type *w;
    int f, d_ix;
    int current_side = LEFT_SIDE;

#ifdef ADAGIO
    if (!(gus_voice[tpgm].keep & FFF_FLAG)) return(0);
    if (tracing) printf("load_fff_patch(prog=%d,tpgm=%d,reverb=%d,main_volume=%d) keep 0x%02X\n",
	prog,tpgm,reverb,main_volume, gus_voice[tpgm].keep);
#endif

    if (!(p=ptch[prog])) return(0);

#ifdef ADAGIO
    name = gus_voice[tpgm].vname;
    percussion = (prog >= 128);
    bank = gus_voice[tpgm].bank & 0x7f;
    newmode = gus_voice[tpgm].modes;
#endif

    while (p->bank != bank && p->next_bank) p = p->next_bank;
    if (p->bank != bank) return(0);

#ifdef ADAGIO
    if (prog < 128) note_to_use = prog;
    else note_to_use = prog - 128;
#endif

    d_ix = p->file_index;
    f = dat_opened[d_ix];
    if (f < 0) return(0);
    if (!f) {
      f = open(dat_filename[d_ix], O_RDONLY, 0);
      dat_opened[d_ix] = f;
      if (f < 0) {
    	    perror(dat_filename[d_ix]);
    	    return(0);
      }
    }

    ctl->cmsg(CMSG_INFO, VERB_NOISY, "Loading%s %s[%d,%d] from %s",
	(p->nlayers == 2)? " stereo" : "",
	 name, (percussion)? prog-128 : prog, bank,
	 dat_filename[d_ix]);


#ifdef FFFDEBUG
printf("patch for prog %d, note %d:\n", prog, note);
#endif
    e = p->envelope;
    l = p->layr;
    if (!l->nwaves) return(0);

    ip = (Instrument *)safe_malloc(sizeof(Instrument));
  
  lp=(InstrumentLayer *)safe_malloc(sizeof(InstrumentLayer));
  lp->lo = 0;
  lp->hi = 127;
  lp->instrument = ip;
  lp->next = 0;

#ifdef ADAGIO
    gus_voice[tpgm].loaded |= DSP_MASK;
    gus_voice[tpgm].prog = next_wave_prog++;
#endif

/* back here to load right part of a stereo patch */
other_side:

    ip->type = INST_GUS;
    ip->samples = l->nwaves;
    ip->sample = (Sample *)malloc(sizeof(Sample) * ip->samples);

    if (current_side == LEFT_SIDE) {
	ip->left_samples = ip->samples;
	ip->left_sample = ip->sample;
	ip->right_samples = 0;
	ip->right_sample = 0;
    } else {
	ip->right_samples = ip->samples;
	ip->right_sample = ip->sample;
    }

    w = l->waves;

  for (i = 0; i < ip->samples; i++) {
    int v;

    for (v = 0; v < ip->samples && v < e->num_envelopes ; v++)
	if (e->envp_records[v].hirange >= w->high_note) break;

    sp=&(ip->sample[i]);

    sp->data_length = w->size;
    sp->loop_start = w->loopstart >> 4;
    sp->loop_end = w->loopend >> 4;
#ifdef FFFDEBUG
printf("data_length %ld, loop_start %ld, loop_end %ld\n",
sp->data_length, sp->loop_start, sp->loop_end);
#endif

    sp->sample_rate = 44100;

    sp->freq_center = l->freq_center;
    sp->freq_scale = l->freq_scale;

#ifdef ADAGIO
    if (prog > 127 && gus_voice[tpgm].fix_key) {
	sp->freq_scale = 0;
	note_to_use = sp->freq_center = gus_voice[tpgm].fix_key;
    }
    else if (!sp->freq_scale) gus_voice[tpgm].fix_key = sp->freq_center;
#endif

    if (prog > 127 && l->freq_scale == 1024) {
    /* for the SoundCanvas patch set, we should play the prog # */
	sp->freq_scale = 0;
	sp->freq_center = note_to_use;
    }

#ifdef ADAGIO
    if (gus_voice[tpgm].trnsps && current_side == LEFT_SIDE)
	sp->freq_center += gus_voice[tpgm].trnsps - 64;
    if (gus_voice[tpgm].right_trnsps && current_side == RIGHT_SIDE)
	sp->freq_center += gus_voice[tpgm].right_trnsps - 64;
#endif

    if (sp->freq_scale) {
	note_to_use = -1;
	sp->low_freq = freq_table[w->low_note];
	sp->high_freq = freq_table[w->high_note];
    } else {
/* for the time being, the note_to_play logic for percussion is disabled,
 * and freq for note will be determined by freq_center */
	note_to_use = sp->freq_center;
	sp->low_freq = sp->high_freq = freq_table[note_to_use];
    }

    sp->root_freq = w->sample_ratio;

#ifdef FFFDEBUG
printf("sample_rate %ld, low_freq %ld, high_freq %ld, root_freq %ld\n",
sp->sample_rate, sp->low_freq, sp->high_freq, sp->root_freq);
#endif

    sp->attenuation = 0x7f & (l->attenuation + w->attenuation);

    sp->panning = l->pan;

      for (v=ATTACK; v<DELAY; v++)
	{
	  sp->modulation_rate[v]=0;
	  sp->modulation_offset[v]=0;
	}
      sp->modulation_rate[DELAY] = sp->modulation_offset[DELAY] = 0;
      sp->modEnvToFilterFc=0;
      sp->modEnvToPitch=0;
      sp->resonance=0;
      sp->cutoff_freq=0;
      sp->reverberation=0;
      sp->chorusdepth=0;
      sp->exclusiveClass=0;
      sp->keyToModEnvHold=0;
      sp->keyToModEnvDecay=0;
      sp->keyToVolEnvHold=0;
      sp->keyToVolEnvDecay=0;
	sp->lfo_sweep_increment = 0;
	sp->lfo_phase_increment = 0;
	sp->modLfoToFilterFc = 0;
	sp->vibrato_delay = 0;

      if (!l->tremolo.freq || !l->tremolo.depth)
	{
	  sp->tremolo_sweep_increment=
	    sp->tremolo_phase_increment=sp->tremolo_depth=0;
	}
      else
	{
	  sp->tremolo_sweep_increment=convert_tremolo_sweep(l->tremolo.sweep);
	  sp->tremolo_phase_increment=convert_tremolo_rate(l->tremolo.freq/4);
	  sp->tremolo_depth=l->tremolo.depth;
	}

      if (!l->vibrato.freq || !l->vibrato.depth)
	{
	  sp->vibrato_sweep_increment=
	    sp->vibrato_control_ratio=sp->vibrato_depth=0;
	}
      else
	{
	  sp->vibrato_control_ratio=convert_vibrato_rate(l->vibrato.freq/4);
	  sp->vibrato_sweep_increment=
	    convert_vibrato_sweep(l->vibrato.sweep,
	        (int32)sp->vibrato_control_ratio);
	  sp->vibrato_depth=l->vibrato.depth;

	}

    mode = MODES_ENVELOPE;
    if (e->mode == 2) mode |= MODES_SUSTAIN;
    else if (e->mode == 1) mode |= MODES_FAST_RELEASE;
    if (!(w->format & 1)) mode |= MODES_16BIT;
    if (!(w->format & 2)) mode |= MODES_UNSIGNED;
/* bit pos. for REVERSE and PINGPONG are inverted ? */
    if (!(w->format & 4)) mode |= MODES_REVERSE;
    if (w->format & 8) mode |= MODES_LOOPING;

    if (w->format & 16) mode |= MODES_PINGPONG;
    if (w->format & 32) {
	fprintf(stderr, "can't handle uLaw wave data\n");
  #ifdef ADAGIO
	  gus_voice[tpgm].loaded = 0;
  #endif
	return(0);
    }

    sp->modes = mode;

/**
I think it's better to adjust the envelopes, somehow.  Now, loops and
envelopes are only stripped by specific request in config file.
    if (note_to_use >= 0) {
	strip_loop = 1;
	strip_envelope = 1;
    }
**/

#define STRIP_PERCUSSION
#ifdef ADAGIO
#ifndef STRIP_PERCUSSION
      strip_loop = strip_envelope = strip_tail = 0;
#endif
      if (!i && current_side == LEFT_SIDE) {
        gus_voice[tpgm].vibrato_sweep = l->vibrato.sweep;
        gus_voice[tpgm].vibrato_rate = l->vibrato.freq;
        gus_voice[tpgm].vibrato_depth = l->vibrato.depth;
        if (!newmode) gus_voice[tpgm].modes = sp->modes;
      }
      if (newmode) sp->modes = newmode;

      amp = gus_voice[tpgm].volume;

      if (gus_voice[tpgm].strip & LOOPS_FLAG) strip_loop = 1;
      if (gus_voice[tpgm].strip & ENVELOPE_FLAG) strip_envelope = 1;
      if (gus_voice[tpgm].strip & TAIL_FLAG) strip_tail = 1;
      if (gus_voice[tpgm].keep & LOOPS_FLAG) strip_loop = 0;
      if (gus_voice[tpgm].keep & ENVELOPE_FLAG) strip_envelope = 0;
      if (gus_voice[tpgm].keep & TAIL_FLAG) strip_tail = 0;
#endif /* ADAGIO */

/* next is taken literally from timidity routine */

      /* Mark this as a fixed-pitch instrument if such a deed is desired. */
      if (note_to_use!=-1)
	sp->note_to_use=(uint8)(note_to_use);
      else
	sp->note_to_use=0;
      
      /* seashore.pat in the Midia patch set has no Sustain. I don't
         understand why, and fixing it by adding the Sustain flag to
         all looped patches probably breaks something else. We do it
         anyway. */
#if 0 
      if (sp->modes & MODES_LOOPING) 
	sp->modes |= MODES_SUSTAIN;
#endif

/** debug -- How is strip_loop set to 1 here??? **/
/**if (!percussion) strip_loop = 0;*/
/* if (sp->loop_start < 0) strip_loop = 1; */
if (sp->loop_end > sp->data_length) strip_loop = 1;
if (sp->loop_start >=  sp->loop_end) strip_loop = 1;
if (!percussion && strip_loop == 1) {
	fprintf(stderr, "loop start %lu, loop end %lu, datalength %lu\n",
		sp->loop_start, sp->loop_end, sp->data_length);
}

      /* Strip any loops and envelopes we're permitted to */
      if ((strip_loop==1) && 
	  (sp->modes & (MODES_SUSTAIN | MODES_LOOPING | 
			MODES_PINGPONG | MODES_REVERSE)))
	{
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Removing loop and/or sustain");
	  sp->modes &=~(MODES_SUSTAIN | MODES_LOOPING | 
			MODES_PINGPONG | MODES_REVERSE);
	}

      if (strip_envelope==1)
	{
	  if (sp->modes & MODES_ENVELOPE)
	    ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Removing envelope");
	  sp->modes &= ~MODES_ENVELOPE;
	}
      else if (strip_envelope != 0)
	{
	  /* Have to make a guess. */
	  if (!(sp->modes & (MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE)))
	    {
	      /* No loop? Then what's there to sustain? No envelope needed
		 either... */
	      sp->modes &= ~(MODES_SUSTAIN|MODES_ENVELOPE);
	      ctl->cmsg(CMSG_INFO, VERB_DEBUG, 
			" - No loop, removing sustain and envelope");
	    }
#ifndef ADAGIO
	  else if (!(sp->modes & MODES_SUSTAIN))
	    {
	      /* No sustain? Then no envelope.  I don't know if this is
		 justified, but patches without sustain usually don't need the
		 envelope either... at least the Gravis ones. They're mostly
		 drums.  I think. */
	      sp->modes &= ~MODES_ENVELOPE;
	      ctl->cmsg(CMSG_INFO, VERB_DEBUG, 
			" - No sustain, removing envelope");
	    }
#endif /* ADAGIO */
	}


#define ENV_FS
#define MIR_R 40
/*#define MIR_R 40*/
/* Magic Increase Release_Rate */
	{
	/* Take up to 6 rate/offset pairs from envelope points;
	 * if there are fewer than 6, go to offset=8 at the specified
	 * release rate + an ad hoc adjustment MIR_R to speed up
	 * percussion releases.
	 */
	  int j, k, na, nr, so, sr;
	  na = e->envp_records[v].nattack;
	  nr = e->envp_records[v].nrelease;
	  so = e->envp_records[v].sustain_offset;
	  sr = e->envp_records[v].sustain_rate;
	  j = 0;

#ifdef NO_ENV_FILL
	  if (na + nr < 2)
	      sp->modes &= ~MODES_ENVELOPE;
#endif
	  if (!na) {
	    sp->envelope_offset[j] = 255;
	    sp->envelope_rate[j] = 63;
	    j++;
	  } else
	  for (k = 0; k < na && j < 6; k++,j++) {
	    sp->envelope_offset[j] = e->envp_records[v].envp_points_attack[k].offset;
	    sp->envelope_rate[j] = e->envp_records[v].envp_points_attack[k].rate;
	  }
#ifdef ENV_FS
	  for (; j < 3; j++) {
	    sp->envelope_offset[j] = (na)? e->envp_records[v].envp_points_attack[na-1].offset : 245;
	    sp->envelope_rate[j] = 193;
	  }
#endif
	  if (!nr) {
	    sp->envelope_offset[j] = 8;
	    sp->envelope_rate[j] = 63;
	    /* sp->envelope_rate[j] = 193 ; */
	    /*if (prog > 127) sp->envelope_rate[j] = 193;
	    else sp->envelope_rate[j] = e->envp_records[v].sustain_rate;*/
	    j++;
	  } else
	  for (k = 0; k < nr && j < 6; k++,j++) {
	    sp->envelope_offset[j] = e->envp_records[v].envp_points_release[k].offset;
	    sp->envelope_rate[j] = e->envp_records[v].envp_points_release[k].rate;
	  }
#ifdef ENV_FS
	  for (; j < 6; j++) {
	    sp->envelope_offset[j] = (nr)? e->envp_records[v].envp_points_release[nr-1].offset : 8;
	    /*sp->envelope_rate[j] = 193;*/
	    sp->envelope_rate[j] = 63;
	  }
#endif

#define ZERO_OFFSET 8
#ifndef ENV_FS
	  for (; j < 6; j++) {
	    sp->envelope_offset[j] = ZERO_OFFSET;
	    sp->envelope_rate[j] = e->envp_records[v].release_rate+MIR_R;
	  }
#endif

#ifdef ADAGIO
	  for (j = 0; j < 6; j++) {
	    if (gus_voice[tpgm].keep & USEENV_FLAG) {
		sp->envelope_offset[j] = gus_voice[tpgm].envelope_offset[j];
		sp->envelope_rate[j] = gus_voice[tpgm].envelope_rate[j]; 
	    }
	    else if (!i && current_side == LEFT_SIDE) {
		gus_voice[tpgm].envelope_offset[j] = sp->envelope_offset[j];
		gus_voice[tpgm].envelope_rate[j] = sp->envelope_rate[j];
	    }
	  }
#ifdef REV_E_ADJUST
	  if (reverb && prog < 120) {
	    int r = reverb;
	    int dec = sp->envelope_rate[2];
	    r = (127 - r) / 6;
	    if (r < 0) r = 0;
	    if (r > 28) r = 28;
	    r += dec & 0x3f;
	    if (r > 63) r = 63;
	    dec = (dec & 0xc0) | r;
	    sp->envelope_rate[2] = dec;
#if 0
	    r = reverb;
	    if (r > 127) r = 127;
	    if (prog < 120) sp->envelope_rate[3] = (2<<6) | (12 - (r>>4));
	    else if (prog > 127) sp->envelope_rate[1] = (3<<6) | (63 - (r>>1));
#endif
	  }
#endif /* REV_E_ADJUST */

#ifdef VOL_E_ADJUST
#define VR_NUM 2
#define VR_DEN 3
          for (j = 0; j < 6; j++) {
	    int voff, poff;
            voff = sp->envelope_offset[j];
            poff = 2 + main_volume + 63 + gus_voice[tpgm].volume / 2;
            voff = ((poff + VR_NUM*256) * voff + VR_DEN*128) / (VR_DEN*256);
            sp->envelope_offset[j] = voff;
          }
#endif /* VOL_E_ADJUST */

#endif /* ADAGIO */

/******debug*********
	if (prog==67)
	  for (j = 0; j < 6; j++) {
	printf("\t%d: rate %ld offset %ld\n", j, sp->envelope_rate[j], sp->envelope_offset[j]);
	  }
******debug*********/

	  for (j = 0; j < 6; j++) {
	    sp->envelope_offset[j] = convert_envelope_offset(sp->envelope_offset[j]);
	    sp->envelope_rate[j] =
	 (j<3)? convert_envelope_rate_attack(sp->envelope_rate[j], 11) :
	    		convert_envelope_rate(sp->envelope_rate[j]);
	  }
	}


    sp->data = (sample_t *)safe_malloc( 2 * w->size );

    if (lseek(f, (off_t)w->start, SEEK_SET) == -1) {
	perror("bad seek in data file");
  #ifdef ADAGIO
	  gus_voice[tpgm].loaded = 0;
  #endif
	return (0);
    }
    cnt = read(f, sp->data,  2 * w->size);

#ifdef FFFDEBUG
printf("read %d byte sample\n", cnt);
#endif
    if (cnt != 2 * w->size) {
	perror("bad read of data file");
	fprintf(stderr, "read %d bytes of desired %ld at offset %ld\n",
		 cnt, 2 * w->size, w->start);
  #ifdef ADAGIO
	  gus_voice[tpgm].loaded = 0;
  #endif
	return(0);
    }

      if (!(sp->modes & MODES_16BIT)) /* convert to 16-bit data */
	{
	  int32 i=sp->data_length;
	  uint8 *cp=(uint8 *)(sp->data);
	  uint16 *tmp,*new;
	  tmp=new=safe_malloc(sp->data_length*2);
	  while (i--)
	    *tmp++ = (uint16)(*cp++) << 8;
	  cp=(uint8 *)(sp->data);
	  sp->data = (sample_t *)new;
	  free(cp);
	  sp->data_length *= 2;
	  sp->loop_start *= 2;
	  sp->loop_end *= 2;
	}
#ifndef LITTLE_ENDIAN
      else
	/* convert to machine byte order */
	{
	  int32 i=w->size;
	  int16 *tmp=(int16 *)sp->data,s;
	  while (i--)
	    { 
	      s=LE_SHORT(*tmp);
	      *tmp++=s;
	    }
	}
#endif
      
      if (sp->modes & MODES_UNSIGNED) /* convert to signed data */
	{
	  int32 i=w->size;
	  int16 *tmp=(int16 *)sp->data;
	  while (i--)
	    *tmp++ ^= 0x8000;
	}

      /* Reverse reverse loops and pass them off as normal loops */
      if (sp->modes & MODES_REVERSE)
	{
	  int32 t;
	  /* The GUS apparently plays reverse loops by reversing the
	     whole sample. We do the same because the GUS does not SUCK. */

	  t=sp->loop_start;
	  sp->loop_start=sp->data_length - sp->loop_end;
	  sp->loop_end=sp->data_length - t;

	  sp->modes &= ~MODES_REVERSE;
	  sp->modes |= MODES_LOOPING; /* just in case */
	}

      /* If necessary do some anti-aliasing filtering  */

      if (antialiasing_allowed)
	  antialiasing(sp,play_mode->rate);

#ifdef ADJUST_SAMPLE_VOLUMES
      if (amp!=-1)
	sp->volume=(double)(amp) / 100.0;
      else
	{
/* For Adagio, this adjustment is never done, now, since amp value is taken from
 * gus_voice value. Maybe I should change this back.
 */
	  /* Try to determine a volume scaling factor for the sample.
	     This is a very crude adjustment, but things sound more
	     balanced with it. Still, this should be a runtime option. */
	  int32 i=sp->data_length/2;
	  int16 maxamp=0,a;
	  int16 *tmp=(int16 *)sp->data;
	  while (i--)
	    {
	      a=*tmp++;
	      if (a<0) a=-a;
	      if (a>maxamp)
		maxamp=a;
	    }
	  sp->volume=32768.0 / (double)(maxamp);
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * volume comp: %f", sp->volume);
	}
#else
      if (amp!=-1)
	sp->volume=(double)(amp) / 100.0;
      else
	sp->volume=1.0;
#endif

/* Following keeps high 4 bits of fraction. */ 
    /* Then fractional samples */
    sp->data_length <<= FRACTION_BITS;
    sp->loop_start <<= FRACTION_BITS;
    sp->loop_end <<= FRACTION_BITS;

    /* Adjust for fractional loop points. This is a guess. Does anyone
	 know what "fractions" really stands for? */
/* I think the low 4 bits of the fraction are always 0, but may as well
 * use them ...  I have no idea what they're for, but information should
 * never be discarded.
 */
    sp->loop_start |=
	(w->loopstart & 0x0F) << (FRACTION_BITS-4);
    sp->loop_end |=
	(w->loopend & 0x0F) << (FRACTION_BITS-4);

      /* If this instrument will always be played on the same note,
	 and it's not looped, we can resample it now. */
    if (sp->note_to_use && !(sp->modes & MODES_LOOPING))
	pre_resample(sp);
      
    if (w->next_wave) w = w->next_wave;
  }


  /**if (!(play_mode->encoding & PE_MONO)) {**/
    if (current_side == LEFT_SIDE && p->nlayers == 2 && l->next) {
      if (e->next) e = e->next;
      l = l->next;
      current_side = RIGHT_SIDE;
      goto other_side;
    }
    /* conceal the left/right distinction from timidity */
    ip->samples = ip->left_samples;
    ip->sample = ip->left_sample;
  /**}**/

  return(lp);
}


#ifdef ADAGIO
#ifndef FFF_DISPLAY_ONLY

int load_fff_gus_patch(int seq_fd, int prog, int tpgm, int reverb, int main_volume, int voicepan, int patch_mem_avail) {
#ifdef NO_DEV_SEQUENCER
    return(0);
#else
    extern int next_wave_prog;
    struct patch_info *patch;
    int cnt, i, bank, newmode;
    int amp=-1, note_to_use, strip_loop, strip_envelope, strip_tail;
    unsigned int mode;
    struct ptch_type *p;
    struct envp_type *e;
    struct layr_type *l;
    struct wave_type *w;
    int f, d_ix;
    int current_side = LEFT_SIDE;

    if (!(gus_voice[tpgm].keep & FFF_FLAG)) return(0);
    if (!(p=ptch[prog])) return(0);

    bank = gus_voice[tpgm].bank & 0x7f;
    newmode = gus_voice[tpgm].modes;

    while (p->bank != bank && p->next_bank) p = p->next_bank;
    if (p->bank != bank) return(0);

    if (prog < 128) note_to_use = prog;
    else note_to_use = prog - 128;

    d_ix = p->file_index;
    f = dat_opened[d_ix];
    if (f < 0) return(0);
    if (!f) {
      f = open(dat_filename[d_ix], O_RDONLY, 0);
      dat_opened[d_ix] = f;
      if (f < 0) {
    	    perror(dat_filename[d_ix]);
    	    return(0);
      }
    }


#ifdef FFFDEBUG
printf("patch for prog %d, note %d:\n", prog, note);
#endif
    e = p->envelope;
    l = p->layr;
    if (!l->nwaves) return(0);

/************/
    w = l->waves;
    for (i = 0, cnt = 0; i < l->nwaves; i++) {
      cnt += sizeof(*patch) + 2 * w->size;
      if (w->next_wave) w = w->next_wave;
    }
    if (p->nlayers == 2 && l->next) {
      if (e->next) e = e->next;
      l = l->next;
      w = l->waves;
      for (i = 0; i < l->nwaves; i++) {
        cnt += sizeof(*patch) + 2 * w->size;
        if (w->next_wave) w = w->next_wave;
      }
    }
    gus_voice[tpgm].mem_req = cnt;
    if (cnt + 100 > patch_mem_avail) return(0);
    e = p->envelope;
    l = p->layr;
/************/

    if (very_verbose) printf("Loading %s from %s\n",
	 gus_voice[tpgm].vname, dat_filename[d_ix]);
    gus_voice[tpgm].loaded |= GUS_MASK;

/* back here to load right part of a stereo patch */
other_side:


    if (current_side == LEFT_SIDE) {
	gus_voice[tpgm].prog = next_wave_prog;
    } else {
	gus_voice[tpgm].right_prog = next_wave_prog;
    }

    w = l->waves;

  for (i = 0; i < l->nwaves; i++) {
    int v;

    for (v = 0; v < l->nwaves && v < e->num_envelopes ; v++)
	if (e->envp_records[v].hirange >= w->high_note) break;

    patch = (struct patch_info *) malloc(sizeof(*patch) + 2 * w->size);

    patch->key = GUS_PATCH;
    patch->device_no = gus_dev;

    patch->instr_no = next_wave_prog;


    patch->len = 2 * w->size;
/**
    patch->loop_start = 2 * (w->loopstart >> 8);
    patch->loop_end = 2 * (w->loopend >> 8);
**/
    patch->loop_start = 2 * (w->loopstart >> 4);
    patch->loop_end = 2 * (w->loopend >> 4);

    patch->fractions = (int)( (0x0f & (w->loopstart >> 4)) | (0xf0 & w->loopend) );

#ifdef FFFDEBUG
printf("data_length %ld, loop_start %ld, loop_end %ld\n",
patch->len, patch->loop_start, patch->loop_end);
#endif


    patch->base_freq = 44100;

    patch->scale_frequency = l->freq_center;
    patch->scale_factor = l->freq_scale;

    if (prog > 127 && gus_voice[tpgm].fix_key) {
	patch->scale_factor = 0;
	note_to_use = patch->scale_frequency = gus_voice[tpgm].fix_key;
    }
    else if (!patch->scale_factor) gus_voice[tpgm].fix_key = patch->scale_frequency;

    if (prog > 127 && l->freq_scale == 1024) {
    /* for the SoundCanvas patch set, we should play the prog # */
	patch->scale_factor = 0;
	patch->scale_frequency = note_to_use;
    }

    /* calculate this? driver doesn't use it */
    patch->detuning = 0;

    if (patch->scale_factor) {
	note_to_use = -1;
	patch->low_note = freq_table[w->low_note];
	patch->high_note = freq_table[w->high_note];
    } else {
/* for the time being, the note_to_play logic for percussion is disabled,
 * and freq for note will be determined by freq_center */
	note_to_use = patch->scale_frequency;
	patch->low_note = patch->high_note = freq_table[note_to_use];
    }
    patch->base_note = w->sample_ratio;

#ifdef FFFDEBUG
printf("sample_rate %ld, low_freq %ld, high_freq %ld, root_freq %ld\n",
patch->base_freq, patch->low_note, patch->high_note, patch->base_note);
#endif


    if (current_side == LEFT_SIDE)
	gus_voice[tpgm].attenuation = 0x7f & (l->attenuation + w->attenuation);
    else gus_voice[tpgm].right_attenuation = 0x7f & (l->attenuation + w->attenuation);

    patch->panning = 2*(l->pan - 64);
    patch->resonance=0;
    patch->cutoff_freq=0;
    patch->reverberation=0;
    patch->chorusdepth=0;

    patch->tremolo_sweep = l->tremolo.sweep;
    patch->tremolo_rate = l->tremolo.freq;
    patch->tremolo_depth = l->tremolo.depth;

    patch->vibrato_sweep = l->vibrato.sweep;
    patch->vibrato_rate = l->vibrato.freq;
    patch->vibrato_depth = l->vibrato.depth;


    mode = WAVE_ENVELOPES | WAVE_FRACTIONS | WAVE_VIBRATO | WAVE_TREMOLO | WAVE_SCALE;
    if (e->mode == 2) mode |= WAVE_SUSTAIN_ON;
    else if (e->mode == 1) mode |= WAVE_FAST_RELEASE;
    if (!(w->format & 1)) mode |= WAVE_16_BITS;
    if (!(w->format & 2)) mode |= WAVE_UNSIGNED;
/* bit pos. for REVERSE and PINGPONG are inverted ? */
    if (!(w->format & 4)) mode |= WAVE_LOOP_BACK;
    if (w->format & 8) mode |= WAVE_LOOPING;

    if (w->format & 16) mode |= WAVE_BIDIR_LOOP;
    if (w->format & 32) mode |= WAVE_MULAW;

    patch->mode = mode;

      strip_loop = strip_envelope = strip_tail = 0;
      if (!i && current_side == LEFT_SIDE) {
        gus_voice[tpgm].vibrato_sweep = l->vibrato.sweep;
        gus_voice[tpgm].vibrato_rate = l->vibrato.freq;
        gus_voice[tpgm].vibrato_depth = l->vibrato.depth;
        if (!newmode) gus_voice[tpgm].modes = patch->mode;
      }
      if (newmode) patch->mode = newmode;

      amp = gus_voice[tpgm].volume;
/**
      if (gus_voice[tpgm].fix_key) {
	patch->scale_factor = 0;
	patch->scale_frequency = gus_voice[tpgm].fix_key;
      }
**/
      if (gus_voice[tpgm].strip & LOOPS_FLAG) strip_loop = 1;
      if (gus_voice[tpgm].strip & ENVELOPE_FLAG) strip_envelope = 1;
      if (gus_voice[tpgm].strip & TAIL_FLAG) strip_tail = 1;
      if (gus_voice[tpgm].keep & LOOPS_FLAG) strip_loop = 0;
      if (gus_voice[tpgm].keep & ENVELOPE_FLAG) strip_envelope = 0;
      if (gus_voice[tpgm].keep & TAIL_FLAG) strip_tail = 0;

#ifndef MIR_R
#define MIR_R 40
#endif
/* Magic Increase Release_Rate */
	{
	/* Take up to 6 rate/offset pairs from envelope points;
	 * if there are fewer than 6, go to offset=8 at the specified
	 * release rate + an ad hoc adjustment MIR_R to speed up
	 * percussion releases.
	 */
	  int j, k, na, nr, so, sr;
#ifdef GVOL_E_ADJUST
	  int voff, poff;
#endif
	  na = e->envp_records[v].nattack;
	  nr = e->envp_records[v].nrelease;
	  so = e->envp_records[v].sustain_offset;
	  sr = e->envp_records[v].sustain_rate;
	  j = 0;
/*#if 0*/
	  for (k = 0; k < na && j < 6; k++,j++) {
	    patch->env_offset[j] = e->envp_records[v].envp_points_attack[k].offset;
	    patch->env_rate[j] = e->envp_records[v].envp_points_attack[k].rate;
	  }
	  for (k = 0; k < nr && j < 6; k++,j++) {
	    patch->env_offset[j] = e->envp_records[v].envp_points_release[k].offset;
	    patch->env_rate[j] = e->envp_records[v].envp_points_release[k].rate;
	  }
	  for (; j < 6; j++) {
	    patch->env_offset[j] = 8;
	    patch->env_rate[j] = e->envp_records[v].release_rate+MIR_R;
	  }
/*#endif*/


#if 0

#ifdef NO_ENV_FILL
	  if (na + nr < 2)
	      patch->mode &= ~WAVE_ENVELOPES;
#endif
	  if (!na) {
	    patch->env_offset[j] = 255;
	    patch->env_rate[j] = 63;
	    j++;
	  } else
	  for (k = 0; k < na && j < 6; k++,j++) {
	    patch->env_offset[j] = e->envp_records[v].envp_points_attack[k].offset;
	    patch->env_rate[j] = e->envp_records[v].envp_points_attack[k].rate;
	  }
#ifdef ENV_FS
	  for (; j < 3; j++) {
	    patch->env_offset[j] = (na)? e->envp_records[v].envp_points_attack[na-1].offset : 245;
	    patch->env_rate[j] = 193;
	  }
#endif
	  if (!nr) {
	    patch->env_offset[j] = 8;
	    /*patch->env_rate[j] = 193;*/
	    if (prog > 127) patch->env_rate[j] = 193;
	    else patch->env_rate[j] = e->envp_records[v].sustain_rate;
	    j++;
	  } else
	  for (k = 0; k < nr && j < 6; k++,j++) {
	    patch->env_offset[j] = e->envp_records[v].envp_points_release[k].offset;
	    patch->env_rate[j] = e->envp_records[v].envp_points_release[k].rate;
	  }
#ifdef ENV_FS
	  for (; j < 6; j++) {
	    patch->env_offset[j] = (nr)? e->envp_records[v].envp_points_release[nr-1].offset : 8;
	    patch->env_rate[j] = 193;
	  }
#endif

#ifndef ENV_FS
#define ZERO_OFFSET 8
	  for (; j < 6; j++) {
	    patch->env_offset[j] = ZERO_OFFSET;
	    patch->env_rate[j] = e->envp_records[v].release_rate+MIR_R;
	  }
#endif

	  for (j = 0; j < 6; j++) {
	    if (gus_voice[tpgm].keep & USEENV_FLAG) {
		patch->env_offset[j] = gus_voice[tpgm].envelope_offset[j];
		patch->env_rate[j] = gus_voice[tpgm].envelope_rate[j]; 
	    }
	    else if (!i && current_side == LEFT_SIDE) {
		gus_voice[tpgm].envelope_offset[j] = patch->env_offset[j];
		gus_voice[tpgm].envelope_rate[j] = patch->env_rate[j];
	    }
	  }

#ifdef GREV_E_ADJUST
	  if (reverb) {
	    int r = reverb;
	    int dec = patch->env_rate[2];
	    r = (127 - r) / 6;
	    if (r < 0) r = 0;
	    if (r > 28) r = 28;
	    r += dec & 0x3f;
	    if (r > 63) r = 63;
	    dec = (dec & 0xc0) | r;
	    patch->env_rate[2] = dec;
	    r = reverb;
	    if (r > 127) r = 127;
	    if (prog < 120) patch->env_rate[3] = (2<<6) | (12 - (r>>4));
	    else if (prog > 127) patch->env_rate[1] = (3<<6) | (63 - (r>>1));
	  }
#endif

#ifdef GVOL_E_ADJUST
#ifndef VR_NUMG
#define VR_NUMG 3
#define VR_DENG 5
#endif
          for (j = 0; j < 6; j++) {
            voff = patch->env_offset[j];
            poff = 2 + main_volume + 63 + gus_voice[tpgm].volume / 2;
            voff = ((poff + VR_NUMG*256) * voff + VR_DENG*128) / (VR_DENG*256);
            patch->env_offset[j] = voff;
          }
#endif

#endif /* #if 0 */
	}

    patch->volume = (int)gus_voice[tpgm].volume;

    if (lseek(f, (off_t)w->start, SEEK_SET) == -1) {
	perror("bad seek in data file");
	gus_voice[tpgm].loaded = 0;
	return (0);
    }
    cnt = read(f, patch->data,  2 * w->size);

#ifdef FFFDEBUG
printf("read %d byte sample\n", cnt);
#endif
    if (cnt != 2 * w->size) {
	perror("bad read of data file");
	gus_voice[tpgm].loaded = 0;
	return(0);
    }

    if (write(seq_fd, (char *) patch, sizeof(*patch) + patch->len) == -1) {
        free(patch);
	fprintf(stderr, "couldn't send fff patch to gus\n");
	gus_voice[tpgm].loaded = 0;
        return (0);
    }
    /* patch_mem_used += patch->len; */
    free(patch);

    if (w->next_wave) w = w->next_wave;
  }

  next_wave_prog++;

  if (current_side == LEFT_SIDE && p->nlayers == 2 && l->next) {
      if (e->next) e = e->next;
      l = l->next;
      current_side = RIGHT_SIDE;
      goto other_side;
  }

  return(1);
#endif /* !NO_DEV_SEQUENCER */
}
#endif
#endif /* ADAGIO */

#endif
