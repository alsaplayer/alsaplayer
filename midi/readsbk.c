/*================================================================
 * readsbk.c:
 *	read soundfont file
 *================================================================*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gtim.h"
#include "sbk.h"

/*----------------------------------------------------------------
 * function prototypes
 *----------------------------------------------------------------*/

#define NEW(type,nums)	(type*)malloc(sizeof(type) * (nums))

static int READCHUNK(tchunk *vp, FILE *fd)
{
	if (fread(vp, 8, 1, fd) != 1) return -1;
	vp->size = LE_LONG(vp->size);
	return 1;
}

static int READDW(uint32 *vp, FILE *fd)
{
	if (fread(vp, 4, 1, fd) != 1) return -1;
	*vp = LE_LONG(*vp);
	return 1;
}	

static int READW(uint16 *vp, FILE *fd)
{
	if (fread(vp, 2, 1, fd) != 1) return -1;
	*vp = LE_SHORT(*vp);
	return 1;
}

#define READSTR(var,fd)	fread(var, 20, 1, fd)

#if 0
static int READSTR(char *str, FILE *fd)
{
    int n;

    if(fread(str, 20, 1, fd) != 1) return -1;
    str[20] = '\0';
    n = strlen(str);
    while(n > 0 && str[n - 1] == ' ')
	n--;
    str[n] = '\0';
    return n;
}
#endif

#define READID(var,fd)	fread(var, 1, 4, fd)
#define READB(var,fd)	fread(var, 1, 1, fd)
#define SKIPB(fd)	{uint8 dummy; fread(&dummy, 1, 1, fd);}
#define SKIPW(fd)	{uint16 dummy; fread(&dummy, 2, 1, fd);}
#define SKIPDW(fd)	{uint32 dummy; fread(&dummy, 4, 1, fd);}

static int getchunk(const char *id);
static void process_chunk(int id, int s, SFInfo *sf, FILE *fd);
static void load_sample_names(int size, SFInfo *sf, FILE *fd);
static void load_preset_header(int size, SFInfo *sf, FILE *fd);
static void load_inst_header(int size, SFInfo *sf, FILE *fd);
static void load_bag(int size, SFInfo *sf, FILE *fd, int *totalp, unsigned short **bufp);
static void load_gen(int size, SFInfo *sf, FILE *fd, int *totalp, tgenrec **bufp);
static void load_sample_info(int size, SFInfo *sf, FILE *fd);



enum {
	/* level 0 */
	UNKN_ID, RIFF_ID, LIST_ID,
	/* level 1 */
	INFO_ID, SDTA_ID, PDTA_ID,
	/* info stuff */
	IFIL_ID, ISNG_ID, IROM_ID, INAM_ID, IVER_ID, IPRD_ID, ICOP_ID,
#ifdef tplussbk
	ICRD_ID, IENG_ID, ISFT_ID, ICMT_ID,
#endif
	/* sample data stuff */
	SNAM_ID, SMPL_ID,
	/* preset stuff */
	PHDR_ID, PBAG_ID, PMOD_ID, PGEN_ID,
	/* inst stuff */
	INST_ID, IBAG_ID, IMOD_ID, IGEN_ID,
	/* sample header */
	SHDR_ID
};


/*----------------------------------------------------------------
 * debug routine
 *----------------------------------------------------------------*/

#if 0
static void debugid(char *tag, char *p)
{
	char buf[5]; strncpy(buf, p, 4); buf[4]=0;
	fprintf(stderr,"[%s:%s]\n", tag, buf);
}

static void debugname(char *tag, char *p)
{
	char buf[21]; strncpy(buf, p, 20); buf[20]=0;
	fprintf(stderr,"[%s:%s]\n", tag, buf);
}

static void debugval(char *tag, int v)
{
	fprintf(stderr, "[%s:%d]\n", tag, v);
}
#else
#define debugid(t,s) /**/
#define debugname(t,s) /**/
#define debugval(t,v) /**/
#endif


/*----------------------------------------------------------------
 * load sbk file
 *----------------------------------------------------------------*/

void load_sbk(FILE *fd, SFInfo *sf)
{
	tchunk chunk, subchunk;

	READID(sf->sbkh.riff, fd);
	READDW(&sf->sbkh.size, fd);
	READID(sf->sbkh.sfbk, fd);

	sf->in_rom = 1;
	while (! feof(fd)) {
		READID(chunk.id, fd);
		switch (getchunk(chunk.id)) {
		case LIST_ID:
			READDW(&chunk.size, fd);
			READID(subchunk.id, fd);
			process_chunk(getchunk(subchunk.id), chunk.size - 4, sf, fd);
			break;
		}
	}
}


/*----------------------------------------------------------------
 * free buffer
 *----------------------------------------------------------------*/

void free_sbk(SFInfo *sf)
{
	free(sf->samplenames);
	free(sf->presethdr);
	free(sf->sampleinfo);
	free(sf->insthdr);
	free(sf->presetbag);
	free(sf->instbag);
	free(sf->presetgen);
	free(sf->instgen);
	/* if (sf->sf_name) free(sf->sf_name); */
	memset(sf, 0, sizeof(*sf));
}



/*----------------------------------------------------------------
 * get id value
 *----------------------------------------------------------------*/

static int getchunk(const char *id)
{
	static struct idstring {
		const char *str;
		int id;
	} idlist[] = {
		{"LIST", LIST_ID},
#ifdef tplussbk
		{"sfbk", SFBK_ID},
#endif
		{"INFO", INFO_ID},
		{"sdta", SDTA_ID},
		{"snam", SNAM_ID},
		{"smpl", SMPL_ID},
		{"pdta", PDTA_ID},
		{"phdr", PHDR_ID},
		{"pbag", PBAG_ID},
		{"pmod", PMOD_ID},
		{"pgen", PGEN_ID},
		{"inst", INST_ID},
		{"ibag", IBAG_ID},
		{"imod", IMOD_ID},
		{"igen", IGEN_ID},
		{"shdr", SHDR_ID},
		{"ifil", IFIL_ID},
		{"isng", ISNG_ID},
		{"irom", IROM_ID},
		{"iver", IVER_ID},
		{"INAM", INAM_ID},
		{"IPRD", IPRD_ID},
		{"ICOP", ICOP_ID},
#ifdef tplussbk
		{"ICRD", ICRD_ID},
		{"IENG", IENG_ID},
		{"ISFT", ISFT_ID},
		{"ICMT", ICMT_ID},
#endif
	};

	unsigned int i;

	for (i = 0; i < sizeof(idlist)/sizeof(idlist[0]); i++) {
		if (strncmp(id, idlist[i].str, 4) == 0) {
			debugid("ok", id);
			return idlist[i].id;
		}
	}

	debugid("xx", id);
	return UNKN_ID;
}


static void load_sample_names(int size, SFInfo *sf, FILE *fd)
{
	int i;
	sf->nrsamples = size / 20;
	sf->samplenames = NEW(tsamplenames, sf->nrsamples);
	for (i = 0; i < sf->nrsamples; i++) {
		READSTR(sf->samplenames[i].name, fd);
		sf->samplenames[i].name[20] = 0;
	}
}

static void load_preset_header(int size, SFInfo *sf, FILE *fd)
{
	int i;
	sf->nrpresets = size / 38;
	sf->presethdr = NEW(tpresethdr, sf->nrpresets);
	for (i = 0; i < sf->nrpresets; i++) {
		READSTR(sf->presethdr[i].name, fd);
		READW(&sf->presethdr[i].preset, fd);
		sf->presethdr[i].sub_preset = sf->presethdr[i].preset;
		READW(&sf->presethdr[i].bank, fd);
		sf->presethdr[i].sub_bank = sf->presethdr[i].bank;
		READW(&sf->presethdr[i].bagNdx, fd);
		SKIPDW(fd); /* lib */
		SKIPDW(fd); /* genre */
		SKIPDW(fd); /* morph */
	}
}

static void load_inst_header(int size, SFInfo *sf, FILE *fd)
{
	int i;

	sf->nrinsts = size / 22;
	sf->insthdr = NEW(tinsthdr, sf->nrinsts);
	for (i = 0; i < sf->nrinsts; i++) {
		READSTR(sf->insthdr[i].name, fd);
		READW(&sf->insthdr[i].bagNdx, fd);
	}
		
}

static void load_bag(int size, SFInfo *sf, FILE *fd, int *totalp, unsigned short **bufp)
{
	unsigned short *buf;
	int i;

	debugval("bagsize", size);
	size /= 4;
	buf = NEW(unsigned short, size);
	for (i = 0; i < size; i++) {
		READW(&buf[i], fd);
		SKIPW(fd); /* mod */
	}
	*totalp = size;
	*bufp = buf;
}

static void load_gen(int size, SFInfo *sf, FILE *fd, int *totalp, tgenrec **bufp)
{
	tgenrec *buf;
	int i;

	debugval("gensize", size);
	size /= 4;
	buf = NEW(tgenrec, size);
	for (i = 0; i < size; i++) {
		READW(&buf[i].oper, fd);
		READW(&buf[i].amount, fd);
	}
	*totalp = size;
	*bufp = buf;
}

static void load_sample_info(int size, SFInfo *sf, FILE *fd)
{
	int i;

	debugval("infosize", size);
	if (sf->version > 1) {
		sf->nrinfos = size / 46;
		sf->nrsamples = sf->nrinfos;
		sf->sampleinfo = NEW(tsampleinfo, sf->nrinfos);
		sf->samplenames = NEW(tsamplenames, sf->nrsamples);
	}
	else  {
		sf->nrinfos = size / 16;
		sf->sampleinfo = NEW(tsampleinfo, sf->nrinfos);
	}


	for (i = 0; i < sf->nrinfos; i++) {
		if (sf->version > 1)
			READSTR(sf->samplenames[i].name, fd);
		READDW(&sf->sampleinfo[i].startsample, fd);
		READDW(&sf->sampleinfo[i].endsample, fd);
		READDW(&sf->sampleinfo[i].startloop, fd);
		READDW(&sf->sampleinfo[i].endloop, fd);
		if (sf->version > 1) {
			READDW(&sf->sampleinfo[i].samplerate, fd);
			READB(&sf->sampleinfo[i].originalPitch, fd);
			READB(&sf->sampleinfo[i].pitchCorrection, fd);
			READW(&sf->sampleinfo[i].samplelink, fd);
			READW(&sf->sampleinfo[i].sampletype, fd);
		} else {
			if (sf->sampleinfo[i].startsample == 0)
				sf->in_rom = 0;
			sf->sampleinfo[i].startloop++;
			sf->sampleinfo[i].endloop += 2;
			sf->sampleinfo[i].samplerate = 44100;
			sf->sampleinfo[i].originalPitch = 60;
			sf->sampleinfo[i].pitchCorrection = 0;
			sf->sampleinfo[i].samplelink = 0;
			if (sf->in_rom)
				sf->sampleinfo[i].sampletype = 0x8001;
			else
				sf->sampleinfo[i].sampletype = 1;
		}
	}
}


/*
 */

static void process_chunk(int id, int s, SFInfo *sf, FILE *fd)
{
	int cid;
	tchunk subchunk;

	switch (id) {
	case INFO_ID:
		READCHUNK(&subchunk, fd);
		while ((cid = getchunk(subchunk.id)) != LIST_ID) {
			switch (cid) {
			case IFIL_ID:
				READW(&sf->version, fd);
				READW(&sf->minorversion, fd);
				break;
			/*
			case INAM_ID:
				sf->sf_name = (char*)malloc(subchunk.size);
				if (sf->sf_name == NULL) {
					fprintf(stderr, "can't malloc\n");
					exit(1);
				}
				fread(sf->sf_name, 1, subchunk.size, fd);
				break;
			*/
			default:
				fseek(fd, subchunk.size, SEEK_CUR);
				break;
			}
			READCHUNK(&subchunk, fd);
			if (feof(fd))
				return;
		}
		fseek(fd, -8, SEEK_CUR); /* seek back */
		break;

	case SDTA_ID:
		READCHUNK(&subchunk, fd);
		while ((cid = getchunk(subchunk.id)) != LIST_ID) {
			switch (cid) {
			case SNAM_ID:
				if (sf->version > 1) {
					printf("**** version 2 has obsolete format??\n");
					fseek(fd, subchunk.size, SEEK_CUR);
				} else
					load_sample_names(subchunk.size, sf, fd);
				break;
			case SMPL_ID:
				sf->samplepos = ftell(fd);
				sf->samplesize = subchunk.size;
				fseek(fd, subchunk.size, SEEK_CUR);
			}
			READCHUNK(&subchunk, fd);
			if (feof(fd))
				return;
		}
		fseek(fd, -8, SEEK_CUR); /* seek back */
		break;

	case PDTA_ID:
		READCHUNK(&subchunk, fd);
		while ((cid = getchunk(subchunk.id)) != LIST_ID) {
			switch (cid) {
			case PHDR_ID:
				load_preset_header(subchunk.size, sf, fd);
				break;

			case PBAG_ID:
				load_bag(subchunk.size, sf, fd,
					 &sf->nrpbags, &sf->presetbag);
				break;

			case PMOD_ID: /* ignored */
				fseek(fd, subchunk.size, SEEK_CUR);
				break;

			case PGEN_ID:
				load_gen(subchunk.size, sf, fd,
					 &sf->nrpgens, &sf->presetgen);
				break;

			case INST_ID:
				load_inst_header(subchunk.size, sf, fd);
				break;

			case IBAG_ID:
				load_bag(subchunk.size, sf, fd,
					 &sf->nribags, &sf->instbag);
				break;

			case IMOD_ID: /* ingored */
				fseek(fd, subchunk.size, SEEK_CUR);
				break;

			case IGEN_ID:
				load_gen(subchunk.size, sf, fd,
					 &sf->nrigens, &sf->instgen);
				break;
				
			case SHDR_ID:
				load_sample_info(subchunk.size, sf, fd);
				break;

			default:
				fprintf(stderr, "unknown id\n");
				fseek(fd, subchunk.size, SEEK_CUR);
				break;
			}
			READCHUNK(&subchunk, fd);
			if (feof(fd)) {
				debugid("file", "EOF");
				return;
			}
		}
		fseek(fd, -8, SEEK_CUR); /* rewind */
		break;
	}
}

