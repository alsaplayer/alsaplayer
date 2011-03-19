/*================================================================
 * SoundFont file extension
 *	written by Takashi Iwai <iwai@dragon.mm.t.u-tokyo.ac.jp>
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
 *================================================================*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "gtim.h"
#ifdef LITTLE_ENDIAN
#define USE_POSIX_MAPPED_FILES
#endif
#define READ_WHOLE_SF_FILE
#ifdef READ_WHOLE_SF_FILE
#include <sys/stat.h>
#endif
#ifdef USE_POSIX_MAPPED_FILES
#include <sys/mman.h>
#endif
#include "common.h"
#include "tables.h"
#include "instrum.h"
#include "playmidi.h"
#include "effects.h"
#include "md.h"
#include "controls.h"
#include "sbk.h"
#include "sflayer.h"
#include "output.h"
#include "filter.h"
#include "resample.h"

/*----------------------------------------------------------------
 * compile flags
 *----------------------------------------------------------------*/

/* use some modifications from TiMidity++ */
#define tplussbkuse

/*#define GREGSTEST*/

#ifdef ADAGIO
#define SF_SUPPRESS_VIBRATO
#endif


/*----------------------------------------------------------------
 * local parameters
 *----------------------------------------------------------------*/

typedef struct _Layer {
	int16 val[PARM_SIZE];
	int8 set[PARM_SIZE];
} Layer;

typedef struct _SampleList {
	Sample v;
	struct _SampleList *next;
	uint32 startsample, endsample;
	uint32 cutoff_freq;
	FLOAT_T resonance;
} SampleList;

typedef struct _InstList {
	int bank, preset, keynote;
	int inum, velrange;
	int samples, rsamples;
	int already_loaded;
	char *fname;
	FILE *fd;
#ifdef READ_WHOLE_SF_FILE
	unsigned char *contents;
	int size_of_contents;
#endif
	SampleList *slist, *rslist;
	struct _InstList *next;
} InstList;

typedef struct SFInsts {
	char *fname;
	FILE *fd;
	uint16 version, minorversion;
	int32 samplepos, samplesize;
#ifdef READ_WHOLE_SF_FILE
	unsigned char *contents;
	int size_of_contents;
#endif
	InstList *instlist;
} SFInsts;


/*----------------------------------------------------------------*/

static int load_one_side(SFInsts *rec, SampleList *sp, int sample_count, Sample *base_sample, int amp);
static Instrument *load_from_file(SFInsts *rec, InstList *ip, int amp);
static void parse_preset(SFInsts *rec, SFInfo *sf, int preset);
static void parse_gen(Layer *lay, tgenrec *gen);
static void parse_preset_layer(Layer *lay, SFInfo *sf, int idx);
static void merge_layer(Layer *dst, Layer *src);
static int search_inst(Layer *lay);
static void parse_inst(SFInsts *rec, Layer *pr_lay, SFInfo *sf, int preset, int inst, int inum, int num_i);
static void parse_inst_layer(Layer *lay, SFInfo *sf, int idx);
static int search_sample(Layer *lay);
static void append_layer(Layer *dst, Layer *src, SFInfo *sf);
static void make_inst(SFInsts *rec, Layer *lay, SFInfo *sf, int pr_idx, int in_idx, int inum,
	uint16 pk_range, uint16 pv_range, int num_i, int program);
static int32 calc_root_pitch(Layer *lay, SFInfo *sf, SampleList *sp, int32 cfg_tuning);
static void convert_volume_envelope(Layer *lay, SFInfo *sf, SampleList *sp, int banknum, int preset);
static void convert_modulation_envelope(Layer *lay, SFInfo *sf, SampleList *sp, int banknum, int preset);
static uint32 to_offset(uint32 offset);
static uint32 calc_rate(uint32 diff, double msec);
static double to_msec(Layer *lay, SFInfo *sf, int index);
static FLOAT_T calc_volume(Layer *lay, SFInfo *sf);
static uint32 calc_sustain(Layer *lay, SFInfo *sf, int banknum, int preset);
static uint32 calc_modulation_sustain(Layer *lay, SFInfo *sf, int banknum, int preset);
#ifndef SF_SUPPRESS_TREMOLO
static void convert_tremolo(Layer *lay, SFInfo *sf, SampleList *sp);
static void convert_lfo(Layer *lay, SFInfo *sf, SampleList *sp);
#endif
#ifndef SF_SUPPRESS_VIBRATO
static void convert_vibrato(Layer *lay, SFInfo *sf, SampleList *sp);
#endif
static void calc_cutoff(Layer *lay, SFInfo *sf, SampleList *sp);
static void calc_filterQ(Layer *lay, SFInfo *sf, SampleList *sp);

/*----------------------------------------------------------------*/

#define MAX_SF_FILES 40
static int current_sf_index = 0;
static int last_sf_index = 0;

static SFInsts sfrec[MAX_SF_FILES];

int cutoff_allowed = 0;
int command_cutoff_allowed = 0;


#ifdef GREGSTEST
static char *getname(char *p)
{
	static char buf[21];
	strncpy(buf, p, 20);
	buf[20] = 0;
	return buf;
}
#endif

static SFInfo sfinfo;


#ifdef READ_WHOLE_SF_FILE
static int sf_size_of_contents;

#ifndef USE_POSIX_MAPPED_FILES
static unsigned char *read_whole_sf(FILE *fd) {
#else
static unsigned char *read_whole_sf() {
#endif
    struct stat info;
    unsigned char *sf_contents;

    sf_size_of_contents = 0;

#ifndef LITTLE_ENDIAN
    return 0;
#else

#ifndef USE_POSIX_MAPPED_FILES
#ifndef KMIDI
    if (have_commandline_midis < 3) return 0;
#endif
#endif
    if (stat(current_filename, &info)) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Can't find file `%s'.", current_filename);
    	return 0;
    }
/* check here if size less than what we are allowed */
    if (info.st_size + current_patch_memory > max_patch_memory) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "File `%s' at %d bytes is too big to read all at once.",
		 current_filename, info.st_size);
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "  (I will try to read it one patch at a time.)");
	return 0;
    }

#ifdef USE_POSIX_MAPPED_FILES
    sf_contents = (unsigned char *)mmap(0, info.st_size,  PROT_READ,
	 MAP_SHARED, current_filedescriptor, 0);

    if (sf_contents == (unsigned char *)(-1)) {
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Couldn't mmap `%s'.", current_filename);
    	return 0;
    }
#else
    sf_contents = (unsigned char *)malloc(info.st_size);

    if (!sf_contents) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Couldn't get memory for entire file `%s'.", current_filename);
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "  (I will try to read it one patch at a time.)");
    	return 0;
    }
    rewind(fd);
    if (!fread(sf_contents, info.st_size, 1, fd)) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Couldn't read `%s'.", current_filename);
    	free(sf_contents);
    	return 0;
    }
#endif
    sf_size_of_contents = info.st_size;
    current_patch_memory += info.st_size;
    ctl->cmsg(CMSG_INFO, VERB_NOISY, "File `%s' required %d bytes of memory.", current_filename, sf_size_of_contents);
    return sf_contents;
#endif
}
#endif

/*
 * init_soundfont
 *	fname: name of soundfont file
 *	newbank: toneset # or, if >255, drumset # + 256
 *	oldbank: same as newbank unless cfg file has "sf <name> oldbank=<num>",
 *		in which case oldbank=<num>
 *	level: nesting level of cfg file (what is this for?)
 */
int init_soundfont(char *fname, int oldbank, int newbank, int level)
{
	int i;
	InstList *ip;
#ifdef READ_WHOLE_SF_FILE
	unsigned char *sf_contents = 0;
	int whole_sf_already_read = 0;
#endif

	ctl->cmsg(CMSG_INFO, VERB_NOISY, "init soundfont `%s'", fname);

	current_sf_index = -1;
	for (i = 0; i < last_sf_index; i++) if (sfrec[i].fname && !strcmp(sfrec[i].fname, fname)) {
		current_sf_index = i;
		rewind(sfrec[i].fd);
#ifdef READ_WHOLE_SF_FILE
		whole_sf_already_read = 1;
#endif
		break;
	}

	if (current_sf_index == -1) {
	    if (last_sf_index >= MAX_SF_FILES) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Too many soundfont files to load %s.", fname);
		return FALSE;
	    }
	    current_sf_index = last_sf_index;

	    if ((sfrec[current_sf_index].fd = open_file(fname, 1, OF_VERBOSE, level)) == NULL) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Can't open soundfont file %s.", fname);
		return FALSE;
	    }
	    last_sf_index++;
	    sfrec[current_sf_index].fname = strcpy((char*)safe_malloc(strlen(fname)+1), fname);
	}

	load_sbk(sfrec[current_sf_index].fd, &sfinfo);

	for (i = 0; i < sfinfo.nrpresets-1; i++) {
		/* if substituting banks, only parse matching bank */
		if (oldbank != newbank) {
			int bank = sfinfo.presethdr[i].bank;
			int preset = sfinfo.presethdr[i].preset;
			/* is it a matching percussion preset? */
			if (newbank >= 256 && bank == 128 && preset == oldbank) {
				preset = sfinfo.presethdr[i].sub_preset = newbank - 256;
				parse_preset(&sfrec[current_sf_index], &sfinfo, i);
			}
			/* is it a matching melodic bank? */
			else if (bank == oldbank) {
				bank = sfinfo.presethdr[i].sub_bank = newbank;
				parse_preset(&sfrec[current_sf_index], &sfinfo, i);
			}
		}
		/* but if not substituting banks, parse them all */
		else parse_preset(&sfrec[current_sf_index], &sfinfo, i);
	}

	/* copy header info */
	sfrec[current_sf_index].version = sfinfo.version;
	sfrec[current_sf_index].minorversion = sfinfo.minorversion;
	sfrec[current_sf_index].samplepos = sfinfo.samplepos;
	sfrec[current_sf_index].samplesize = sfinfo.samplesize;

	free_sbk(&sfinfo);


#ifdef READ_WHOLE_SF_FILE
	if (whole_sf_already_read) {
		sf_contents = sfrec[current_sf_index].contents;
		sf_size_of_contents = sfrec[current_sf_index].size_of_contents;
	}
	else
#ifndef USE_POSIX_MAPPED_FILES
	sf_contents = read_whole_sf(sfrec[current_sf_index].fd);
#else
	sf_contents = read_whole_sf();
#endif
	if (!sf_contents) return FALSE;
	sfrec[current_sf_index].contents = sf_contents;
	sfrec[current_sf_index].size_of_contents = sf_size_of_contents;

	for (ip = sfrec[current_sf_index].instlist; ip; ip = ip->next) {
	    if (!ip->already_loaded) {
		ip->contents = sf_contents;
		ip->size_of_contents = sf_size_of_contents;
	    }
	    ip->already_loaded = 10000000;
	}
#else
	/* mark instruments as loaded so they won't be loaded again if we're re-called */
	for (ip = sfrec[current_sf_index].instlist; ip; ip = ip->next) ip->already_loaded = 10000000;
#endif
	return TRUE;
}


void end_soundfont(void)
{
    InstList *ip, *next;
    FILE *still_open;
    char *still_not_free;
    unsigned char *contents_not_free;
    int contents_size = 0;

    current_sf_index = 0;

    while (current_sf_index < last_sf_index) {

	if (!sfrec[current_sf_index].instlist) continue;

	still_open = NULL;
	still_not_free = NULL;
	contents_not_free = NULL;

	while (1) {
	    for (ip = sfrec[current_sf_index].instlist; ip; ip = next) {
		next = ip->next;
		if (!still_open && ip->fd) {
		    still_open = ip->fd;
		    still_not_free = ip->fname;
		    contents_not_free = ip->contents;
		    contents_size = ip->size_of_contents;
		}
		if (still_open && ip->fd == still_open) ip->fd = NULL;
	    }
	    if (still_open) {
		fclose(still_open);
		still_open = NULL;
		if (still_not_free) free(still_not_free);
		still_not_free = NULL;
		if (contents_not_free) {
#ifdef USE_POSIX_MAPPED_FILES
		    munmap(contents_not_free, contents_size);
#else
		    free(contents_not_free);
#endif
		    current_patch_memory -= contents_size;
		}
		contents_not_free = NULL;
		contents_size = 0;
	    }
	    else break;
	}

	for (ip = sfrec[current_sf_index].instlist; ip; ip = next) {
		next = ip->next;
		free(ip);
	}

	sfrec[current_sf_index].version =
	sfrec[current_sf_index].minorversion =
	sfrec[current_sf_index].samplepos =
	sfrec[current_sf_index].samplesize = 0;
	sfrec[current_sf_index].instlist = NULL;
	sfrec[current_sf_index].fd = NULL;

	current_sf_index++;
    }

    current_sf_index = last_sf_index = 0;
}

/*----------------------------------------------------------------
 * get converted instrument info and load the wave data from file
 *----------------------------------------------------------------*/

/* two (high/low) 8 bit values in 16 bit parameter */
#define LO_VAL(val)	((val) & 0xff)
#define HI_VAL(val)	(((val) >> 8) & 0xff)
#define SET_LO(vp,val)	((vp) = ((vp) & 0xff00) | (val))
#define SET_HI(vp,val)	((vp) = ((vp) & 0xff) | ((val) << 8))

static int patch_memory;

#ifdef ADAGIO
InstrumentLayer *load_sbk_patch(int gm_num, int tpgm, int reverb, int main_volume) {
    extern int next_wave_prog;
    char *name;
    int percussion, amp=-1, keynote, strip_loop, strip_envelope, strip_tail, bank, newmode;
#else
InstrumentLayer *load_sbk_patch(const char *name, int gm_num, int bank, int percussion,
			   int panning, int amp, int keynote, int sf_ix) {
#endif
	int preset;
	int not_done, load_index=0;
	InstList *ip;
	InstrumentLayer *lp, *nlp;
	Instrument *inst = NULL;

    preset = gm_num;
    if (gm_num >= 128) preset -= 128;

#ifdef ADAGIO
    if (!(gus_voice[tpgm].keep & SBK_FLAG)) return(0);
    bank = gus_voice[tpgm].bank & 0x7f;
    newmode = gus_voice[tpgm].modes;
    strip_loop = strip_envelope = strip_tail = amp = -1;
    percussion = (gm_num >= 128);
    name = gus_voice[tpgm].vname;
/*
    if (very_verbose) printf("load_sbk_patch(%s:bank=%d,preset=%d,tpgm=%d,reverb=%d,main_volume=%d) keep 0x%02X\n",
	name,bank,preset,tpgm,reverb,main_volume, gus_voice[tpgm].keep);
*/
#endif

    if (percussion) {
	keynote = preset;
	preset = bank;
	bank = 128;
    }
    else keynote = -1;

    if (sf_ix < 0 || sf_ix >= MAX_SF_FILES) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Indexing non-existent soundfont %d.",
		sf_ix);
	return 0;
    }
    if (!sfrec[sf_ix].fname || !sfrec[sf_ix].instlist) {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Indexing uninitialized soundfont %d.",
		sf_ix);
	return 0;
    }

    cutoff_allowed = command_cutoff_allowed;

    patch_memory = 0;

    not_done = 1;
    lp = 0;

    while (not_done) {

	for (ip = sfrec[sf_ix].instlist; ip; ip = ip->next) {
		if (ip->bank == bank && ip->preset == preset &&
			ip->already_loaded > load_index &&
		    (keynote < 0 || keynote == ip->keynote))
			break;
	}
	inst = 0;
	if (ip && (ip->samples || ip->rsamples)) {
		if (!load_index)
			load_index = ip->already_loaded - 1;
		ip->already_loaded = load_index;
		sfrec[sf_ix].fname = ip->fname;
#ifdef READ_WHOLE_SF_FILE
		sfrec[sf_ix].contents = ip->contents;
#endif
    		ctl->cmsg(CMSG_INFO, VERB_NOISY, "%s%s[%d,%d] %s%s (%d-%d)",
			(percussion)? "   ":"", name,
			(percussion)? keynote : preset, (percussion)? preset : bank,
			(ip->rsamples)? "(2) " : "",
			sfrec[sf_ix].fname,
			LO_VAL(ip->velrange),
				HI_VAL(ip->velrange)? HI_VAL(ip->velrange) : 127 );
		sfrec[sf_ix].fd = ip->fd;
		inst = load_from_file(&sfrec[sf_ix], ip, amp);
#ifdef READ_WHOLE_SF_FILE
		if (inst) inst->contents = ip->contents;
#endif
	}
	else {
		ip = 0;
		not_done = 0;
		if (!lp) {
		    ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Can't find %s %s[%d,%d] in %s.",
			(percussion)? "drum":"instrument", name,
			(percussion)? keynote : preset, (percussion)? preset : bank, sfrec[sf_ix].fname);
		    return 0;
		}
	}


#ifdef ADAGIO
	if (inst) {
	    gus_voice[tpgm].loaded |= DSP_MASK;
	    gus_voice[tpgm].prog = next_wave_prog++;
	}
#endif
	if (inst) {
		nlp = lp;
		lp=(InstrumentLayer*) safe_malloc(sizeof(InstrumentLayer));
		patch_memory += sizeof(InstrumentLayer);
		lp->lo = LO_VAL(ip->velrange);
		lp->hi = HI_VAL(ip->velrange);
		lp->instrument = inst;
		lp->next = nlp;
	}

    	lp->size = patch_memory;
	if (check_for_rc()) return lp;
	if (patch_memory + current_patch_memory > max_patch_memory) return lp;

    } /* while (not_done) */

    lp->size = patch_memory;
    return lp;
}


static Instrument *load_from_file(SFInsts *rec, InstList *ip, int amp)
{
	Instrument *inst;
	int r_amp = amp;
/*
	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Loading SF bank %d prg %d note %d from %s",
		  ip->bank, ip->preset, ip->keynote, rec->fname);
*/

	inst = (Instrument *)safe_malloc(sizeof(Instrument));
	patch_memory += sizeof(Instrument);
	inst->type = INST_SF2;
	/* we could have set up right samples but no left ones */
	if (!ip->samples && ip->rsamples && ip->rslist) {
		ip->samples = ip->rsamples;
		ip->slist = ip->rslist;
		ip->rsamples = 0;
		ip->rslist = 0;
	}
	inst->samples = ip->samples;
	inst->sample = (Sample *)safe_malloc(sizeof(Sample)*ip->samples);
	patch_memory += sizeof(Sample)*ip->samples;

	inst->left_samples = inst->samples;
	inst->left_sample = inst->sample;
	inst->right_samples = ip->rsamples;
/*#define PSEUDO_STEREO*/
#ifdef PSEUDO_STEREO
	if (!ip->rsamples && ip->keynote >= 0) {
		ip->rsamples = ip->samples;
		ip->rslist = ip->slist;
		if (ip->keynote % 2) r_amp = -2;
		else r_amp = -3;
	}
#endif
	if (ip->rsamples) inst->right_sample = (Sample *)safe_malloc(sizeof(Sample)*ip->rsamples);
	else inst->right_sample = 0;

	if (load_one_side(rec, ip->slist, ip->samples, inst->sample, amp) &&
	    load_one_side(rec, ip->rslist, ip->rsamples, inst->right_sample, r_amp))
		return inst;

	if (inst->right_sample) free(inst->right_sample);
	free(inst->sample);
	free(inst);
	return 0;
}


static int load_one_side(SFInsts *rec, SampleList *sp, int sample_count, Sample *base_sample, int amp)
{
	int i;
	uint32 samplerate_save;

	for (i = 0; i < sample_count && sp; i++, sp = sp->next) {
		Sample *sample = base_sample + i;
#ifndef LITTLE_ENDIAN
		int32 j;
		int16 *tmp, s;
#endif
		memcpy(sample, &sp->v, sizeof(Sample));
/* here, if we read whole file, sample->data = rec.contents + sp->startsample */
#ifdef READ_WHOLE_SF_FILE
	    if (rec->contents)
		sample->data = (sample_t *)(rec->contents + sp->startsample);
		else
#endif
		sample->data = (sample_t *) safe_malloc(sp->endsample);
#ifndef READ_WHOLE_SF_FILE
		patch_memory += sp->endsample;
#else
	    if (!rec->contents)
#endif
		if (fseek(rec->fd, (int)sp->startsample, SEEK_SET)) {
			ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Can't find sample in file!\n");
			return 0;
		}
#ifdef READ_WHOLE_SF_FILE
	    if (!rec->contents)
#endif
		if (!fread(sample->data, sp->endsample, 1, rec->fd)) {
			ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Can't read sample from file!\n");
			return 0;
		}

#ifndef LITTLE_ENDIAN
/* NOTE: only do this once for all samples when first loaded ?? */
		tmp = (int16*)sample->data;
		for (j = 0; j < sp->endsample/2; j++) {
			s = LE_SHORT(*tmp);
			*tmp++ = s;
		}
#endif

	/* Note: _One_ thing that keeps this from working right is that,
	   apparently, zero-crossing points change so loops give clicks;
	   Another problem is that have to reload voices between songs.
	   The filter really ought to be in resample.c.
	*/
		/* do some filtering if necessary */
		/* (moved below -- should it be here?) */

#ifdef PSEUDO_STEREO
	if (amp<-1) {
		if (sp->v.panning >= 64) sp->v.panning = 0;
		else sp->v.panning = 127;
		if (amp==-3) sp->v.panning = 127 - sp->v.panning;
		/*amp = 80;*/
		amp = -1;
	}
#endif

#ifdef ADJUST_SAMPLE_VOLUMES
      if (amp!=-1)
	sample->volume=(double)(amp) / 100.0;
      else
	{
	  /* Try to determine a volume scaling factor for the sample.
	     This is a very crude adjustment, but things sound more
	     balanced with it. Still, this should be a runtime option. */
	  uint32 i, numsamps=sp->endsample/2;
	  uint32 higher=0, highcount=0;
	  int16 maxamp=0,a;
	  int16 *tmp=(int16 *)sample->data;
	  i = numsamps;
	  while (i--)
	    {
	      a=*tmp++;
	      if (a<0) a=-a;
	      if (a>maxamp)
		maxamp=a;
	    }
	  tmp=(int16 *)sample->data;
	  i = numsamps;
	  while (i--)
	    {
	      a=*tmp++;
	      if (a<0) a=-a;
	      if (a > 3*maxamp/4)
		{
		   higher += a;
		   highcount++;
		}
	    }
	  if (highcount) higher /= highcount;
	  else higher = 10000;
	  sample->volume = (32768.0 * 0.875) /  (double)higher ;
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG, " * volume comp: %f", sample->volume);
	}
#else
      if (amp!=-1)
	sample->volume=(double)(amp) / 100.0;
      else
	sample->volume=1.0;
#endif


	if (antialiasing_allowed) {
		/* restore the normal value */
		sample->data_length >>= FRACTION_BITS;
		antialiasing(sample, play_mode->rate);
		/* convert again to the fractional value */
		sample->data_length <<= FRACTION_BITS;
	}

		/* resample it if possible */
		samplerate_save = sample->sample_rate;
    /* trim off zero data at end */
    {
	int ls = sample->loop_start>>FRACTION_BITS;
	int le = sample->loop_end>>FRACTION_BITS;
	int se = sample->data_length>>FRACTION_BITS;
	while (se > 1 && !sample->data[se-1]) se--;
	if (le > se) le = se;
	if (ls >= le) sample->modes &= ~MODES_LOOPING;
	sample->loop_end = le<<FRACTION_BITS;
	sample->data_length = se<<FRACTION_BITS;
    }


#ifndef READ_WHOLE_SF_FILE
/* NOTE: tell pre_resample not to free */
	if (sample->note_to_use && !(sample->modes & MODES_LOOPING))
		pre_resample(sample);
#endif

/*fprintf(stderr,"sample %d, note_to_use %d\n", i, sample->note_to_use);*/
#ifdef LOOKUP_HACK
	/* squash the 16-bit data into 8 bits. */
	{
		uint8 *gulp,*ulp;
		int16 *swp;
		int l = sample->data_length >> FRACTION_BITS;
		gulp = ulp = safe_malloc(l + 1);
		swp = (int16 *)sample->data;
		while (l--)
			*ulp++ = (*swp++ >> 8) & 0xFF;
		free(sample->data);
		sample->data=(sample_t *)gulp;
	}
#endif

	if (!sample->sample_rate && !dont_filter_drums) {
    	        if (!i) ctl->cmsg(CMSG_INFO, VERB_DEBUG, "cutoff = %ld ; resonance = %g",
			sp->cutoff_freq, sp->resonance);
#ifdef TOTALLY_LOCAL
		do_lowpass(sample, samplerate_save, sample->data, sample->data_length >> FRACTION_BITS,
			sp->cutoff_freq, sp->resonance);
#endif
	}

/*
printf("loop start %ld, loop end %ld, len %d\n", sample->loop_start>>FRACTION_BITS, sample->loop_end>>FRACTION_BITS,
sample->data_length >> FRACTION_BITS);
*/

/* #define HANNU_CLICK_REMOVAL */
/*#define DEBUG_CLICK*/
#ifdef HANNU_CLICK_REMOVAL
    if ((sample->modes & MODES_LOOPING)) {
	int nls = sample->loop_start>>FRACTION_BITS;
	int nle = sample->loop_end>>FRACTION_BITS;
	int ipt = 0, ips = 0;
	int v1, v2, inls, inle;
	inls = nls;
	inle = nle;
	while (!ipt) {
		v1 = sample->data[nle-1];  v2 = sample->data[nle];
		if (v2==0) {
			if (v1 < 0) ipt = 1;
			else if (v1 > 0) ipt = 2;
		}
		else {
			if (v1 <= 0 && v2 > 0) ipt = 1;
			else if (v1 >= 0 && v2 < 0) ipt = 2;
		}
		if (!ipt) nle--;
		if (nle <= inls) break;
	}
	if (ipt && nls > 0) while (!ips) {
		v1 = sample->data[nls-1];  v2 = sample->data[nls];
		if (v2==0) {
			if (ipt == 1 && v1 < 0) ips = 1;
			else if (ipt == 2 && v1 > 0) ips = 2;
		}
		else {
			if (ipt == 1 && v1 <= 0 && v2 > 0) ips = 1;
			else if (ipt == 2 && v1 >= 0 && v2 < 0) ips = 2;
		}
		if (!ips) nls--;
		if (nls < 1) break;
	}
	if (ipt && ips && ipt == ips && (nle-nls) == (inle-inls)) {
#ifdef DEBUG_CLICK
printf("changing loop start from %d to %d, loop end from %d to %d, len %d to %d\n",
inls, nls,
inle, nle,
inle-inls, nle-nls);
#endif
		sample->loop_start = nls<<FRACTION_BITS;
		sample->loop_end = nle<<FRACTION_BITS;
	}
    }
#endif
	}
	return 1;
}

/*----------------------------------------------------------------
 * parse a preset
 *----------------------------------------------------------------*/

static void parse_preset(SFInsts *rec, SFInfo *sf, int preset)
{
	int from_ndx, to_ndx;
	Layer lay, glay;
	int i, inst, inum, num_i;

	from_ndx = sf->presethdr[preset].bagNdx;
	to_ndx = sf->presethdr[preset+1].bagNdx;
	num_i = to_ndx - from_ndx;

#ifdef GREGSTEST
if (to_ndx - from_ndx > 1) {
fprintf(stderr,"Preset #%d (%s) has %d(-1) instruments.\n", preset,
 getname(sf->presethdr[preset].name), to_ndx - from_ndx);
}
#endif

	memset(&glay, 0, sizeof(glay));
	for (i = from_ndx, inum = 0; i < to_ndx; i++) {
		memset(&lay, 0, sizeof(Layer));
		parse_preset_layer(&lay, sf, i);
		inst = search_inst(&lay);
		if (inst < 0) {/* global layer */
			memcpy(&glay, &lay, sizeof(Layer));
			num_i--;
		}
		else {
			/* append_layer(&lay, &glay, sf); */
			merge_layer(&lay, &glay);
			parse_inst(rec, &lay, sf, preset, inst, inum, num_i);
			inum++;
		}
	}
}

/* map a generator operation to the layer structure */
static void parse_gen(Layer *lay, tgenrec *gen)
{
	lay->set[gen->oper] = 1;
	lay->val[gen->oper] = gen->amount;
}

/* parse preset generator layers */
static void parse_preset_layer(Layer *lay, SFInfo *sf, int idx)
{
	int i;
	for (i = sf->presetbag[idx]; i < sf->presetbag[idx+1]; i++)
		parse_gen(lay, sf->presetgen + i);
}


/* merge two layers; never overrides on the destination */
static void merge_layer(Layer *dst, Layer *src)
{
	int i;
	for (i = 0; i < PARM_SIZE; i++) {
		if (src->set[i] && !dst->set[i]) {
			dst->val[i] = src->val[i];
			dst->set[i] = 1;
		}
	}
}

/* search instrument id from the layer */
static int search_inst(Layer *lay)
{
	if (lay->set[SF_instrument])
		return lay->val[SF_instrument];
	else
		return -1;
}

/* parse an instrument */
static void parse_inst(SFInsts *rec, Layer *pr_lay, SFInfo *sf, int preset, int inst, int inum, int num_i)
{
	int from_ndx, to_ndx;
	int i, sample;
	uint16 pv_range=0, pk_range=0;
	Layer lay, glay;

	from_ndx = sf->insthdr[inst].bagNdx;
	to_ndx = sf->insthdr[inst+1].bagNdx;

	if (pr_lay->set[SF_velRange])
		pv_range = pr_lay->val[SF_velRange];
	if (pr_lay->set[SF_keyRange])
		pk_range = pr_lay->val[SF_keyRange];

	memcpy(&glay, pr_lay, sizeof(Layer));
	for (i = from_ndx; i < to_ndx; i++) {
		memset(&lay, 0, sizeof(Layer));
		parse_inst_layer(&lay, sf, i);
		sample = search_sample(&lay);
		if (sample < 0) /* global layer */
			append_layer(&glay, &lay, sf);
		else {
			/* append_layer(&lay, &glay, sf); */
			merge_layer(&lay, &glay);
			make_inst(rec, &lay, sf, preset, inst, inum, pk_range, pv_range, num_i, -1);
		}
	}
}

/* parse instrument generator layers */
static void parse_inst_layer(Layer *lay, SFInfo *sf, int idx)
{
	int i;
	for (i = sf->instbag[idx]; i < sf->instbag[idx+1]; i++)
		parse_gen(lay, sf->instgen + i);
}

/* search a sample id from instrument layers */
static int search_sample(Layer *lay)
{
	if (lay->set[SF_sampleId])
		return lay->val[SF_sampleId];
	else
		return -1;
}


/* append two layers; parameters are added to the original value */
static void append_layer(Layer *dst, Layer *src, SFInfo *sf)
{
	int i;
	for (i = 0; i < PARM_SIZE; i++) {
		if (src->set[i]) {
			if (sf->version == 1 && i == SF_instVol)
				dst->val[i] = (src->val[i] * 127) / 127;
			else if (i == SF_keyRange || i == SF_velRange) {
				/* high limit */
				if (HI_VAL(dst->val[i]) > HI_VAL(src->val[i]))
					SET_HI(dst->val[i], HI_VAL(src->val[i]));
				/* low limit */
				if (LO_VAL(dst->val[i]) < LO_VAL(src->val[i]))
					SET_LO(dst->val[i], LO_VAL(src->val[i]));
			} else
				dst->val[i] += src->val[i];
			dst->set[i] = 1;
		}
	}
}

static char kvec[2][128];
static void clear_kvec()
{
	int i;
	for (i=0; i<128; i++) {
		kvec[0][i] = kvec[1][i] = 0;
	}
}
static void new_kvec(int lr)
{
	int i;
	for (i=0; i<128; i++) {
		kvec[lr][i] = 0;
	}
}
static void union_kvec(int lr, int kr)
{
	int i;
	if (!kr || kr<0) return;
	for (i=0; i<128; i++)
	    if (i >= LO_VAL(kr) && i <= HI_VAL(kr)) kvec[lr][i] = 1;

}
static int intersect_kvec(int lr, int kr)
{
	int i;
	if (!kr || kr<0) return 0;
	for (i=0; i<128; i++)
	    if (i >= LO_VAL(kr) && i <= HI_VAL(kr) && kvec[lr][i]) return 1;
	return 0;
}
static int subset_kvec(int lr, int kr)
{
	int i;
	if (!kr || kr<0) return 0;
	for (i=0; i<128; i++)
	    if (i >= LO_VAL(kr) && i <= HI_VAL(kr) && !kvec[lr][i]) return 0;
	return 1;
}

static char vvec[2][128];

static void clear_vvec()
{
	int i;
	for (i=0; i<128; i++) {
		vvec[0][i] = vvec[1][i] = 0;
	}
}
static void union_vvec(int lr, int vr)
{
	int i;
	if (!vr || vr<0) return;
	for (i=0; i<128; i++)
	    if (i >= LO_VAL(vr) && i <= HI_VAL(vr)) vvec[lr][i] = 1;

}
static int intersect_vvec(int lr, int vr)
{
	int i;
	if (!vr || vr<0) return 0;
	for (i=0; i<128; i++)
	    if (i >= LO_VAL(vr) && i <= HI_VAL(vr) && vvec[lr][i]) return 1;
	return 0;
}

static char rvec[2][128];

static void clear_rvec()
{
	int i;
	for (i=0; i<128; i++) {
		rvec[0][i] = rvec[1][i] = 0;
	}
}
static void union_rvec(int lr, int kr)
{
	int i;
	if (!kr || kr<0) return;
	for (i=0; i<128; i++)
	    if (i >= LO_VAL(kr) && i <= HI_VAL(kr)) rvec[lr][i] = 1;
}
static int subset_rvec(int lr, int kr)
{
	int i;
	if (!kr || kr<0) return 0;
	for (i=0; i<128; i++)
	    if (i >= LO_VAL(kr) && i <= HI_VAL(kr) && !rvec[lr][i]) return 0;
	return 1;
}
static int intersect_rvec(int lr, int kr)
{
	int i;
	if (!kr || kr<0) return 0;
	for (i=0; i<128; i++)
	    if (i >= LO_VAL(kr) && i <= HI_VAL(kr) && rvec[lr][i]) return 1;
	return 0;
}
/* convert layer info to timidity instrument strucutre */
static void make_inst(SFInsts *rec, Layer *lay, SFInfo *sf, int pr_idx, int in_idx, int inum,
	uint16 pk_range, uint16 pv_range, int num_i, int program)
{
	int banknum = sf->presethdr[pr_idx].bank;
	int preset = sf->presethdr[pr_idx].preset;
	int sub_banknum = sf->presethdr[pr_idx].sub_bank;
	int sub_preset = sf->presethdr[pr_idx].sub_preset;
	int keynote, truebank, note_to_use, velrange;
	int strip_loop = 0, strip_envelope = 0,
		strip_tail = 0, panning = 0, cfg_tuning = 0;
#ifndef ADAGIO
	ToneBank *bank=0;
	const char **namep;
#endif
	InstList *ip;
	tsampleinfo *sample;
	SampleList *sp;
#ifdef DO_LINKED_WAVES
	int linked_wave;
#endif
	int sampleFlags, stereo_chan = 0, keyrange, x_chan = 0;
	static int lastpreset = -1;
	static int lastbanknum = -1;
	static int lastvelrange = -1;
	static int lastrange = -1;
	static int last_chan = -1;
	static int lastinum = -1;
	static int laststereo_chan = 0;
	static int lastpk_range = 0;

	if (pk_range) velrange = 0;
	else if (pv_range) velrange = pv_range;
	else if (lay->set[SF_velRange]) velrange = lay->val[SF_velRange];
	else velrange = 0;

#ifdef GREGSTEST
if (velrange)
fprintf(stderr,"bank %d, preset %d, inum %d, lo %d, hi %d\n", banknum, preset, inum,
LO_VAL(velrange), HI_VAL(velrange));
#endif

	sample = &sf->sampleinfo[lay->val[SF_sampleId]];
#ifdef DO_LINKED_WAVES
	linked_wave = sample->samplelink;
	if (linked_wave && linked_wave < sfinfo.nrinfos) {
		if (inum == -1) sample = &sf->sampleinfo[linked_wave - 1];
		else make_inst(rec, lay, sf, pr_idx, in_idx, -1, pk_range, velrange, num_i, -1);
	}
#endif
	if (sample->sampletype & 0x8000) /* is ROM sample? */
		return;

	if (lay->set[SF_keyRange]) keyrange = lay->val[SF_keyRange];
	else keyrange = 0;

	/* Were we called to do later notes of a percussion range? */
	if (program >= 0) keyrange = (program << 8) | program;
	else if (banknum == 128 && LO_VAL(keyrange) < HI_VAL(keyrange)) {
		int drumkey = LO_VAL(keyrange);
		int lastdkey = HI_VAL(keyrange);
		keyrange = (drumkey << 8) | drumkey;
		drumkey++;
		/* Do later notes of percussion key range. */
		for ( ; drumkey <= lastdkey; drumkey++)
		  make_inst(rec, lay, sf, pr_idx, in_idx, inum, pk_range, pv_range, num_i, drumkey);
	}
	/* set bank/preset name */
	if (banknum == 128) {
		truebank = sub_preset;
		if (program == -1) program = keynote = LO_VAL(keyrange);
		else keynote = program;
#ifndef ADAGIO
		bank = drumset[truebank];
#endif
	} else {
		keynote = -1;
		truebank = sub_banknum;
		program = preset;
#ifndef ADAGIO
		bank = tonebank[truebank];
#endif
	}

#ifdef GREGSTEST
fprintf(stderr,"Adding keynote #%d preset %d to %s (%d), bank %d (=? %d).\n", keynote, preset,
 getname(sf->insthdr[in_idx].name), in_idx, banknum, truebank);
#endif


#ifndef ADAGIO
	if (!bank) return;
	namep = &bank->tone[program].name;
#ifdef GREGSTEST
	if (*namep) fprintf(stderr,"cfg name is %s\n", *namep);
	else printf("NO CFG NAME!\n");
#endif
	/* if not declared, we don't load it */
	if (*namep == 0) return;

	/* could be a GUS patch */
	if (bank->tone[program].font_type != FONT_SBK) return;
	/* note we're loading this preset from a certain soundfont */
	if (bank->tone[program].sf_ix == -1)
		bank->tone[program].sf_ix = current_sf_index;
	/* has it been loaded already from a different soundfont? */
	else if (bank->tone[program].sf_ix != current_sf_index) return;

	panning = bank->tone[program].pan;
	strip_loop = bank->tone[program].strip_loop;
	strip_envelope = bank->tone[program].strip_envelope;
	strip_tail = bank->tone[program].strip_tail;
	note_to_use = bank->tone[program].note;
	cfg_tuning = bank->tone[program].tuning;
	/*if (!strip_envelope) strip_envelope = (banknum == 128);*/
#endif

	/* search current instrument list */
	for (ip = rec->instlist; ip; ip = ip->next) {
		if (ip->bank == sub_banknum && ip->preset == sub_preset &&
		    (keynote < 0 || keynote == ip->keynote))
			break;
	}
	/* don't append sample when instrument completely specified */
	if (ip && ip->already_loaded) return;

	/* search current instrument list */
	for (ip = rec->instlist; ip; ip = ip->next) {
		if (ip->bank == sub_banknum && ip->preset == sub_preset &&
		    ip->velrange == velrange &&
		    (keynote < 0 || keynote == ip->keynote))
			break;
	}

/* This doesn't work for Industry Standard G. Piano, which is mono but
   pans to the right for higher notes.
	if (!lay->set[SF_panEffectsSend] || lay->val[SF_panEffectsSend] <= 0)
		stereo_chan = 0;
	else	stereo_chan = 1;
*/

/* earlier
	if (lay->set[SF_keyRange]) keyrange = lay->val[SF_keyRange];
	else keyrange = 0;
*/
	if (lastbanknum != banknum || lastpreset != preset) {
		lastbanknum = banknum;
		lastpreset = preset;
		lastvelrange = velrange;
		lastrange = -1;
		clear_rvec();
		clear_vvec();
		clear_kvec();
	}
	if (lastvelrange != velrange) {
		lastvelrange = velrange;
		clear_rvec();
	}
	if (inum != lastinum) {
		lastrange = -1;
		clear_rvec();
		lastinum = inum;
	}

	if (banknum < 128 && pv_range && !pk_range) {
		if (pv_range == lastrange) x_chan = last_chan;
		else if (!intersect_vvec(0, pv_range)) x_chan = 0;
		else if (!intersect_vvec(1, pv_range)) x_chan = 1;
		else {
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
		  "sndfont: invalid velocity range in bank %d preset %d: low %d, high %d",
			banknum, preset, LO_VAL(pv_range), HI_VAL(pv_range));
		return;
		}
		if (pv_range != lastrange) {
		#if 0
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "sndfont: x_chan %d vel range in bank %d preset %d: low %d, high %d",
			x_chan, banknum, preset, LO_VAL(pv_range), HI_VAL(pv_range));
		#endif
			last_chan = x_chan;
			clear_rvec();
			union_vvec(x_chan, pv_range);
			lastrange = pv_range;
		}
		#if 0
		else if (intersect_rvec(x_chan, keyrange))
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "sndfont: invalid layered key range in bank %d preset %d: low %d, high %d",
			banknum, preset, LO_VAL(keyrange), HI_VAL(keyrange));
		#endif
	}

	#if 0
	if (x_chan != 0)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "sndfont: ignoring 2nd layered key range in bank %d preset %d: low %d, high %d",
			banknum, preset, LO_VAL(keyrange), HI_VAL(keyrange));
	#endif
	/* duplicated key ranges at preset level would require more than two patches per note */
	if (x_chan != 0) return;

	if (banknum == 128 && num_i > 1 && pk_range) {
		/*
		 * keyranges at the preset level are to fill in for missing
		 * patches, so if we have a patch, skip the fill in
		 */
		if (ip) return;
		/*
		 * the preset keyrange says which fill ins are to be taken from the
		 * current instrument
		 */
		if (LO_VAL(keyrange) < LO_VAL(pk_range) || HI_VAL(keyrange) > HI_VAL(pk_range))
			return;
	}

	/* melodic preset with multiple instruments */
	if (banknum < 128 && num_i > 1 && (!pv_range || pk_range)) {
		/* if no preset keyrange to tell us, put just first instrument at left */
		if (!pk_range) {
		    if (inum == 0) stereo_chan = 0;
		    else stereo_chan = 1;
		}
		else if (pk_range == lastpk_range) {
		    stereo_chan = laststereo_chan;
		    if (!subset_kvec(stereo_chan, keyrange)) return;
		}
		else {
		    if (!intersect_kvec(0, pk_range)) stereo_chan = 0;
		    else {
			stereo_chan = 1;
			new_kvec(stereo_chan);
		    }
		    #if 0
		    else {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
		  	"sndfont: invalid preset key range in bank %d preset %d: low %d, high %d",
				banknum, preset, LO_VAL(pk_range), HI_VAL(pk_range));
			return;
		    }
		    #endif
		    laststereo_chan = stereo_chan;
		    lastpk_range = pk_range;
		    union_kvec(stereo_chan, pk_range);
		    if (!subset_kvec(stereo_chan, keyrange)) return;
		}
	}
	else if (!intersect_rvec(0, keyrange)) stereo_chan = 0;
	else if (!intersect_rvec(1, keyrange)) stereo_chan = 1;
	else if (!subset_rvec(0, keyrange)) stereo_chan = 0;
	else if (!subset_rvec(1, keyrange)) stereo_chan = 1;
	else {
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
		  "sndfont: invalid key range in bank %d preset %d: low %d, high %d",
			banknum, preset, LO_VAL(keyrange), HI_VAL(keyrange));
		return;
	}

	union_rvec(stereo_chan, keyrange);

#if 0
if (pk_range)
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	  "adding chan %d in bank %d preset %d: low %d, high %d (preset low %d high %d; vel lo %d hi %d)",
		stereo_chan, banknum, preset, LO_VAL(keyrange), HI_VAL(keyrange),
		LO_VAL(pk_range), HI_VAL(pk_range),
		LO_VAL(velrange), HI_VAL(velrange));
#endif

	if (ip == NULL) {
		ip = (InstList*)safe_malloc(sizeof(InstList));
		ip->bank = sub_banknum;
		ip->preset = sub_preset;
		ip->keynote = keynote;
		ip->inum = inum;
		ip->velrange = velrange;
		ip->already_loaded = 0;
		ip->samples = 0;
		ip->rsamples = 0;
		ip->slist = NULL;
		ip->rslist = NULL;
		ip->fd = rec->fd;
		ip->fname = rec->fname;
		ip->next = rec->instlist;
		rec->instlist = ip;
	}

	/* add a sample */
	sp = (SampleList*)safe_malloc(sizeof(SampleList));
/* fix to check sample type for left vs. right? */
	if (stereo_chan == 0) {
	/* if (inum == ip->inum) { */
		sp->next = ip->slist;
		ip->slist = sp;
		ip->samples++;
	}
	else {
		sp->next = ip->rslist;
		ip->rslist = sp;
		ip->rsamples++;
	}


	/* set sample position */
	sp->startsample = ((short)lay->val[SF_startAddrsHi] * 32768)
		+ (short)lay->val[SF_startAddrs]
		+ sample->startsample;
	sp->endsample = ((short)lay->val[SF_endAddrsHi] * 32768)
		+ (short)lay->val[SF_endAddrs]
		+ sample->endsample - sp->startsample;

	if (sp->endsample > (1 << (32-FRACTION_BITS)))
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "sndfont: patch size %ld greater than max (%ld)", sp->endsample, 1<<(32-FRACTION_BITS));

	/* set loop position */
	sp->v.loop_start = ((short)lay->val[SF_startloopAddrsHi] * 32768)
		+ (short)lay->val[SF_startloopAddrs]
		+ sample->startloop - sp->startsample;
	sp->v.loop_end = ((short)lay->val[SF_endloopAddrsHi] * 32768)
		+ (short)lay->val[SF_endloopAddrs]
		+ sample->endloop - sp->startsample;
	sp->v.data_length = sp->endsample;
#if 0
/* debug loop point calculation */
{
	int32 xloop_start = sample->startloop - sp->startsample;
	int32 xloop_end = sample->endloop - sp->startsample;
	int yloop_start = (lay->val[SF_startloopAddrsHi] << 15) + lay->val[SF_startloopAddrs];
	int yloop_end = (lay->val[SF_endloopAddrsHi] << 15) + lay->val[SF_endloopAddrs];
	xloop_start += yloop_start;
	xloop_end += yloop_end;
	if (xloop_start > xloop_end) {
	fprintf(stderr,"start %ld > end %ld\n", xloop_start, xloop_end);
	fprintf(stderr,"\tstart %ld = orig %lu + yloop_start %d\n", xloop_start,
		sp->v.loop_start, yloop_start);
	fprintf(stderr,"\t\tyloop_start 0x%x = high 0x%x + low 0x%x\n", yloop_start,
		(lay->val[SF_startloopAddrsHi] << 15), lay->val[SF_startloopAddrs]);
	fprintf(stderr,"\t\tend %ld = orig %lu + yloop_end %d\n", xloop_end,
		sp->v.loop_end, yloop_end);
	fprintf(stderr,"\tlen %lu, startsample %lu sample startloop %lu endsample %lu\n",
		sample->endsample - sample->startsample,
		sample->startsample, sample->startloop, sample->endsample);
	}
	if (xloop_end > sp->v.data_length) {
	fprintf(stderr,"end %ld > length %lu\n", xloop_end, sp->v.data_length);
	fprintf(stderr,"\tend %ld = orig %lu + yloop_end %d\n", xloop_end,
		sp->v.loop_end, yloop_end);
	fprintf(stderr,"\t\tyloop_end 0x%x = high 0x%x + low 0x%x\n", yloop_end,
		(lay->val[SF_endloopAddrsHi] << 15), lay->val[SF_endloopAddrs]);
	fprintf(stderr,"\tlen %lu, startsample %lu sample endloop %lu endsample %lu\n",
		sample->endsample - sample->startsample,
		sample->startsample, sample->endloop, sample->endsample);
	fprintf(stderr,"\t\tstart offset = high 0x%x + low 0x%x\n",
		(lay->val[SF_startAddrsHi] << 15), lay->val[SF_startAddrs]);
	fprintf(stderr,"\t\tend offset = high 0x%x + low 0x%x\n",
		(lay->val[SF_endAddrsHi] << 15), lay->val[SF_endAddrs]);
	}
}
#endif

#if 0
	if (sp->v.loop_start < 0) {
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Negative loop pointer: removing loop");
		strip_loop = 1;
	}
#endif
	if (sp->v.loop_start > sp->v.loop_end) {
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Illegal loop position: removing loop");
		strip_loop = 1;
	}
	if (sp->v.loop_end > sp->v.data_length) {
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Illegal loop end or data size: adjusting loop");
		if (sp->v.loop_end - sp->v.data_length > 4)
			sp->v.loop_end = sp->v.data_length - 4;
		else sp->v.loop_end = sp->v.data_length;
	}

/* debug */
#if 0
if (strip_loop == 1) {
	fprintf(stderr, "orig start sample %lu, endsample %lu\n", sample->startsample, sample->endsample);
	fprintf(stderr, "SF_startAddr %d, SF_endAddr %d\n",
		((short)lay->val[SF_startAddrsHi] * 32768) + (short)lay->val[SF_startAddrs],
		((short)lay->val[SF_endAddrsHi] * 32768) + (short)lay->val[SF_endAddrs] );
	fprintf(stderr, "start sample %lu, length %lu\n", sp->startsample, sp->endsample);
	fprintf(stderr, "orig loop start %lu, loop end %lu\n", sample->startloop, sample->endloop);
	fprintf(stderr, "SF_startloopAddr %d, SF_endloopAddr %d\n",
		((short)lay->val[SF_startloopAddrsHi] * 32768) + (short)lay->val[SF_startloopAddrs],
		((short)lay->val[SF_endloopAddrsHi] * 32768) + (short)lay->val[SF_endloopAddrs] );
	fprintf(stderr, "loop start %lu, loop end %lu\n", sp->v.loop_start, sp->v.loop_end);
}
#endif


	sp->v.sample_rate = sample->samplerate;
	if (lay->set[SF_keyRange]) {
		sp->v.low_freq = freq_table[LO_VAL(lay->val[SF_keyRange])];
		sp->v.high_freq = freq_table[HI_VAL(lay->val[SF_keyRange])];
	} else {
		sp->v.low_freq = freq_table[0];
		sp->v.high_freq = freq_table[127];
	}
	if (lay->set[SF_keyExclusiveClass]) sp->v.exclusiveClass = lay->val[SF_keyExclusiveClass];
	else sp->v.exclusiveClass = 0;

	/* scale tuning: 0  - 100 */
	sp->v.scale_tuning = 100;
	if (lay->set[SF_scaleTuning]) {
		if (sf->version == 1)
			sp->v.scale_tuning = lay->val[SF_scaleTuning] ? 50 : 100;
		else
			sp->v.scale_tuning = lay->val[SF_scaleTuning];
	}
/** --gl **/
#if 0
	sp->v.freq_scale = 1024;
	sp->v.freq_center = 60;
#endif
	if (sp->v.scale_tuning == 100) sp->v.freq_scale = 1024;
	else sp->v.freq_scale = (sp->v.scale_tuning * 1024) / 100;

	/* sp->v.freq_center = 60; set in calc_root_pitch */
	sp->v.attenuation = 0;
/**  **/

	/* root pitch */
	sp->v.root_freq = calc_root_pitch(lay, sf, sp, cfg_tuning);
/* *
if (banknum < 128 && (preset == 11 || preset == 42))
fprintf(stderr, "preset %d, root_freq %ld (scale tune %d, freq scale %d, center %d)\n", preset, sp->v.root_freq,
sp->v.scale_tuning, sp->v.freq_scale, sp->v.freq_center);
* */
	/* volume envelope & total volume */
	sp->v.volume = 1.0; /* was calc_volume(lay,sf) */

	if (lay->set[SF_sampleFlags]) sampleFlags = lay->val[SF_sampleFlags];
	else sampleFlags = 0;

	sp->v.modes = MODES_16BIT | MODES_ENVELOPE;


	if (sampleFlags == 3) sp->v.modes |= MODES_FAST_RELEASE;

	/* arbitrary adjustments (look at sustain of vol envelope? ) */
	if (sampleFlags && lay->val[SF_sustainEnv2] == 0) sampleFlags = 3;
	else if (sampleFlags && lay->val[SF_sustainEnv2] >= 1000) sampleFlags = 1;
	else if (banknum != 128) {
		/* organs, accordians */
		if (program >= 16 && program <= 23) sampleFlags = 3;
		/* strings */
		else if (program >= 40 && program <= 44) sampleFlags = 3;
		/* strings, voice */
		else if (program >= 48 && program <= 54) sampleFlags = 3;
		/* horns, woodwinds */
		else if (program >= 56 && program <= 79) sampleFlags = 3;
		/* lead, pad, fx */
		else if (program >= 80 && program <=103) sampleFlags = 3;
		/* bagpipe, fiddle, shanai */
		else if (program >=109 && program <=111) sampleFlags = 3;
		/* breath noise, ... telephone, helicopter */
		else if (program >=121 && program <=125) sampleFlags = 3;
		/* applause */
		else if (program ==126) sampleFlags = 3;
	}


	if (sampleFlags == 2) sampleFlags = 0;

	if (sampleFlags == 1 || sampleFlags == 3)
		sp->v.modes |= MODES_LOOPING;
	if (sampleFlags == 3)
		sp->v.modes |= MODES_SUSTAIN;


	convert_volume_envelope(lay, sf, sp, banknum, program);
	convert_modulation_envelope(lay, sf, sp, banknum, program);

	if (strip_tail == 1) sp->v.data_length = sp->v.loop_end + 1;

      /* Strip any loops and envelopes we're permitted to */
      if ((strip_loop==1) && 
	  (sp->v.modes & (MODES_SUSTAIN | MODES_LOOPING | 
			MODES_PINGPONG | MODES_REVERSE)))
	{
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Removing loop and/or sustain");
	  sp->v.modes &=~(MODES_SUSTAIN | MODES_LOOPING | 
			MODES_PINGPONG | MODES_REVERSE);
	}

      if (strip_envelope==1)
	{
	  if (sp->v.modes & MODES_ENVELOPE)
	    ctl->cmsg(CMSG_INFO, VERB_DEBUG, " - Removing envelope");
	  sp->v.modes &= ~MODES_ENVELOPE;
	}

	/* if (banknum == 128 && !(sp->v.modes&(MODES_LOOPING|MODES_SUSTAIN))) sp->v.modes |= MODES_FAST_RELEASE; */


	/* panning position: 0 to 127 */
	if (panning != -1) sp->v.panning=(uint8)(panning & 0x7F);
	else if (lay->set[SF_panEffectsSend]) {
#ifdef tplussbk
		    int val;
		    /* panning position: 0 to 127 */
		    val = (int)tbl->val[SF_panEffectsSend];
		    if(val <= -500)
			sp->v.panning = 0;
		    else if(val >= 500)
			sp->v.panning = 127;
		    else
			sp->v.panning = (int8)((val + 500) * 127 / 1000);
#else
		if (sf->version == 1)
			sp->v.panning = (int8)lay->val[SF_panEffectsSend];
		else
			sp->v.panning = (int8)(((int)lay->val[SF_panEffectsSend] + 500) * 127 / 1000);
#endif
	}
	else sp->v.panning = 64;

	if (lay->set[SF_chorusEffectsSend]) {
		if (sf->version == 1)
			sp->v.chorusdepth = (int8)lay->val[SF_chorusEffectsSend];
		else
			sp->v.chorusdepth = (int8)((int)lay->val[SF_chorusEffectsSend] * 127 / 1000);
	}
	else sp->v.chorusdepth = 0;

	if (lay->set[SF_reverbEffectsSend]) {
		if (sf->version == 1)
			sp->v.reverberation = (int8)lay->val[SF_reverbEffectsSend];
		else
			sp->v.reverberation = (int8)((int)lay->val[SF_reverbEffectsSend] * 127 / 1000);
	}
	else sp->v.reverberation = 0;

	/* tremolo & vibrato */
	sp->v.tremolo_sweep_increment = 0;
	sp->v.tremolo_phase_increment = 0;
	sp->v.tremolo_depth = 0;
#ifndef SF_SUPPRESS_TREMOLO
	convert_tremolo(lay, sf, sp);
#endif
	sp->v.lfo_sweep_increment = 0;
	sp->v.lfo_phase_increment = 0;
	sp->v.modLfoToFilterFc = 0;
#ifndef SF_SUPPRESS_TREMOLO
	convert_lfo(lay, sf, sp);
#endif
	sp->v.vibrato_sweep_increment = 0;
	sp->v.vibrato_control_ratio = 0;
	sp->v.vibrato_depth = 0;
	sp->v.vibrato_delay = 0;
#ifndef SF_SUPPRESS_VIBRATO
	convert_vibrato(lay, sf, sp);
#endif

	/* set note to use for drum voices */
	if (note_to_use!=-1)
		sp->v.note_to_use = (uint8)note_to_use;
	else if (banknum == 128)
		sp->v.note_to_use = keynote;
	else
		sp->v.note_to_use = 0;

	/* convert to fractional samples */
	sp->v.data_length <<= FRACTION_BITS;
	sp->v.loop_start <<= FRACTION_BITS;
	sp->v.loop_end <<= FRACTION_BITS;

	/* point to the file position */
	sp->startsample = sp->startsample * 2 + sf->samplepos;
	sp->endsample *= 2;

/*printf("%s: bank %d, preset %d\n", *namep, banknum, preset);*/
	/* set cutoff frequency */
	/*if (lay->set[SF_initialFilterFc] || lay->set[SF_env1ToFilterFc])*/
		calc_cutoff(lay, sf, sp);
	/*else sp->cutoff_freq = 0;*/
/*
if (sp->cutoff_freq)
printf("bank %d, program %d, f= %d (%d)\n", banknum, program, sp->cutoff_freq, lay->val[SF_initialFilterFc]);
*/
	if (lay->set[SF_initialFilterQ])
		calc_filterQ(lay, sf, sp);
	sp->v.cutoff_freq = sp->cutoff_freq;
	sp->v.resonance = sp->resonance;
}

/* calculate root pitch */
static int32 calc_root_pitch(Layer *lay, SFInfo *sf, SampleList *sp, int32 cfg_tuning)
{
	int32 root, tune;
	tsampleinfo *sample;

	sample = &sf->sampleinfo[lay->val[SF_sampleId]];

	root = sample->originalPitch;
	/* sp->v.freq_center = root; */

	tune = sample->pitchCorrection;
	if (sf->version == 1) {
		if (lay->set[SF_samplePitch]) {
			root = lay->val[SF_samplePitch] / 100;
			tune = -lay->val[SF_samplePitch] % 100;
			if (tune <= -50) {
				root++;
				tune = 100 + tune;
			}
			if (sp->v.scale_tuning == 50)
				tune /= 2;
		}
		/* orverride root key */
		if (lay->set[SF_rootKey])
			root += lay->val[SF_rootKey] - 60;
		/* tuning */
		tune += lay->val[SF_coarseTune] * sp->v.scale_tuning +
			lay->val[SF_fineTune] * sp->v.scale_tuning / 100;
	} else {
		/* override root key */
		if (lay->set[SF_rootKey])
			root = lay->val[SF_rootKey];
		/* tuning */
#ifdef tplussbk
		tune += lay->val[SF_coarseTune] * sp->v.scale_tuning
			+ (int)lay->val[SF_fineTune] * (int)sp->v.scale_tuning / 100;
#else
		tune += lay->val[SF_coarseTune] * 100
			+ lay->val[SF_fineTune];
#endif
	}

	tune += cfg_tuning;

	/* it's too high.. */
	if (lay->set[SF_keyRange] &&
	    root >= HI_VAL(lay->val[SF_keyRange]) + 60)
		root -= 60;

	while (tune <= -100) {
		root++;
		tune += 100;
	}
	while (tune > 0) {
		root--;
		tune -= 100;
	}

    if (root > 0) sp->v.freq_center = root;
    else sp->v.freq_center = 60;
/* have to adjust for rate ... */
	/* sp->v.sample_rate */
	/* play_mode->rate */
#ifdef tplussbkuse

    /* -100 < tune <= 0 */
    tune = (-tune * 256) / 100;
    /* 256 > tune >= 0 */
    /* 1.059 >= bend_fine[tune] >= 1.0 */

    if(root > 127)
	return (int32)((FLOAT_T)freq_table[127] *
				  bend_coarse[root - 127] * bend_fine[tune]);
				  
    else if(root < 0)
	return (int32)((FLOAT_T)freq_table[0] /
				  bend_coarse[-root] * bend_fine[tune]);
    else
	return (int32)((FLOAT_T)freq_table[root] * bend_fine[tune]);

#else
	return (int32)((double)freq_table[root] * bend_fine[(-tune*255)/100]);
#endif
}


/*#define EXAMINE_SOME_ENVELOPES*/
/*----------------------------------------------------------------
 * convert volume envelope
 *----------------------------------------------------------------*/

static void convert_volume_envelope(Layer *lay, SFInfo *sf, SampleList *sp, int banknum, int preset)
{
	uint32 sustain = calc_sustain(lay, sf, banknum, preset);
	double delay = to_msec(lay, sf, SF_delayEnv2);
	double attack = to_msec(lay, sf, SF_attackEnv2);
	double hold = to_msec(lay, sf, SF_holdEnv2);
	double decay = to_msec(lay, sf, SF_decayEnv2);
	double release = to_msec(lay, sf, SF_releaseEnv2);
	FLOAT_T vol = calc_volume(lay,sf);
	uint32 volume;
	/* int milli = play_mode->rate/1000; */
#ifdef EXAMINE_SOME_ENVELOPES
	static int no_shown = 0;
no_shown = banknum==128 && (preset == 57 || preset == 56);
if (no_shown) {
printf("PRESET %d\n",preset);
	printf("vol %f sustainEnv2 %d delayEnv2 %d attackEnv2 %d holdEnv2 %d decayEnv2 %d releaseEnv2 %d\n", vol,
	lay->val[SF_sustainEnv2],
	lay->val[SF_delayEnv2],
	lay->val[SF_attackEnv2],
	lay->val[SF_holdEnv2],
	lay->val[SF_decayEnv2],
	lay->val[SF_releaseEnv2] );
	printf("attack %f hold %f sustain %ld decay %f release %f delay %f\n", attack, hold, sustain,
		decay, release, delay);
}
#endif

	if (vol > 255.0) volume = 255;
	else if (vol < 1.0) volume = 0;
	else volume = (uint32)vol;

	if (!lay->set[SF_releaseEnv2] && banknum < 128) release = 400;
	if (!lay->set[SF_decayEnv2] && banknum < 128) decay = 400;

#define HOLD_EXCURSION 1
#define ENV_BOTTOM 0

/* ramp from 0 to <volume> in <attack> msecs */
	sp->v.envelope_offset[ATTACK] = to_offset(volume);
	sp->v.envelope_rate[ATTACK] = calc_rate(volume, attack);

/* ramp down HOLD_EXCURSION in <hold> msecs */
	sp->v.envelope_offset[HOLD] = to_offset(volume-HOLD_EXCURSION);
	sp->v.envelope_rate[HOLD] = calc_rate(HOLD_EXCURSION, hold);

/* ramp down by <sustain> in <decay> msecs */
	if(sustain <= ENV_BOTTOM) sustain = ENV_BOTTOM;
	if(sustain > volume - HOLD_EXCURSION) sustain = volume - HOLD_EXCURSION;

	sp->v.envelope_offset[DECAY] = to_offset(sustain);
	sp->v.envelope_rate[DECAY] = calc_rate(volume - HOLD_EXCURSION - sustain, decay);
	if (fast_decay) sp->v.envelope_rate[DECAY] *= 2;

/* ramp to ENV_BOTTOM in ?? msec */
	sp->v.envelope_offset[RELEASE] = to_offset(ENV_BOTTOM);
	sp->v.envelope_rate[RELEASE] = calc_rate(255, release);
	if (fast_decay) sp->v.envelope_rate[RELEASE] *= 2;

	sp->v.envelope_offset[RELEASEB] = to_offset(ENV_BOTTOM);
	sp->v.envelope_rate[RELEASEB] = to_offset(200);
	sp->v.envelope_offset[RELEASEC] = to_offset(ENV_BOTTOM);
	sp->v.envelope_rate[RELEASEC] = to_offset(200);

	/* pc400.sf2 has bad delay times for soprano sax */
	if (delay > 5) delay = 5;

	sp->v.envelope_rate[DELAY] = (int32)( (delay*play_mode->rate) / 1000 );

	sp->v.modes |= MODES_ENVELOPE;
#ifdef EXAMINE_SOME_ENVELOPES
if (no_shown) {
	/*no_shown++;*/
	printf(" attack(0): off %ld  rate %ld\n",
		sp->v.envelope_offset[0] >>(7+15), sp->v.envelope_rate[0] >>(7+15));
	printf("   hold(1): off %ld  rate %ld\n",
		sp->v.envelope_offset[1] >>(7+15), sp->v.envelope_rate[1] >>(7+15));
	printf("sustain(2): off %ld  rate %ld\n",
		sp->v.envelope_offset[2] >>(7+15), sp->v.envelope_rate[2] >>(7+15));
	printf("release(3): off %ld  rate %ld\n",
		sp->v.envelope_offset[3] >>(7+15), sp->v.envelope_rate[3] >>(7+15));
	printf("  decay(4): off %ld  rate %ld\n",
		sp->v.envelope_offset[4] >>(7+15), sp->v.envelope_rate[4] >>(7+15));
	printf("    die(5): off %ld  rate %ld\n",
		sp->v.envelope_offset[5] >>(7+15), sp->v.envelope_rate[5] >>(7+15));
	printf("  delay(6): off %ld  rate %ld\n",
		sp->v.envelope_offset[6], sp->v.envelope_rate[6]);
}
#endif
}

static void convert_modulation_envelope(Layer *lay, SFInfo *sf, SampleList *sp, int banknum, int preset)
{
	uint32 sustain = calc_modulation_sustain(lay, sf, banknum, preset);
	double delay = to_msec(lay, sf, SF_delayEnv1);
	double attack = to_msec(lay, sf, SF_attackEnv1);
	double hold = to_msec(lay, sf, SF_holdEnv1);
	double decay = to_msec(lay, sf, SF_decayEnv1);
	double release = to_msec(lay, sf, SF_releaseEnv1);
	FLOAT_T vol = calc_volume(lay,sf);
	uint32 volume;
#ifdef EXAMINE_SOME_ENVELOPES
	static int no_shown = 0;
no_shown = banknum==0 && (preset == 11 || preset == 64);
if (no_shown) {
printf("PRESET %d\n",preset);
	printf("sustainEnv1 %d delayEnv1 %d attackEnv1 %d holdEnv1 %d decayEnv1 %d releaseEnv1 %d\n",
	lay->val[SF_sustainEnv1],
	lay->val[SF_delayEnv1],
	lay->val[SF_attackEnv1],
	lay->val[SF_holdEnv1],
	lay->val[SF_decayEnv1],
	lay->val[SF_releaseEnv1] );
	printf("attack %f hold %f sustain %ld decay %f release %f delay %f\n", attack, hold, sustain,
		decay, release, delay);
}
#endif

	if (vol > 255.0) volume = 255;
	else if (vol < 1.0) volume = 0;
	else volume = (uint32)vol;


/* ramp from 0 to <volume> in <attack> msecs */
	sp->v.modulation_offset[ATTACK] = to_offset(volume);
	sp->v.modulation_rate[ATTACK] = calc_rate(volume, attack);

/* ramp down HOLD_EXCURSION in <hold> msecs */
	sp->v.modulation_offset[HOLD] = to_offset(volume-HOLD_EXCURSION);
	sp->v.modulation_rate[HOLD] = calc_rate(HOLD_EXCURSION, hold);

/* ramp down by <sustain> in <decay> msecs */
	if(sustain <= ENV_BOTTOM) sustain = ENV_BOTTOM;
	if(sustain > volume - HOLD_EXCURSION) sustain = volume - HOLD_EXCURSION;

	sp->v.modulation_offset[DECAY] = to_offset(sustain);
	sp->v.modulation_rate[DECAY] = calc_rate(volume - HOLD_EXCURSION - sustain, decay);
	if (fast_decay) sp->v.modulation_rate[DECAY] *= 2;

/* ramp to ENV_BOTTOM in ?? msec */
	sp->v.modulation_offset[RELEASE] = to_offset(ENV_BOTTOM);
	sp->v.modulation_rate[RELEASE] = calc_rate(255, release);
	if (fast_decay) sp->v.modulation_rate[RELEASE] *= 2;

	sp->v.modulation_offset[RELEASEB] = to_offset(ENV_BOTTOM);
	sp->v.modulation_rate[RELEASEB] = to_offset(200);
	sp->v.modulation_offset[RELEASEC] = to_offset(ENV_BOTTOM);
	sp->v.modulation_rate[RELEASEC] = to_offset(200);

	if (delay > 5) delay = 5;

	sp->v.modulation_rate[DELAY] = (int32)( (delay*play_mode->rate) / 1000 );

#ifdef EXAMINE_SOME_ENVELOPES
if (no_shown) {
	/*no_shown++;*/
	printf(" attack(0): off %ld  rate %ld\n",
		sp->v.modulation_offset[0] >>(7+15), sp->v.modulation_rate[0] >>(7+15));
	printf("   hold(1): off %ld  rate %ld\n",
		sp->v.modulation_offset[1] >>(7+15), sp->v.modulation_rate[1] >>(7+15));
	printf("sustain(2): off %ld  rate %ld\n",
		sp->v.modulation_offset[2] >>(7+15), sp->v.modulation_rate[2] >>(7+15));
	printf("release(3): off %ld  rate %ld\n",
		sp->v.modulation_offset[3] >>(7+15), sp->v.modulation_rate[3] >>(7+15));
	printf("  decay(4): off %ld  rate %ld\n",
		sp->v.modulation_offset[4] >>(7+15), sp->v.modulation_rate[4] >>(7+15));
	printf("    die(5): off %ld  rate %ld\n",
		sp->v.modulation_offset[5] >>(7+15), sp->v.modulation_rate[5] >>(7+15));
	printf("  delay(6): off %ld  rate %ld\n",
		sp->v.modulation_offset[6], sp->v.modulation_rate[6]);
}
#endif
}

/* convert from 8bit value to fractional offset (15.15) */
static uint32 to_offset(uint32 offset)
{
	if (offset >255) return 255 << (7+15);
	return (uint32)offset << (7+15);
}

/* calculate ramp rate in fractional unit;
 * diff = 8bit, time = msec
 */
static uint32 calc_rate(uint32 diff, double msec)
{
    double rate;

    diff <<= (7+15);
    /* rate = ((double)diff / play_mode->rate) * control_ratio * 1000.0 / msec; */
    /** rate = ( (double)diff * 1000.0 ) / (msec * CONTROLS_PER_SECOND); **/
    /** rate = ( (double)diff * 1000.0 ) / CONTROLS_PER_SECOND; **/
    rate = ( (double)diff * 1000.0 ) / (msec * CONTROLS_PER_SECOND);
    /* rate *= 2;  ad hoc adjustment ?? */
    if (rate < 10.0) return 10;
    return (uint32)rate;

/*
 *	control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
 *	samples per control = (samples per sec) / (controls per sec)
 *	rate is amp-change per control
 *		diff per msec
 *		diff*1000 per sec
 *		(diff*1000 per sec) / (controls per sec) = amp change per control
 *			diff/(msec/1000) = (diff * 1000) / msec = amp change per sec
 *			((diff * 1000)/msec) / CONTROLS_PER_SECOND = amp change per control
 *			= (diff * 1000) / (msec * CONTROLS_PER_SECOND)
 */
}

#define TO_MSEC(tcents) (int32)(1000 * pow(2.0, (double)(tcents) / 1200.0))
#define TO_MHZ(abscents) (int32)(8176.0 * pow(2.0,(double)(abscents)/1200.0))
#define TO_HZ(abscents) (int32)(8.176 * pow(2.0,(double)(abscents)/1200.0))
/* #define TO_HZ(abscents) (int32)(8176 * pow(2.0,(double)(abscents)/12000.0)) */
#define TO_LINEAR(centibel) pow(10.0, -(double)(centibel)/200.0)
/* #define TO_VOLUME(centibel) (uint8)(255 * (1.0 - (centibel) / (1200.0 * log10(2.0)))); */
#define TO_VOLUME(centibel) (uint8)(255 * pow(10.0, -(double)(centibel)/200.0))

/* convert the value to milisecs */
static double to_msec(Layer *lay, SFInfo *sf, int index)
{
	int16 value;
	double msec;

	if (sf->version == 1) {
		if (! lay->set[index])
			return 1.0;  /* 6msec minimum */
		value = lay->val[index];
		return (double)value;
	}

	if (lay->set[index]) value = lay->val[index];
	else value = -12000;

	if (index == SF_attackEnv2 && value > 1000) value = -12000;

	msec = (double)(1000 * pow(2.0, (double)( value ) / 1200.0));
	/*if (msec > 1000.0) return 1.0;*/

	return msec;
}

#define CB_TO_VOLUME(centibel) (255 * (1.0 - ((double)(centibel)/100.0) / (1200.0 * log10(2.0)) ))
/* convert peak volume to linear volume (0-255) */
static FLOAT_T calc_volume(Layer *lay, SFInfo *sf)
{
	if (sf->version == 1)
		return (FLOAT_T)(lay->val[SF_instVol] * 2) / 255.0;
	else
		return CB_TO_VOLUME((double)lay->val[SF_instVol]);
}

/* convert sustain volume to linear volume */
static uint32 calc_sustain(Layer *lay, SFInfo *sf, int banknum, int preset)
{
	int32 level;
	if (!lay->set[SF_sustainEnv2])
		return 250;
	level = lay->val[SF_sustainEnv2];
	if (sf->version == 1) {
		if (level < 96)
			level = 1000 * (96 - level) / 96;
		else
			return 0;
	}
	return TO_VOLUME(level);
}
/* convert sustain volume to linear volume */
static uint32 calc_modulation_sustain(Layer *lay, SFInfo *sf, int banknum, int preset)
{
	int32 level;
	if (!lay->set[SF_sustainEnv1])
		return 250;
	level = lay->val[SF_sustainEnv1];
	if (sf->version == 1) {
		if (level < 96)
			level = 1000 * (96 - level) / 96;
		else
			return 0;
	}
	return TO_VOLUME(level);
}


#ifndef SF_SUPPRESS_TREMOLO
/*----------------------------------------------------------------
 * tremolo (LFO1) conversion
 *----------------------------------------------------------------*/

static void convert_tremolo(Layer *lay, SFInfo *sf, SampleList *sp)
{
	int32 level;
	uint32 freq;

	if (!lay->set[SF_lfo1ToVolume])
		return;

	level = lay->val[SF_lfo1ToVolume];
	if (!level) return;
#if 0
printf("(lev=%d", (int)level);
#endif

	if (level < 0) level = -level;

	if (sf->version == 1)
		level = (120 * level) / 64;  /* to centibel */
	/* else level = TO_VOLUME((double)level); */
	else level = 255 - (uint8)(255 * (1.0 - (level) / (1200.0 * log10(2.0))));

	sp->v.tremolo_depth = level;

	/* frequency in mHz */
	if (! lay->set[SF_freqLfo1]) {
		if (sf->version == 1)
			freq = TO_MHZ(-725);
		else
			freq = 8;
	} else {
		freq = lay->val[SF_freqLfo1];
#if 0
printf(" freq=%d)", (int)freq);
#endif
		if (freq > 0 && sf->version == 1)
			freq = (int)(3986.0 * log10((double)freq) - 7925.0);
		else freq = TO_HZ(freq);
	}

#if 0
printf(" depth=%d freq=%d\n", (int)level, (int)freq);
#endif
	if (freq < 1) freq = 1;
	freq *= 20;
	if (freq > 255) freq = 255;

	sp->v.tremolo_phase_increment = convert_tremolo_rate((uint8)freq);
	sp->v.tremolo_sweep_increment = convert_tremolo_sweep((uint8)(freq/5));
}


static void convert_lfo(Layer *lay, SFInfo *sf, SampleList *sp)
{
	int32 freq=0, level;

	if (sf->version == 1) return;

	if (!lay->set[SF_lfo1ToFilterFc] || !lay->set[SF_initialFilterFc])
		return;

	level = lay->val[SF_lfo1ToFilterFc];
	if (!level) return;
/* FIXME: delay not yet implemented */
#ifdef DEBUG_CONVERT_LFO
printf("(lev=%d", (int)level);
#endif
	sp->v.modLfoToFilterFc = pow(2.0, ((FLOAT_T)level/1200.0));

	/* frequency in mHz */
	if (lay->set[SF_freqLfo1]) freq = lay->val[SF_freqLfo1];

#ifdef DEBUG_CONVERT_LFO
printf(" freq=%d)", (int)freq);
#endif
	if (!freq) freq = 8;
	else freq = TO_HZ(freq);

#ifdef DEBUG_CONVERT_LFO
printf(" depth=%f freq=%d\n", sp->modLfoToFilterFc, (int)freq);
#endif
	if (freq < 1) freq = 1;
	freq *= 20;
	if (freq > 255) freq = 255;

	sp->v.lfo_phase_increment = convert_tremolo_rate((uint8)freq);
	sp->v.lfo_sweep_increment = convert_tremolo_sweep((uint8)(freq/5));
}
#endif


#ifndef SF_SUPPRESS_VIBRATO
/*----------------------------------------------------------------
 * vibrato (LFO2) conversion
 * (note: my changes to Takashi's code are unprincipled --gl)
 *----------------------------------------------------------------*/

static void convert_vibrato(Layer *lay, SFInfo *sf, SampleList *sp)
{
	int32 shift=0, freq=0, delay=0;

	if (lay->set[SF_lfo2ToPitch]) {
		shift = lay->set[SF_lfo2ToPitch];
		if (lay->set[SF_freqLfo2]) freq = lay->val[SF_freqLfo2];
#if 0
printf("modLfo=%d freq=%d",(int)shift, (int)freq);
#endif
		if (lay->set[SF_delayLfo2]) delay = (int32)to_msec(lay, sf, SF_delayLfo2);
	}
	else if (lay->set[SF_lfo1ToPitch]) {
		shift = lay->set[SF_lfo1ToPitch];
		if (lay->set[SF_freqLfo1]) freq = lay->val[SF_freqLfo1];
#if 0
printf("vibLfo=%d freq=%d",(int)shift, (int)freq);
#endif
		if (lay->set[SF_delayLfo1]) delay = (int32)to_msec(lay, sf, SF_delayLfo1);
	}
/*
	else if (lay->set[SF_freqLfo2]) {
		freq = lay->val[SF_freqLfo2];
		shift = 1;
	}
*/

	if (!shift) return;

	/* pitch shift in cents (= 1/100 semitone) */
	if (sf->version == 1)
		shift = (1200 * shift / 64 + 1) / 2;

	/* cents to linear; 400cents = 256 */
	/*sp->v.vibrato_depth = (int8)((int32)shift * 256 / 400);*/
	/** sp->v.vibrato_depth = shift * 400 / 64; **/

	sp->v.vibrato_depth = (int32)(pow(2.0, ((FLOAT_T)shift/1200.0)) * VIBRATO_RATE_TUNING);
	/* frequency in mHz */
	if (!freq) {
		if (sf->version == 1)
			freq = TO_HZ(-725);
		else
			freq = 8;
	} else {
		if (sf->version == 1)
			freq = (int32)(3986.0 * log10((double)freq) - 7925.0);
		else freq = TO_HZ(freq);
	}
	if (freq < 1) freq = 1;

	freq *= 20;
	if (freq > 255) freq = 255;

	sp->v.vibrato_control_ratio = convert_vibrato_rate((uint8)freq);

	/* convert mHz to control ratio */
#if 0
	sp->v.vibrato_control_ratio = freq *
		(VIBRATO_RATE_TUNING * play_mode->rate) /
		(2 * CONTROLS_PER_SECOND * VIBRATO_SAMPLE_INCREMENTS);
#endif

#if 0
printf(" f=%d depth=%d (shift %d)\n", (int)freq, (int)sp->v.vibrato_depth, (int)shift);
#endif
	/* sp->v.vibrato_sweep_increment = 74; */
	sp->v.vibrato_sweep_increment = convert_vibrato_sweep((uint8)(freq/5),
		sp->v.vibrato_control_ratio);

	sp->v.vibrato_delay = delay * control_ratio;
}
#endif


/* calculate cutoff/resonance frequency */
static void calc_cutoff(Layer *lay, SFInfo *sf, SampleList *sp)
{
	int16 val;

	if (! lay->set[SF_initialFilterFc]) {
		val = 13500;
	} else {
		val = lay->val[SF_initialFilterFc];
		if (sf->version == 1) {
			if (val == 127)
				val = 14400;
			else if (val > 0)
				val = 50 * val + 4366;
		}
	}
	if (lay->set[SF_env1ToFilterFc] && lay->set[SF_initialFilterFc]) {
		sp->v.modEnvToFilterFc = pow(2.0, ((FLOAT_T)lay->val[SF_env1ToFilterFc]/1200.0));
		/* sp->v.modEnvToFilterFc = pow(2.0, ((FLOAT_T)lay->val[SF_env1ToFilterFc]/12000.0)); */
/* printf("val %d -> %f\n", (int)lay->val[SF_env1ToFilterFc], sp->v.modEnvToFilterFc); */
	}
	else sp->v.modEnvToFilterFc = 0;

	if (lay->set[SF_env1ToPitch]) {
		sp->v.modEnvToPitch = pow(2.0, ((FLOAT_T)lay->val[SF_env1ToPitch]/1200.0));
/* printf("mE %d -> %f\n", (int)lay->val[SF_env1ToPitch], sp->v.modEnvToPitch);*/
	}
	else sp->v.modEnvToPitch = 0;

	sp->cutoff_freq = TO_HZ(val);

	if (lay->set[SF_autoHoldEnv1]) sp->v.keyToModEnvHold=lay->val[SF_autoHoldEnv1];
	else sp->v.keyToModEnvHold=0;
	if (lay->set[SF_autoDecayEnv1]) sp->v.keyToModEnvDecay=lay->val[SF_autoDecayEnv1];
	else sp->v.keyToModEnvDecay=0;
	if (lay->set[SF_autoHoldEnv2]) sp->v.keyToVolEnvHold=lay->val[SF_autoHoldEnv2];
	else sp->v.keyToVolEnvHold=0;
	if (lay->set[SF_autoDecayEnv2]) sp->v.keyToVolEnvDecay=lay->val[SF_autoDecayEnv2];
	else sp->v.keyToVolEnvDecay=0;
}

static void calc_filterQ(Layer *lay, SFInfo *sf, SampleList *sp)
{
	int16 val = lay->val[SF_initialFilterQ];
	if (sf->version == 1)
		val = val * 3 / 2; /* to centibels */
	sp->resonance = pow(10.0, (double)val / 2.0 / 200.0) - 1;
	if (sp->resonance < 0)
		sp->resonance = 0;
}

