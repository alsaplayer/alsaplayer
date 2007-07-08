/*================================================================
 * sbk2cfg  --  extracts info from sf2/sbk font and constructs
 *		a TiMidity cfg file.  Greg Lee, lee@hawaii.edu, 5/98.
 * The code is adapted from "sbktext" by Takashi Iwai, which contained
 * the following notice:
 *================================================================
 * Copyright (C) 1996,1997 Takashi Iwai
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
 *================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gtim.h"
#include "sbk.h"
#include "sflayer.h"

static SFInfo sfinfo;

static char *getname(char *p);
static void print_sbk(SFInfo *sf, FILE *fout);

static int check_sbk(SFInfo *sf);

int sf2cfg(char *sffname, char *outname)
{
	FILE *fp;
	int all_ok;

	if ((fp = fopen(sffname, "r")) == NULL) {
		return 0;
	}

	all_ok = 1;

	load_sbk(fp, &sfinfo);
	fclose(fp);

	if (!check_sbk(&sfinfo)) return 0;

	if ((fp = fopen(outname, "w")) == NULL) {
		return 0;
	}

	print_sbk(&sfinfo, fp);

	fprintf(fp, "\nsf \"%s\"\n", sffname);

	free_sbk(&sfinfo);
	fclose(fp);
	return 1;
}


static char *getname(char *p)
{
	int i;
	static char buf[21];
	strncpy(buf, p, 20);
	buf[20] = 0;
	for (i = 19; i > 4 && buf[i]==' '; i--) {
	  buf[i] = 0;
	}
	for (i = 0; buf[i]; i++) {
	  if (buf[i] == ' ') buf[i] = '_';
	  if (buf[i] == '#') buf[i] = '@';
	}
	return buf;
}

static int check_sbk(SFInfo *sf) {
	if (sf->version != 2) return 0;
	return 1;
}

static void print_sbk(SFInfo *sf, FILE *fout)
{
    int i, bank, preset, lastpatch;
    int lastbank = -1;
    tpresethdr *ip;
    tinsthdr *tp;

    fprintf(fout, "#  ** SoundFont: %d %d\n", sf->version, sf->minorversion);
    fprintf(fout, "#  ** SampleData: %d %d\n", (int)sf->samplepos, (int)sf->samplesize);

    fprintf(fout, "#\n#  ** Presets: %d\n", sf->nrpresets-1);

    for (bank = 0; bank <= 128; bank++) {
      for (preset = 0; preset <= 127; preset++) {
      lastpatch = -1;
	for (i = 0, ip = sf->presethdr; i < sf->nrpresets-1; i++) {
	    int b, g, inst, sm_idx;
	    if (ip[i].bank != bank) continue;
	    if (ip[i].preset != preset) continue;
	    inst = -1;
	    for (b = ip[i].bagNdx; b < ip[i+1].bagNdx; b++) {
		for (g = sf->presetbag[b]; g < sf->presetbag[b+1]; g++)
		   if (sf->presetgen[g].oper == SF_instrument) {
			inst = sf->presetgen[g].amount;
			break;
		   }
	    }
	    if (inst < 0) continue;
	    tp = sf->insthdr;
	    if (bank != 128) {
		int realwaves = 0;
		if (bank != lastbank) {
			fprintf(fout, "\nbank %d sf\n", bank);
			lastbank = bank;
		}
		for (b = tp[inst].bagNdx; b < tp[inst+1].bagNdx; b++) {
		    sm_idx = -1;
		    for (g = sf->instbag[b]; g < sf->instbag[b+1]; g++) {
			if (sf->instgen[g].oper == SF_sampleId) sm_idx = sf->instgen[g].amount;
		    }
		    if (sm_idx >= 0 && sf->sampleinfo[sm_idx].sampletype < 0x8000) realwaves++;
		}
		if (realwaves && ip[i].preset != lastpatch) {
		    fprintf(fout, "\t%3d %s\n", ip[i].preset, getname(ip[i].name));
		    lastpatch = ip[i].preset;
		}
	    }
	    else {
		int keynote, c, dpreset;
		fprintf(fout, "\ndrumset %d sf\t%s\n", ip[i].preset, getname(ip[i].name));

		for (dpreset = 0; dpreset < 128; dpreset++)
	        for (c = ip[i].bagNdx; c < ip[i+1].bagNdx; c++) {
		  inst = -1;
		  for (g = sf->presetbag[c]; g < sf->presetbag[c+1]; g++)
		   if (sf->presetgen[g].oper == SF_instrument) {
			inst = sf->presetgen[g].amount;
			break;
		   }
		  if (inst >= 0) for (b = tp[inst].bagNdx; b < tp[inst+1].bagNdx; b++) {
		    int hikeynote = -1;
		    sm_idx = keynote = -1;
		    for (g = sf->instbag[b]; g < sf->instbag[b+1]; g++) {
			if (sf->instgen[g].oper == SF_sampleId) sm_idx = sf->instgen[g].amount;
			else if (sf->instgen[g].oper == SF_keyRange) {
			    keynote = sf->instgen[g].amount & 0xff;
			    hikeynote = (sf->instgen[g].amount >> 8) & 0xff;
			}
		    }
		    if (sm_idx < 0) continue;
		    if (sf->sampleinfo[sm_idx].sampletype >= 0x8000) continue;
		    /*if (keynote != dpreset) continue;*/
		    if (dpreset < keynote) continue;
		    if (dpreset > hikeynote) continue;
		    if (sm_idx >= 0 && keynote >= 0 && dpreset != lastpatch) {
			   fprintf(fout, "\t%3d %s\n", dpreset, getname( sf->samplenames[sm_idx].name ));
			   lastpatch = dpreset;
		    }
		  }
	        }

	    }
	}
      }
    }
}
