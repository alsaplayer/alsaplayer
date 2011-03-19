/*================================================================
 * SoundFont(tm) file format
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

#ifndef SBK_H_DEF
#define SBK_H_DEF

typedef struct _tchunk {
	char id[4];
	uint32 size;
} tchunk;

typedef struct _tsbkheader {
	char riff[4];	/* RIFF */
	uint32 size;	/* size of sbk after there bytes */
	char sfbk[4];	/* sfbk id */
} tsbkheader;

typedef struct _tsamplenames {
	char name[21];
} tsamplenames;

typedef struct _tpresethdr {
	char name[20];
	uint16 preset, sub_preset, bank, sub_bank, bagNdx;
	/*int lib, genre, morphology;*/ /* reserved */
} tpresethdr;

typedef struct _tsampleinfo {
	uint32 startsample, endsample;
	uint32 startloop, endloop;
	/* ver.2 additional info */
	uint32 samplerate;
	uint8 originalPitch;
	uint8 pitchCorrection;
	uint16 samplelink;
	uint16 sampletype;  /*1=mono, 2=right, 4=left, 8=linked, $8000=ROM*/
} tsampleinfo;

typedef struct _tinsthdr {
	char name[20];
	uint16 bagNdx;
} tinsthdr;

typedef struct _tgenrec {
	uint16 oper;
	uint16 amount;
} tgenrec;


/*
 *
 */

typedef struct _SFInfo {
	uint16 version, minorversion;
	uint32 samplepos, samplesize;

	int nrsamples;
	tsamplenames *samplenames;

	int nrpresets;
	tpresethdr *presethdr;
	
	int nrinfos;
	tsampleinfo *sampleinfo;

	int nrinsts;
	tinsthdr *insthdr;

	int nrpbags, nribags;
	uint16 *presetbag, *instbag;

	int nrpgens, nrigens;
	tgenrec *presetgen, *instgen;

	tsbkheader sbkh;

	/*char *sf_name;*/

	int in_rom;

} SFInfo;


/*----------------------------------------------------------------
 * functions
 *----------------------------------------------------------------*/

extern void load_sbk(FILE *fp, SFInfo *sf);
extern void free_sbk(SFInfo *sf);
extern void sbk_to_text(char *text, int type, int val, SFInfo *sf);
extern void autocfg();

#endif
