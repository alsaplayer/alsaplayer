%{
/*
 * Cfg file reader, Greg Lee, 8/93.
 * Adapted from Adagio for KMidi 12/99.
 *	$Id$
 */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "effects.h"
#include "md.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "version.h"

#define YY_ALWAYS_INTERACTIVE 0
#define YY_NEVER_INTERACTIVE 1

#define YY_INPUT(buf,result,max_size) \
	 result = fread( buf, 1, max_size, yyin );

#ifdef STANDALONE
extern int set_play_mode(char *cp);
#endif
static int prescanning;
static char doing_drums = 0;
static char doing_sf = 0;
static int patchno = -1;
static int i, j;
static char *patchname;
static char cfg_flag = 1;
static int tone_bank;
static void set_patchno(char *m);

static char *s_dirname = 0;
static char *sfname = 0;

static int current_toneset = 0;
static int current_drumset = 0;

static int cfg_condition = -1;

static ToneBank *bank=0;
static int banknum=0;
static int rcf_count=1;
static int font_type=FONT_NORMAL;


#define MAX_INCLUDE_DEPTH 40
static YY_BUFFER_STATE include_stack[MAX_INCLUDE_DEPTH];
static int include_stack_ptr = 0;

static void new_toneset(int n);
static void new_patch(const char *vname, int num);

%}

%option noyywrap
%option nounput

%s P K S

wh	[ \t]+
owh	[ \t]*
nm	([^ \t\n\r\"#]+|\"[^\"\n]+\")
cm	[ \t]*("#".*)?\n

%%

{owh}"#".*	;
^#.*\n	;

^{owh}"if"{wh}[0-9]+ {
	int i = 0;
	while (isspace(yytext[i])) i++;
	i += 2;
	cfg_condition = atoi(yytext+i);
}
^{owh}"else" {
	cfg_condition = 0;
}
^{owh}("sf"|"sbk"){wh}{nm}{owh}[0-9]* {
	int sf_oldbank, sf_newbank = banknum;
	int i = 0, j;
	while (isspace(yytext[i])) i++;
	if (yytext[i+1] == 'f') i += 2;
	else i += 3;
	while (isspace(yytext[i])) i++;
	if (yytext[i] == '"') {
		i++;
		for (j = i; j < yyleng && yytext[j] != '"'; j++) ;
		/*j--;*/
	}
	else for (j = i; j < yyleng && !isspace(yytext[j]); j++) ;
	sfname = strncpy( (char *)malloc(j-i+1), yytext+i, j-i );
	sfname[j-i] = '\0';
	if (doing_drums) sf_newbank += 256;
	sf_oldbank = sf_newbank;
	if (j < yyleng) {
		while (j < yyleng && isspace(yytext[j])) j++;
		if (j < yyleng && isdigit(yytext[j])) sf_oldbank = atoi(yytext+j);
	}
	init_soundfont(sfname, sf_oldbank, sf_newbank, rcf_count);
}
^{owh}("dir"|"PatchDir:"){wh}{nm} {
	int i = 0;
	while (isspace(yytext[i])) i++;
	if (yytext[i] == 'd') i += 3;
	else i += 9;
	while (isspace(yytext[i])) i++;
	s_dirname = strcpy( (char *)malloc(strlen(yytext+i)+1), yytext+i );
	add_to_pathlist(s_dirname, rcf_count);
	free(s_dirname);
}
^{owh}"source"{wh}{nm} {
	char *fname;
	int i = 0;
	FILE *save_yyin;

	while (isspace(yytext[i])) i++;
	i += 6;
	while (isspace(yytext[i])) i++;

	 if (prescanning && cfg_condition >= 0 && cfg_condition < 30 &&
		rcf_count==1 && !cfg_names[cfg_condition])
	  {
	    cfg_names[cfg_condition] = (char *)safe_malloc(strlen(yytext+i)+1);
	    strcpy(cfg_names[cfg_condition], yytext+i);
	  }

    if (!prescanning && (cfg_condition < 0 || cfg_condition == cfg_select)) {


        if ( include_stack_ptr >= MAX_INCLUDE_DEPTH ) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		 "Probable source loop in configuration files");
		return (-1);
        }
        include_stack[include_stack_ptr++] =
                YY_CURRENT_BUFFER;

	fname = strcpy( (char *)malloc(strlen(yytext+i)+1), yytext+i );

	save_yyin = yyin;
	yyin = NULL;

	yyin = open_file(fname, 1, OF_VERBOSE, rcf_count);

	if (yyin == NULL) {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		 "Source file %s not found\n", fname);
		include_stack_ptr--;
	}

	if (yyin == NULL) yyin = save_yyin;
	else {
            cfg_condition = -1;
	    rcf_count++;
            yy_switch_to_buffer(
                yy_create_buffer( yyin, YY_BUF_SIZE ) );
	}
    }
    cfg_condition = -1;
}  

<<EOF>> {
        if ( --include_stack_ptr < 0 ) {
                yyterminate();
        }
        else {
		if (yyin) close_file(yyin);
                yy_delete_buffer( YY_CURRENT_BUFFER );
		rcf_count--;
                yy_switch_to_buffer(
                     include_stack[include_stack_ptr] );
        }
}

^{owh}"drumset"{wh}[0-9]+{wh}("sf"|"sbk"){wh}{nm}	{
	char *bank_name = NULL;
	int i = 0;
	while (isspace(yytext[i])) i++;
	i += 7;
	current_drumset = atoi(yytext+i);
	doing_drums = 1;
	doing_sf = 1;
	new_toneset(current_drumset);
	font_type=FONT_SBK;
    if (!bank->name) {
	while (isspace(yytext[i])) i++;
	while (isdigit(yytext[i])) i++;
	while (isspace(yytext[i])) i++;
	if (yytext[i+1] == 'f') i += 2; else i += 3;
	while (isspace(yytext[i])) i++;
	patchname = yytext + i;
	if (patchname[0] == '"') {
		bank_name = strncpy( (char *)malloc(strlen(patchname)-1), patchname + 1, strlen(patchname)-1 );
		bank_name[strlen(patchname)-2] = '\0';
	}
	else bank_name = strcpy( (char *)malloc(strlen(patchname)+1), patchname );
	bank->name = bank_name;
    }

}
^{owh}"drumset"{wh}[0-9]+{wh}("sf"|"sbk")	{
	int i = 0;
	while (isspace(yytext[i])) i++;
	i += 7;
	current_drumset = atoi(yytext+i);
	doing_drums = 1;
	doing_sf = 1;
	new_toneset(current_drumset);
	font_type=FONT_SBK;
}
^{owh}"drumset"{wh}[0-9]+{wh}{nm}	{
	char *bank_name = NULL;
	int i = 0;
	while (isspace(yytext[i])) i++;
	i += 7;
	current_drumset = atoi(yytext+i);
	doing_drums = 1;
	doing_sf = 0;
	new_toneset(current_drumset);
	font_type=FONT_NORMAL;
    if (!bank->name) {
	while (isspace(yytext[i])) i++;
	while (isdigit(yytext[i])) i++;
	while (isspace(yytext[i])) i++;
	patchname = yytext + i;
	if (patchname[0] == '"') {
		bank_name = strncpy( (char *)malloc(strlen(patchname)-1), patchname + 1, strlen(patchname)-1 );
		bank_name[strlen(patchname)-2] = '\0';
	}
	else bank_name = strcpy( (char *)malloc(strlen(patchname)+1), patchname );
	bank->name = bank_name;
    }
}
^{owh}"drumset"{wh}[0-9]+	{
	int i = 0;
	while (isspace(yytext[i])) i++;
	i += 7;
	current_drumset = atoi(yytext+i);
	doing_drums = 1;
	doing_sf = 0;
	new_toneset(current_drumset);
	font_type=FONT_NORMAL;
}
^{owh}"bank"{wh}[0-9]+{wh}("sf"|"sbk")	{
	int i = 0;
	while (isspace(yytext[i])) i++;
	i += 4;
	current_toneset = atoi(yytext+i);
	doing_drums = 0;
	doing_sf = 1;
	new_toneset(current_toneset);
	font_type=FONT_SBK;
}
^{owh}"bank"{wh}[0-9]+	{
	int i = 0;
	while (isspace(yytext[i])) i++;
	i += 4;
	current_toneset = atoi(yytext+i);
	doing_drums = 0;
	doing_sf = 0;
	new_toneset(current_toneset);
	font_type=FONT_NORMAL;
}
^{owh}"sfx"	{
	current_toneset = SFXBANK;
	doing_drums = 0;
	doing_sf = 0;
	new_toneset(current_toneset);
	font_type=FONT_NORMAL;
}
^{owh}"drumsfx1"	{
	current_drumset = SFXDRUM1;
	doing_drums = 1;
	doing_sf = 0;
	new_toneset(current_toneset);
	font_type=FONT_NORMAL;
	bank->name = "drumsfx1";
}
^{owh}"drumsfx2"	{
	current_drumset = SFXDRUM2;
	doing_drums = 1;
	doing_sf = 0;
	new_toneset(current_toneset);
	font_type=FONT_NORMAL;
	bank->name = "drumsfx2";
}

^{owh}"[Melodic Patches]" {
	doing_drums = 0;
}
^{owh}"[Drum Patches]" {
	doing_drums = 1;
	current_drumset = current_toneset;
}

^{owh}[0-9,]+("="|{wh}){nm} {
	const char *gm_name = NULL;
	char *vc_name = NULL;
	patchno = atoi(yytext);
	tone_bank = 0;
	if (patchno >= 0) {
		i = 0;
		while (isspace(yytext[i])) i++;
		while (isdigit(yytext[i])) i++;
		if (yytext[i] == ',') {
			i++;
			tone_bank = atoi(yytext + i);
			while (isdigit(yytext[i])) i++;
		}
		else {
			if (doing_drums) tone_bank = current_drumset;
			else tone_bank = current_toneset;
		}
		new_toneset(tone_bank);

		while (isspace(yytext[i])) i++;
		if (!cfg_flag && yytext[i] == '=') i++;
		if ( (patchname = strrchr(yytext + i, '\\')) == NULL)
			patchname = yytext + i;
		else patchname++;
		if (patchname[0] == '"') {
			vc_name = strncpy( (char *)malloc(strlen(patchname)-1), patchname + 1, strlen(patchname)-1 );
			vc_name[strlen(patchname)-2] = '\0';
		}
		else vc_name = strcpy( (char *)malloc(strlen(patchname)+1), patchname );
		if (patchno < 128) gm_name = gm_voice[doing_drums? patchno+128 : patchno].vname;
		else {
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Bad patch number %d in cfg file.", patchno);
		}
		if (gm_name == NULL) gm_name = (doing_drums)? "drum":"instr";

	        ctl->cmsg(CMSG_INFO, VERB_DEBUG, "config: voice %s[%d,%d] = patch %s",
			gm_name, patchno, tone_bank, vc_name);

		new_patch(vc_name, patchno);

	}
	BEGIN(P);
}

^{owh}[A-Z][^\n\r\t\=,#]+(","{owh}[0-9]+)?{owh}"="{owh}{nm} {
	const char *gm_name = NULL;
	char *vc_name = NULL;
	int i;
	set_patchno(yytext);
	tone_bank = 0;
	if (patchno < 256) {
		i = 0;
		while (yytext[i] != '=' && yytext[i] != ',') i++;
		if (yytext[i] == ',') {
			i++;
			tone_bank = atoi(yytext + i);
		}
		else {
			if (doing_drums) tone_bank = current_drumset;
			else tone_bank = current_toneset;
		}

		while (yytext[i] != '=') i++;
		i++;
		while (isspace(yytext[i])) i++;
		if ( (patchname = strrchr(yytext + i, '\\')) == NULL)
			patchname = yytext + i;
		else patchname++;
		if (patchname[0] == '"') {
			vc_name = strncpy( (char *)malloc(strlen(patchname)-1), patchname + 1, strlen(patchname)-1 );
			vc_name[strlen(patchname)-2] = '\0';
		}
		else vc_name = strcpy( (char *)malloc(strlen(patchname)+1), patchname );
		gm_name = gm_voice[patchno].vname;
		if (gm_name == NULL) gm_name = (doing_drums)? "drum":"instr";

	        ctl->cmsg(CMSG_INFO, VERB_DEBUG, "config: voice %s[%d,%d] = patch %s",
			gm_name, patchno, tone_bank, vc_name);

	}
	else {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Bad line \"%s ...\" in cfg file.", yytext);
	}
	new_patch(vc_name, patchno);
	
	BEGIN(P);
}

^{owh}"-p"{owh}[0-9]+	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		j = atoi(yytext+i+2);
		if (j > 1 && j <= MAX_VOICES) config_voices = j;
	} }
^{owh}"-A"{wh}[0-9]+	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		j = atoi(yytext+i+2);
		if (j > 1 && j <= MAX_AMPLIFICATION) config_amplification = j;
	} }
^{owh}"-C"{owh}[0-9]+	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		j = atoi(yytext+i+2);
		if (j > 1 && j <= MAX_CONTROL_RATIO) control_ratio = j;
	} }
^{owh}"-s"{owh}[0-9]+	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		j = atoi(yytext+i+2);
		if (j < 100) j *= 1000;
		if (j > MIN_OUTPUT_RATE && j <= MAX_OUTPUT_RATE) play_mode->rate = j;
	} }
^{owh}"-r"{owh}[0-9]+	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		j = atoi(yytext+i+2);
		max_patch_memory = j * 1000000;
	} }
^{owh}"-k"{owh}[0-9]+	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		j = atoi(yytext+i+2);
		if (j >= 0 && j <= 3) config_interpolation = j;
	} }
^{owh}"-X"{owh}[0-9]+	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		j = atoi(yytext+i+2);
		if (j >= 0 && j <= 2) opt_expression_curve = j;
	} }
^{owh}"-V"{owh}[0-9]+	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		j = atoi(yytext+i+2);
		if (j >= 0 && j <= 2) opt_volume_curve = j;
	} }
^{owh}"-O"{owh}[a-zA-Z]	{ if (prescanning) {
		i=0; while(isspace(yytext[i])) i++;
		i+=2;
		while(isspace(yytext[i])) i++;
#ifdef STANDALONE
		if (set_play_mode(yytext+i)) /* error */;
#endif
	} }

<P>{owh}"amp"{owh}"="{owh}[0-9]+	{
	int i = 0;
	while (yytext[i] != '=') i++;
	bank->tone[patchno].amp = atoi(yytext+i+1);
	BEGIN(P);
}
<P>{owh}"note"{owh}"="{owh}[0-9]+	{
	int i = 0;
	while (yytext[i] != '=') i++;
	bank->tone[patchno].note = atoi(yytext+i+1);
	BEGIN(P);
}
<P>{owh}"tuning"{owh}"="{owh}("+"|"-")[0-9]+	{
	int i = 0;
	while (yytext[i] != '=') i++;
	bank->tone[patchno].tuning = atoi(yytext+i+1);
	BEGIN(P);
}
<P>{owh}"keep"{owh}"="	BEGIN(K);
<P>{owh}"strip"{owh}"="	BEGIN(S);

<K>{owh}"loop"	{
		  bank->tone[patchno].strip_loop=0;
	BEGIN(P);
}
<K>{owh}"tail"	{
		  bank->tone[patchno].strip_tail=0;
	BEGIN(P);
}
<K>{owh}"env"	{
		  bank->tone[patchno].strip_envelope=0;
	BEGIN(P);
}
<S>{owh}"loop"	{
		  bank->tone[patchno].strip_loop=1;
	BEGIN(P);
}
<S>{owh}"tail"	{
		  bank->tone[patchno].strip_tail=1;
	BEGIN(P);
}
<S>{owh}"env"	{
		  bank->tone[patchno].strip_envelope=1;
	BEGIN(P);
}
^{owh}[A-Z][^\n\r\t\:#]+{owh}":"{owh}"poly"  {
	set_patchno(yytext);
	if (patchno < 256) gm_voice[patchno].flags &= ~SOLO_MASK;
}
^{owh}[A-Z][^\n\r\t\:#]+{owh}":"{owh}"solo"  {
	set_patchno(yytext);
	if (patchno < 256) gm_voice[patchno].flags |= SOLO_MASK;
}

. |
\n	{
	patchno = -1;
	BEGIN(0);
}
%%

static void new_toneset(int n)
{
	banknum=n;

	if (!doing_drums) {
	  if (!tonebank[n]) {
		tonebank[n]=(ToneBank *)safe_malloc(sizeof(ToneBank));
	      	memset(tonebank[n], 0, sizeof(ToneBank));
	  }
	  bank=tonebank[n];
	}
	else {
	  if (!drumset[n]) {
		drumset[n]=(ToneBank *)safe_malloc(sizeof(ToneBank));
	      	memset(drumset[n], 0, sizeof(ToneBank));
	  }
	  bank=drumset[n];
	}
}

static void new_patch(const char *vname, int num)
{
	if (bank->tone[num].name) return;
	bank->tone[num].name=vname;
	bank->tone[num].note=bank->tone[num].amp=bank->tone[num].pan=
	  bank->tone[num].strip_loop=bank->tone[num].strip_envelope=
	    bank->tone[num].sf_ix=
	    bank->tone[num].strip_tail=bank->tone[num].last_used=-1;
	bank->tone[num].font_type=font_type;
	bank->tone[num].tuning=0;
	bank->tone[num].layer=0;
}

static void set_patchno(char *m)
{
	int i,j,w,y;
	const char *vn;
	y = w = 0;
	while (isspace(m[w])) w++;
	for (i = 0; i < 256; i++) {
		vn = gm_voice[i].vname;
		if (vn == NULL) continue;
		for (j = 0; m[j+w] && vn[j+y] && m[j+w] == vn[j+y]; j++)
		    if (vn[j+y+1] && isspace(vn[j+y+1]) && m[j+w+1] && !isspace(m[j+w+1]) ) y++ ;
		if (!m[j+w] || m[j+w] == '=' || m[j+w] == ',' || m[j+w] == ':') break;
		if (!vn[j+y] && isspace(m[j+w])) break;
	}
	if (i < 256 && vn != NULL) patchno = i;
	else patchno = 256;
}


char *current_config_file = 0;
int current_config_pc42b = 0;

static int pc42b_config_file(const char *name, int prescan);

int read_config_file(const char *name, int prescan)
{
	int retvalue = 0;
	prescanning = prescan;
	include_stack_ptr = 0;
	rcf_count = 1;

	if (current_config_pc42b) {
		return pc42b_config_file(name, prescan);
	}

	if (current_config_file) {
		yyin = fopen(current_config_file, "r");
	}
	else yyin = open_file(name, 1, OF_VERBOSE, rcf_count);

	if (!yyin) return -1;

	if (!current_config_file) {
	   current_config_file = (char *)safe_malloc(strlen(current_filename)+1);
	   strcpy(current_config_file, current_filename);
	}

	current_toneset = current_drumset = 0;
	doing_drums = doing_sf = 0;
	if (!(retvalue = yylex())) {
	   if (prescan) got_a_configuration = 1;
	   else got_a_configuration = 2;
	}

	close_file(yyin);
	return retvalue;
}

#include "PC42b.h"

static int pc42b_config_file(const char *name, int prescan)
{
	int retvalue = 0;
	int i = 0;
	prescanning = prescan;
	rcf_count = 1;
/*
	if (prescanning) {
	   current_config_file = (char *)safe_malloc(strlen(name)+1);
	   strcpy(current_config_file, name);
	}
*/
	current_toneset = current_drumset = 0;
	doing_drums = doing_sf = 0;

	while (1) {
		int action, which;
		const char *sfname;
		char *vc_name = NULL;

		action = PC42b[i].font_code;
		which = PC42b[i].num;
		sfname = PC42b[i].name;

		switch (action) {
			case FONT_TONESET:
				current_toneset = which;
				doing_drums = 0;
				doing_sf = 1;
				new_toneset(current_toneset);
				font_type=FONT_SBK;
				break;
			case FONT_DRUMSET:
				current_drumset = which;
				doing_drums = 1;
				doing_sf = 1;
				new_toneset(current_drumset);
				font_type=FONT_SBK;
    				if (!bank->name) bank->name = sfname;
				break;
			case FONT_PRESET:
				patchno = which;
				if (doing_drums) tone_bank = current_drumset;
				else tone_bank = current_toneset;
				new_toneset(tone_bank);
				new_patch(sfname, patchno);
				break;
			case FONT_SBK:
				init_soundfont(sfname, which, which, 1);
				break;
		}

		if (action == FONT_SBK) break;
		i++;
	}

	if (prescan) got_a_configuration = 1;
	else got_a_configuration = 2;
	return retvalue;
}

