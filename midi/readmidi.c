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

*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifndef NO_STRING_H
# include <string.h>
#else
#include <strings.h>
#endif

#ifndef __WIN32__
#include <unistd.h>
#endif

#include <ctype.h>

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

static int32 quietchannels=0;

#if MAXCHAN <= 16
#define MERGE_CHANNEL_PORT(ch) ((int)(ch))
#else
#define MERGE_CHANNEL_PORT(ch) ((int)(ch) | (d->midi_port_number << 4))
#endif


/* These would both fit into 32 bits, but they are often added in
   large multiples, so it's simpler to have two roomy ints */
/*static int32 sample_increment, sample_correction;*/ /*samples per MIDI delta-t*/

#ifdef SFXDRUMOLD
/* I seem not to be using these tables ... */
static unsigned char sfxdrum1[100] = {
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,48,49,47,50,
0,46,0,0,0,0,0,0,0,0,
0,0,44,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,79,80,
81,82,83,84,0,0,0,0,0,0,
0,0,0,0,76,77,78,0,0,0,
0,0,0,0,0,0,0,0,0,0
};

static unsigned char sfxdrum2[100] = {
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,59,60,41,
42,46,0,0,0,0,0,0,0,0,
0,0,63,64,65,66,67,68,69,69,
72,70,0,0,0,0,0,0,52,53,
54,56,57,58,0,0,0,0,0,0,
0,0,0,0,73,74,75,71,0,0,
0,0,0,0,0,0,0,0,0,0
};
#endif

/* Computes how many (fractional) samples one MIDI delta-time unit contains */
static void compute_sample_increment(int32 tempo, int32 divisions, struct md *d)
{
  double a;
  a = (double) (tempo) * (double) (play_mode->rate) * (65536.0/1000000.0) /
    (double)(divisions);

  d->sample_correction = (int32)(a) & 0xFFFF;
  d->sample_increment = (int32)(a) >> 16;

  ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Samples per delta-t: %d (correction %d)",
       d->sample_increment, d->sample_correction);
}

#ifdef tplus
static int tf_getc(struct md *d)
{
    uint8 b;
    if (fread(&b,1,1,d->fp) != 1) return EOF;
    return (uint8)b;
}

/* Read variable-length number (7 bits per byte, MSB first) */
static int32 getvl(struct md *d)
{
    int32 l;
    int c;

    errno = 0;
    l = 0;

    /* 1 */
    if((c = tf_getc(d)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 2 */
    if((c = tf_getc(d)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 3 */
    if((c = tf_getc(d)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 4 */
    if((c = tf_getc(d)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;

    /* Error */
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	      "\"%s\": Illegal Variable-length quantity format.",
	      d->midi_name);
    return -2;

  eof:
    if(errno)
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "\"%s\": read_midi_event: %s",
		  d->midi_name, strerror(errno));
    else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Warning: \"%s\": Short midi file.",
		  d->midi_name);
    return -1;
}

#else

/* Read variable-length number (7 bits per byte, MSB first) */
static uint32 getvl(struct md *d)
{
  uint32 l=0;
  uint8 c;
  for (;;)
    {
      fread(&c,1,1,d->fp);
      l += (c & 0x7f);
      if (!(c & 0x80)) return l;
      l<<=7;
    }
}
#endif

/**********************************/
#ifdef INFO_ONLY
struct meta_text_type *meta_text_list = NULL;

static void free_metatext()
{
    struct meta_text_type *meta;
    while (meta_text_list) {
	meta = meta_text_list;
	meta_text_list = meta->next;
	free(meta->text);
	meta->text = NULL;
	free(meta);
	meta = NULL;
    }
}

static int metatext(int type, uint32 leng, char *mess, struct md *d)
{
    static int continued_flag = 0;
    int c;
    uint32 n;
    unsigned char *p = (unsigned char *)mess;
    struct meta_text_type *meta, *mlast;
    char *meta_string;

    /* if (d->at > 0 && (type == 1||type == 5||type == 6||type == 7)) { */
    if (type==5 || ( d->at > 0 && (type==1||type==6||type==7) )) {
	meta = (struct meta_text_type *)safe_malloc(sizeof(struct meta_text_type));
	if (leng > 72) leng = 72;
	meta_string = (char *)safe_malloc(leng+8);
	if (!leng) {
	    continued_flag = 1;
	    meta_string[leng++] = '\012';
	}
	else for (n = 0; n < leng; n++) {
	    c = *p++;
	    if (!n && (c == '/' || c == '\\')) {
		continued_flag = 1;
		meta_string[0] = '\012';
	        if (c == '/') {
		meta_string[1] = ' ';
		meta_string[2] = ' ';
		meta_string[3] = ' ';
		meta_string[4] = ' ';
		leng += 4;
		n += 4;
		}
	    }
	    else {
		/*meta_string[n] = (isprint(c) || isspace(c)) ? c : '.';*/
		meta_string[n] = ((c > 0x7f) || isprint(c) || isspace(c)) ? c : '.';
		if (c == '\012' || c == '\015') continued_flag = 1;
		if (c == '\015') meta_string[n] = '\012';
	    }
	}
	if (!continued_flag) {
		meta_string[leng++] = '\012';
	}
	meta_string[leng] = '\0';
	meta->type = type;
	meta->text = meta_string;
	meta->time = d->at;
	meta->next = NULL;
/*
while d->at==0
1. head = meta1; head->next = NULL
2. for ...: mlast = head
   if ...: not <
   else ...: meta2->next = NULL
	     head->next = meta2
   so: (meta1, meta2)
3. for ...: mlast = meta2
   if ...:
   else: meta3->next = NULL
	 meta2->next = meta3
   so: (meta1, meta2, meta3)
*/
	if (meta_text_list == NULL) {
	    meta->next = meta_text_list;
	    meta_text_list = meta;
	}
	else {
	    for (mlast = meta_text_list; mlast->next != NULL; mlast = mlast->next)
		if (mlast->next->time > meta->time) break;
	    if (mlast == meta_text_list && meta->time < mlast->time) {
		meta->next = meta_text_list;
		meta_text_list = meta;
	    }
	    else {
		meta->next = mlast->next;
		mlast->next = meta;
	    }
	}
	return 1;
    }
    else return 0;
}
#endif

static int sysex(uint32 len, uint8 *syschan, uint8 *sysa, uint8 *sysb, struct md *d)
{
  unsigned char *s=(unsigned char *)safe_malloc(len);
  int id, model, ch, port, adhi, adlo, cd, dta, dtb, dtc;
  if (len != fread(s, 1, len, d->fp))
    {
      free(s);
      return 0;
    }
  if (len<5) { free(s); return 0; }
  if (d->curr_track == d->curr_title_track && d->track_info > 1) d->title[0] = '\0';
  id=s[0]; port=s[1]; model=s[2]; adhi=s[3]; adlo=s[4];
  if (id==0x7e && port==0x7f && model==0x09 && adhi==0x01)
    {
      ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "GM System On", len);
      d->GM_System_On=1;
      free(s);
      return 0;
    }
  ch = adlo & 0x0f;
  *syschan=(uint8)ch;
  if (id==0x7f && len==7 && port==0x7f && model==0x04 && adhi==0x01)
    {
      ctl->cmsg(CMSG_TEXT, VERB_DEBUG, "Master Volume %d", s[4]+(s[5]<<7));
      free(s);
      *sysa = s[4];
      *sysb = s[5];
      return ME_MASTERVOLUME;
      /** return s[4]+(s[5]<<7); **/
    }
  if (len<8) { free(s); return 0; }
  port &=0x0f;
  ch = (adlo & 0x0f) | ((port & 0x03) << 4);
  *syschan=(uint8)ch;
  cd=s[5]; dta=s[6];
  if (len >= 8) dtb=s[7];
  else dtb=-1;
  if (len >= 9) dtc=s[8];
  else dtc=-1;
  free(s);
  if (id==0x43 && model==0x4c)
    {
	if (!adhi && !adlo && cd==0x7e && !dta)
	  {
      	    ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "XG System On", len);
	    d->XG_System_On=1;
	    #ifdef tplus
	    d->vol_table = xg_vol_table;
	    #endif
	  }
	else if (adhi == 2 && adlo == 1)
	 {
	    if (dtb==8) dtb=3;
	    switch (cd)
	      {
		case 0x00:
		  d->XG_System_reverb_type=(dta<<3)+dtb;
		  break;
		case 0x20:
		  d->XG_System_chorus_type=((dta-64)<<3)+dtb;
		  break;
		case 0x40:
		  d->XG_System_variation_type=dta;
		  break;
		case 0x5a:
		  /* dta==0 Insertion; dta==1 System */
		  break;
		default: break;
	      }
	 }
	else if (adhi == 8 && cd <= 40)
	 {
	    *sysa = dta & 0x7f;
	    switch (cd)
	      {
		case 0x01: /* bank select MSB */
		  return ME_TONE_KIT;
		  break;
		case 0x02: /* bank select LSB */
		  return ME_TONE_BANK;
		  break;
		case 0x03: /* program number */
	      		/** MIDIEVENT(d->at, ME_PROGRAM, lastchan, a, 0); **/
		  return ME_PROGRAM;
		  break;
		case 0x08: /*  */
		  /* d->channel[adlo&0x0f].transpose = (char)(dta-64); */
		  d->channel[ch].transpose = (char)(dta-64);
      	    	  ctl->cmsg(CMSG_TEXT, VERB_DEBUG, "transpose channel %d by %d",
			(adlo&0x0f)+1, dta-64);
		  break;
		case 0x0b: /* volume */
		  return ME_MAINVOLUME;
		  break;
		case 0x0e: /* pan */
		  return ME_PAN;
		  break;
		case 0x12: /* chorus send */
		  return ME_CHORUSDEPTH;
		  break;
		case 0x13: /* reverb send */
		  return ME_REVERBERATION;
		  break;
		case 0x14: /* variation send */
		  break;
		case 0x18: /* filter cutoff */
		  return ME_BRIGHTNESS;
		  break;
		case 0x19: /* filter resonance */
		  return ME_HARMONICCONTENT;
		  break;
		default: break;
	      }
	  }
      return 0;
    }
  else if (id==0x41 && model==0x42 && adhi==0x12 && adlo==0x40)
    {
	if (dtc<0) return 0;
	if (!cd && dta==0x7f && !dtb && dtc==0x41)
	  {
      	    ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "GS System On", len);
	    d->GS_System_On=1;
	    #ifdef tplus
	    d->vol_table = gs_vol_table;
	    #endif
	  }
	else if (dta==0x15 && (cd&0xf0)==0x10)
	  {
	    int chan=cd&0x0f;
	    if (!chan) chan=9;
	    else if (chan<10) chan--;
	    chan = MERGE_CHANNEL_PORT(chan);
	    d->channel[chan].kit=dtb;
	  }
	else if (cd==0x01) switch(dta)
	  {
	    case 0x30:
		switch(dtb)
		  {
		    case 0: d->XG_System_reverb_type=16+0; break;
		    case 1: d->XG_System_reverb_type=16+1; break;
		    case 2: d->XG_System_reverb_type=16+2; break;
		    case 3: d->XG_System_reverb_type= 8+0; break;
		    case 4: d->XG_System_reverb_type= 8+1; break;
		    case 5: d->XG_System_reverb_type=32+0; break;
		    case 6: d->XG_System_reverb_type=8*17; break;
		    case 7: d->XG_System_reverb_type=8*18; break;
		  }
		break;
	    case 0x38:
		switch(dtb)
		  {
		    case 0: d->XG_System_chorus_type= 8+0; break;
		    case 1: d->XG_System_chorus_type= 8+1; break;
		    case 2: d->XG_System_chorus_type= 8+2; break;
		    case 3: d->XG_System_chorus_type= 8+4; break;
		    case 4: d->XG_System_chorus_type=  -1; break;
		    case 5: d->XG_System_chorus_type= 8*3; break;
		    case 6: d->XG_System_chorus_type=  -1; break;
		    case 7: d->XG_System_chorus_type=  -1; break;
		  }
		break;
	  }
      return 0;
    }
  return 0;
}

/**********************************/


/* Print a string from the file, followed by a newline. Any non-ASCII
   or unprintable characters will be converted to periods. */
static int dumpstring(uint32 len, const char *label, int type, struct md *d)
{
  char *s=(char *)safe_malloc(len+1);
  if (len != fread(s, 1, len, d->fp))
    {
      free(s);
      return -1;
    }
  s[len]='\0';
#ifdef INFO_ONLY
  if (!metatext(type, len, s, d))
#else
    if ( !(type==5 || ( d->at > 0 && (type==1||type==6||type==7) )) )
#endif
  {
   while (len--)
    {
      if (s[len]<32)
	s[len]='.';
    }
   ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "%s%s", label, s);

   if (type != 3 && d->is_open && len > 10 && strstr(s, " by ") && !strstr(s, "opyright") && !strstr(s, "(C)")) {
     if (!d->author[0]) strncpy(d->author, s, 40);
   }
   if (type == 3 && len > 6 && d->curr_track == 0 && !d->title[0] && !strstr(s, "untitled")) {
	   strncpy(d->title, s, 80);
	   d->curr_title_track = d->curr_track;
   }
  }
  free(s);
  return 0;
}

#ifdef POLYPHONY_COUNT
#define MIDIEVENT(at,t,ch,pa,pb) \
  newev=(MidiEventList *)safe_malloc(sizeof(MidiEventList)); \
  newev->event.time=at; newev->event.type=t; newev->event.channel=ch; \
  newev->event.a=pa; newev->event.b=pb; newev->next=0; newev->event.polyphony = 0; \
  return newev;
#else
#define MIDIEVENT(at,t,ch,pa,pb) \
  newev=(MidiEventList *)safe_malloc(sizeof(MidiEventList)); \
  newev->event.time=at; newev->event.type=t; newev->event.channel=ch; \
  newev->event.a=pa; newev->event.b=pb; newev->next=0; \
  return newev;
#endif

#define MAGIC_EOT ((MidiEventList *)(-1))


/* Read a MIDI event, returning a freshly allocated element that can
   be linked to the event list */
static MidiEventList *read_midi_event(struct md *d)
{
  static uint8 laststatus, lastchan;
  static uint8 nrpn=0, rpn_msb[MAXCHAN], rpn_lsb[MAXCHAN]; /* one per channel */
  uint8 me, type, a,b,c;
  int32 len, i;
  MidiEventList *newev;

  for (;;)
    {
#ifdef tplus
      if ( (len=getvl(d)) < 0) return 0;
      d->at+= len;
#else
      d->at+=getvl(d);
#endif

#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      me = (uint8)i;
#else
      if (fread(&me,1,1,d->fp)!=1)
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "\"%s\": read_midi_event: %s", 
	       d->midi_name, sys_errlist[errno]);
	  return 0;
	}
#endif
      
      if(me==0xF0 || me == 0xF7) /* SysEx event */
	{
	  int32 sret;
	  uint8 sysa=0, sysb=0, syschan=0;
#ifdef tplus
          if ( (len=getvl(d)) < 0) return 0;
#else
	  len=getvl(d);
#endif
	  sret=sysex(len, &syschan, &sysa, &sysb, d);
	  if (sret)
	   {
	     MIDIEVENT(d->at, sret, syschan, sysa, sysb);
	   }
	}
      else if(me==0xFF) /* Meta event */
	{
#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      type = (uint8)i;
#else
	  fread(&type,1,1,d->fp);
#endif
#ifdef tplus
          if ( (len=getvl(d)) < 0) return 0;
#else
	  len=getvl(d);
#endif
	  if (type>0 && type<16)
	    {
	      static const char *label[]={
		"text: ", "text: ", "Copyright: ", "track: ",
		"instrument: ", "lyric: ", "marker: ", "cue point: "};
	      dumpstring(len, label[(type>7) ? 0 : type], type, d);
	    }
	  else
	    switch(type)
	      {

	      case 0x21: /* MIDI port number */
		if(len == 1)
		{
	  	    fread(&d->midi_port_number,1,1,d->fp);
		    if(d->midi_port_number == EOF)
		    {
			    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				      "Warning: \"%s\": Short midi file.",
				      d->midi_name);
			    return 0;
		    }
		    d->midi_port_number &= 0x0f;
		    if (d->midi_port_number)
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
			  "(MIDI port number %d)", d->midi_port_number);
		    d->midi_port_number &= 0x03;
		}
		else skip(d->fp, len);
		break;

	      case 0x2F: /* End of Track */
		return MAGIC_EOT;

	      case 0x51: /* Tempo */
#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      a = (uint8)i;
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      b = (uint8)i;
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      c = (uint8)i;
#else
		fread(&a,1,1,d->fp); fread(&b,1,1,d->fp); fread(&c,1,1,d->fp);
#endif
		MIDIEVENT(d->at, ME_TEMPO, c, a, b);
		
	      default:
		ctl->cmsg(CMSG_INFO, VERB_DEBUG, 
		     "(Meta event type 0x%02x, length %ld)", type, len);
		skip(d->fp, len);
		break;
	      }
	}
      else
	{
	  a=me;
	  if (a & 0x80) /* status byte */
	    {
	      lastchan = MERGE_CHANNEL_PORT(a & 0x0F);
	      laststatus=(a>>4) & 0x07;
#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      a = (uint8)i;
#else
	      fread(&a, 1,1, d->fp);
#endif
	      a &= 0x7F;
	    }
	  switch(laststatus)
	    {
	    case 0: /* Note off */
#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      b = (uint8)i;
#else
	      fread(&b, 1,1, d->fp);
#endif
	      b &= 0x7F;
	      /*MIDIEVENT(d->at, ME_NOTEOFF, lastchan, a,b);*/
	      MIDIEVENT(d->at, ME_NOTEON, lastchan, a,0);

	    case 1: /* Note on */
#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      b = (uint8)i;
#else
	      fread(&b, 1,1, d->fp);
#endif
	      b &= 0x7F;
	      MIDIEVENT(d->at, ME_NOTEON, lastchan, a,b);

	      if (d->curr_track == d->curr_title_track && d->track_info > 1) d->title[0] = '\0';

	    case 2: /* Key Pressure */
#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      b = (uint8)i;
#else
	      fread(&b, 1,1, d->fp);
#endif
	      b &= 0x7F;
	      MIDIEVENT(d->at, ME_KEYPRESSURE, lastchan, a, b);

	    case 3: /* Control change */
#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      b = (uint8)i;
#else
	      fread(&b, 1,1, d->fp);
#endif
	      b &= 0x7F;
	      {
		int control=255;
		switch(a)
		  {
#ifdef tplus
		  case 1: control=ME_MODULATION_WHEEL; break;
		  case 5: control=ME_PORTAMENTO_TIME_MSB; break;
		  case 37: control=ME_PORTAMENTO_TIME_LSB; break;
		  case 65: control=ME_PORTAMENTO; break;
#endif
		  case 7: control=ME_MAINVOLUME; break;
		  case 10: control=ME_PAN; break;
		  case 11: control=ME_EXPRESSION; break;
#ifdef tplus
		  case 64: control=ME_SUSTAIN; b = (b >= 64); break;
#else
		  case 64: control=ME_SUSTAIN; break;
#endif
		  case 71: control=ME_HARMONICCONTENT; break;
		  case 72: control=ME_RELEASETIME; break;
		  case 73: control=ME_ATTACKTIME; break;
		  case 74: control=ME_BRIGHTNESS; break;
		  case 91: control=ME_REVERBERATION; break;
		  case 93: control=ME_CHORUSDEPTH; break;
#ifdef CHANNEL_EFFECT
		  case 94: control=ME_CELESTE; break;
ctl->cmsg(CMSG_INFO, VERB_NORMAL, "CELESTE");
		  case 95: control=ME_PHASER; break;
ctl->cmsg(CMSG_INFO, VERB_NORMAL, "PHASER");
#endif
		  case 120: control=ME_ALL_SOUNDS_OFF; break;
		  case 121: control=ME_RESET_CONTROLLERS; break;
		  case 123: control=ME_ALL_NOTES_OFF; break;

		    /* These should be the SCC-1 tone bank switch
		       commands. I don't know why there are two, or
		       why the latter only allows switching to bank 0.
		       Also, some MIDI files use 0 as some sort of
		       continuous controller. This will cause lots of
		       warnings about undefined tone banks. */
		  case 0:
		    if (d->XG_System_On)
		    	control=ME_TONE_KIT;
		    else control=ME_TONE_BANK;
		    break;
		  case 32:
	            if (d->XG_System_On) control=ME_TONE_BANK;
		    break;

		  case 100: nrpn=0; rpn_msb[lastchan]=b; break;
		  case 101: nrpn=0; rpn_lsb[lastchan]=b; break;
		  case 98: nrpn=1; rpn_lsb[lastchan]=b; break;
		  case 99: nrpn=1; rpn_msb[lastchan]=b; break;
		  case 6:
		    if (nrpn)
		      {
			if (rpn_msb[lastchan]==1) switch (rpn_lsb[lastchan])
			 {
#ifdef tplus
			   case 0x08: control=ME_VIBRATO_RATE; break;
			   case 0x09: control=ME_VIBRATO_DEPTH; break;
			   case 0x0a: control=ME_VIBRATO_DELAY; break;
#endif
			   case 0x20: control=ME_BRIGHTNESS; break;
			   case 0x21: control=ME_HARMONICCONTENT; break;
			/*
			   case 0x63: envelope attack rate
			   case 0x64: envelope decay rate
			   case 0x66: envelope release rate
			*/
			 }
			else switch (rpn_msb[lastchan])
			 {
			/*
			   case 0x14: filter cutoff frequency
			   case 0x15: filter resonance
			   case 0x16: envelope attack rate
			   case 0x17: envelope decay rate
			   case 0x18: pitch coarse
			   case 0x19: pitch fine
			*/
			   case 0x1a: d->drumvolume[lastchan][0x7f & rpn_lsb[lastchan]] = b; break;
			   case 0x1c:
			     if (!b) b=(int) (127.0*rand()/(RAND_MAX));
			     d->drumpanpot[lastchan][0x7f & rpn_lsb[lastchan]] = b;
			     break;
			   case 0x1d: d->drumreverberation[lastchan][0x7f & rpn_lsb[lastchan]] = b; break;
			   case 0x1e: d->drumchorusdepth[lastchan][0x7f & rpn_lsb[lastchan]] = b; break;
			/*
			   case 0x1f: variation send level
			*/
			 }
			ctl->cmsg(CMSG_INFO, VERB_DEBUG, 
				  "(Data entry (MSB) for NRPN %02x,%02x: %ld)",
				  rpn_msb[lastchan], rpn_lsb[lastchan],
				  b);
			break;
		      }
		    
		    switch((rpn_msb[lastchan]<<8) | rpn_lsb[lastchan])
		      {
		      case 0x0000: /* Pitch bend sensitivity */
			control=ME_PITCH_SENS;
			break;
#ifdef tplus
		      case 0x0001: control=ME_FINE_TUNING; break;
		      case 0x0002: control=ME_COARSE_TUNING; break;
#endif

		      case 0x7F7F: /* RPN reset */
			/* reset pitch bend sensitivity to 2 */
			MIDIEVENT(d->at, ME_PITCH_SENS, lastchan, 2, 0);

		      default:
			ctl->cmsg(CMSG_INFO, VERB_DEBUG, 
				  "(Data entry (MSB) for RPN %02x,%02x: %ld)",
				  rpn_msb[lastchan], rpn_lsb[lastchan],
				  b);
			break;
		      }
		    break;
		    
		  default:
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG, 
			      "(Control %d: %d)", a, b);
		    break;
		  }
		if (control != 255)
		  { 
		    MIDIEVENT(d->at, control, lastchan, b, 0); 
		  }
	      }
	      break;

	    case 4: /* Program change */
	      a &= 0x7f;
	      MIDIEVENT(d->at, ME_PROGRAM, lastchan, a, 0);

#ifdef tplus
	    case 5: /* Channel pressure */
	      MIDIEVENT(d->at, ME_CHANNEL_PRESSURE, lastchan, a, 0);
#else
	    case 5: /* Channel pressure - NOT IMPLEMENTED */
	      break;
#endif

	    case 6: /* Pitch wheel */
#ifdef tplus
      if((i = tf_getc(d)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "\"%s\": read_midi_event: %s",
			  d->midi_name, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: \"%s\": Short midi file.",
			  d->midi_name);
	    return 0;
	}
      b = (uint8)i;
#else
	      fread(&b, 1,1, d->fp);
#endif
	      b &= 0x7F;
	      MIDIEVENT(d->at, ME_PITCHWHEEL, lastchan, a, b);

	    case 7:
	      break;

	    default: 
	      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, 
		   "*** Can't happen: status 0x%02X, channel 0x%02X",
		   laststatus, lastchan);
	      break;
	    }
	}
    }
  
  return newev;
}

#undef MIDIEVENT

/* Read a midi track into the linked list, either merging with any previous
   tracks or appending to them. */
static int read_track(int append, struct md *d)
{
  MidiEventList *meep;
  MidiEventList *next, *newev;
  int32 len;
  char tmp[4];

  d->midi_port_number = 0;
  meep=d->evlist;
  if (append && meep)
    {
      /* find the last event in the list */
      for (; meep->next; meep=(MidiEventList *)meep->next)
	;
      d->at=meep->event.time;
    }
  else
    d->at=0;

  /* Check the formalities */
  
  if ((fread(tmp,1,4,d->fp) != 4) || (fread(&len,4,1,d->fp) != 1))
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	   "\"%s\": Can't read track header.", d->midi_name);
      return -1;
    }
  len=BE_LONG(len);
  if (memcmp(tmp, "MTrk", 4))
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	   "\"%s\": Corrupt MIDI file.", d->midi_name);
      return -2;
    }

  for (;;)
    {
      if (!(newev=read_midi_event(d))) /* Some kind of error  */
	return -2;

      if (newev==MAGIC_EOT) /* End-of-track Hack. */
	{
	  return 0;
	}

      next=(MidiEventList *)meep->next;
      while (next && (next->event.time < newev->event.time))
	{
	  meep=next;
	  next=(MidiEventList *)meep->next;
	}
	  
      newev->next=next;
      meep->next=newev;

      d->event_count++; /* Count the event. (About one?) */
      meep=newev;
    }
}

/* Free the linked event list from memory. */
static void free_midi_list(struct md *d)
{
  MidiEventList *meep, *next;
  if (!(meep=d->evlist)) return;
  while (meep)
    {
      next=(MidiEventList *)meep->next;
      free(meep);
      meep=next;
    }
  d->evlist=0;
}


static void xremap_percussion(int *banknumpt, int *this_notept, int this_kit) {
	int i, newmap;
	int banknum = *banknumpt;
	int this_note = *this_notept;
	int newbank, newnote;

	if (this_kit != 127 && this_kit != 126) return;

	for (i = 0; i < XMAPMAX; i++) {
		newmap = xmap[i][0];
		if (!newmap) return;
		if (this_kit == 127 && newmap != XGDRUM) continue;
		if (this_kit == 126 && newmap != SFXDRUM1) continue;
		if (xmap[i][1] != banknum) continue;
		if (xmap[i][3] != this_note) continue;
		newbank = xmap[i][2];
		newnote = xmap[i][4];
		if (newbank == banknum && newnote == this_note) return;
		if (!drumset[newbank]) return;
		*banknumpt = newbank;
		*this_notept = newnote;
		return;
	}
}

/* Allocate an array of MidiEvents and fill it from the linked list of
   events, marking used instruments for loading. Convert event times to
   samples: handle tempo changes. Strip unnecessary events from the list.
   Free the linked list. */
static void groom_list(int32 divisions, struct md *d)
{
  MidiEvent *groomed_list, *lp;
  MidiEventList *meep;
  int32 i, our_event_count, tempo, skip_this_event, new_value;
#ifdef POLYPHONY_COUNT
  uint32 current_polyphony, future_interval;
  MidiEvent *hind_list;
#endif
  int32 sample_cum, samples_to_do, st, dt, counting_time;
  uint32 our_at;
#ifdef INFO_ONLY
  struct meta_text_type *meta = meta_text_list;
#endif
  int current_bank[MAXCHAN], current_banktype[MAXCHAN], current_set[MAXCHAN],
    current_kit[MAXCHAN], current_program[MAXCHAN]; 
  int dset, dnote, drumsflag, mprog;
  /* Or should each bank have its own current program? */

  for (i=0; i<MAXCHAN; i++)
    {
      current_bank[i]=0;
      current_banktype[i]=0;
      current_set[i]=0;
      current_kit[i]=d->channel[i].kit;
      current_program[i]=default_program;
    }

  tempo=500000;
  compute_sample_increment(tempo, divisions, d);

  /* This may allocate a bit more than we need */
  groomed_list=lp=(MidiEvent *)safe_malloc(sizeof(MidiEvent) * (d->event_count+1));
  meep=d->evlist;

  our_event_count=0;
  st=our_at=sample_cum=0;
  counting_time=2; /* We strip any silence before the first NOTE ON. */

#ifdef POLYPHONY_COUNT
  current_polyphony = 0;
  hind_list = lp;
  /*future_interval = play_mode->rate / 2;*/
  future_interval = play_mode->rate / 4;
#endif

  for (i=0; i<d->event_count; i++)
    {
      skip_this_event=0;
      ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		"%6d: ch %2d: event %d (%d,%d)",
		meep->event.time, meep->event.channel + 1,
		meep->event.type, meep->event.a, meep->event.b);
#ifdef INFO_ONLY
      while (meta && meta->time <= our_at)
	{
	   meta->time = st;
	   meta = meta->next;
	}
#endif
      if (meep->event.type==ME_TEMPO)
	{
#ifndef tplus
	  tempo=
	    meep->event.channel + meep->event.b * 256 + meep->event.a * 65536;
	  compute_sample_increment(tempo, divisions, d);
#endif
	  skip_this_event=1;
	}
      else if ((quietchannels & (1<<meep->event.channel)))
	skip_this_event=1;
      else switch (meep->event.type)
	{
	case ME_PROGRAM:
	  if (!d->is_open)
	  {
	    skip_this_event=1;
	    break;
	  }
	  if (current_kit[meep->event.channel])
	    {
	      dset = meep->event.a;
	      if (current_kit[meep->event.channel]==126)
		{
		  /* note request for 2nd sfx rhythm kit */
		  if (dset && drumset[SFXDRUM2])
		  {
			 current_kit[meep->event.channel]=125;
			 current_set[meep->event.channel]=SFXDRUM2;
		         dset=new_value=SFXDRUM2;
		  }
		  else if (!dset && drumset[SFXDRUM1])
		  {
			 current_set[meep->event.channel]=SFXDRUM1;
		         dset=new_value=SFXDRUM1;
		  }
		  else
		  {
		  	ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		       		"XG SFX drum set is undefined");
			skip_this_event=1;
		  	break;
		  }
		}
	      if (drumset[dset]) /* Is this a defined drumset? */
		new_value=dset;
	      else
		{
		  ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		       "Drum set %d is undefined", dset);
		  if (drumset[0])
		      new_value=meep->event.a=0;
		  else
		    {
			skip_this_event=1;
			break;
		    }
		}
	      if (current_set[meep->event.channel] != new_value)
		current_set[meep->event.channel]=new_value;
	      else 
		skip_this_event=1;
	    }
	  else /* not percussion channel */
	    {
	      new_value=meep->event.a;
#if 0
	      if ((current_program[meep->event.channel] != SPECIAL_PROGRAM)
		  && (current_program[meep->event.channel] != new_value))
#endif
	      if (current_program[meep->event.channel] != new_value)
		current_program[meep->event.channel] = new_value;
	      else
		skip_this_event=1;
	    }
	  break;

	case ME_NOTEON:
	#ifdef POLYPHONY_COUNT
	  if (meep->event.b) current_polyphony++;
	  else current_polyphony--;
	#endif

	  if (counting_time)
	    counting_time=1;
#if 0
/* trying to play canyon.mid, but this doesn't work */
	  if (meep->event.channel == 15 && current_program[15] == SPECIAL_PROGRAM)
	    d->channel[15].kit = current_kit[15] = 127;
#endif
	  if (!d->is_open)
	    break;

	  drumsflag = current_kit[meep->event.channel];

	  if (drumsflag) /* percussion channel? */
	    {
	      dset = current_set[meep->event.channel];
	      dnote=meep->event.a;
#ifdef SFXDRUMOLD
	      if (dnote>99) dnote=0;
	      if (current_kit[meep->event.channel]==125)
		dnote=meep->event.a=sfxdrum2[dnote];
	      else if (current_kit[meep->event.channel]==126)
		dnote=meep->event.a=sfxdrum1[dnote];
#endif
	      if (d->XG_System_On) xremap_percussion(&dset, &dnote, current_kit[meep->event.channel]);

	      if (current_config_pc42b) pcmap(&dset, &dnote, &mprog, &drumsflag);

	     if (drumsflag)
	     {
	      /* Mark this instrument to be loaded */
	      if (!(drumset[dset]->tone[dnote].layer))
	       {
		drumset[dset]->tone[dnote].layer=
		    MAGIC_LOAD_INSTRUMENT;
	       }
	      else drumset[dset]->tone[dnote].last_used
		 = current_tune_number;
	      if (!d->channel[meep->event.channel].name) d->channel[meep->event.channel].name=
		    drumset[dset]->name;
	     }
	    }

	  if (!drumsflag) /* not percussion */
	    {
	      int chan=meep->event.channel;
	      int banknum;

	      if (current_banktype[chan]) banknum=SFXBANK;
	      else banknum=current_bank[chan];

	      mprog = current_program[chan];

	      if (mprog==SPECIAL_PROGRAM)
		break;

	      if (d->XG_System_On && banknum==SFXBANK && tonebank[120]) 
		      banknum = 120;

	      if (current_config_pc42b) pcmap(&banknum, &dnote, &mprog, &drumsflag);

	     if (drumsflag)
	     {
	      /* Mark this instrument to be loaded */
	      if (!(drumset[dset]->tone[dnote].layer))
	       {
		drumset[dset]->tone[dnote].layer=
		    MAGIC_LOAD_INSTRUMENT;
	       }
	      else drumset[dset]->tone[dnote].last_used
		 = current_tune_number;
	      if (!d->channel[meep->event.channel].name) d->channel[meep->event.channel].name=
		    drumset[dset]->name;
	     }
	     if (!drumsflag)
	     {
	      /* Mark this instrument to be loaded */
	      if (!(tonebank[banknum]->tone[mprog].layer))
		{
		  tonebank[banknum]->tone[mprog].layer=
		    MAGIC_LOAD_INSTRUMENT;
		}
	      else tonebank[banknum]->tone[mprog].last_used = current_tune_number;
	      if (!d->channel[meep->event.channel].name) d->channel[meep->event.channel].name=
		    tonebank[banknum]->tone[mprog].name;
	     }
	    }
	  break;

	case ME_TONE_KIT:
	  if (!meep->event.a || meep->event.a == 127)
	    {
	      new_value=meep->event.a;
	      if (current_kit[meep->event.channel] != new_value)
		current_kit[meep->event.channel]=new_value;
	      else 
		skip_this_event=1;
	      break;
	    }
	  else if (meep->event.a == 126)
	    {
	     if (d->is_open)
	     {
	      if (drumset[SFXDRUM1]) /* Is this a defined tone bank? */
	        new_value=126;
	      else
		{
	          ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		   "XG rhythm kit %d is undefined", meep->event.a);
	          skip_this_event=1;
	          break;
		}
	     }
	     current_set[meep->event.channel]=SFXDRUM1;
	     current_kit[meep->event.channel]=new_value;
	     break;
	    }
	  else if (meep->event.a != SFX_BANKTYPE)
	    {
	      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		   "XG kit %d is impossible", meep->event.a);
	      skip_this_event=1;
	      break;
	    }

	  if (current_kit[meep->event.channel])
	    {
	      skip_this_event=1;
	      break;
	    }
	  if (d->is_open)
	  {
	   if (tonebank[SFXBANK]) /* Is this a defined tone bank? */
	     new_value=SFX_BANKTYPE;
	   else 
	    {
	      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		   "XG Sfx bank is undefined");
	      skip_this_event=1;
	      break;
	    }
	  }
	  if (current_banktype[meep->event.channel]!=new_value)
	    current_banktype[meep->event.channel]=new_value;
	  else
	    skip_this_event=1;
	  break;


	case ME_TONE_BANK:
	  if (current_kit[meep->event.channel] || !d->is_open)
	    {
	      skip_this_event=1;
	      break;
	    }
	  if (d->XG_System_On && meep->event.a > 0 && meep->event.a < 48) {
	      d->channel[meep->event.channel].variationbank=meep->event.a;
	      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		   "XG variation bank %d", meep->event.a);
	      new_value=meep->event.a=0;
	  }
	  else if (tonebank[meep->event.a]) /* Is this a defined tone bank? */
	    new_value=meep->event.a;
	  else 
	    {
	      ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		   "Tone bank %d is undefined", meep->event.a);
	      new_value=meep->event.a=0;
	    }

	  if (current_bank[meep->event.channel]!=new_value)
	    current_bank[meep->event.channel]=new_value;
	  else
	    skip_this_event=1;
	  break;

	case ME_HARMONICCONTENT:
	  d->channel[meep->event.channel].harmoniccontent=meep->event.a;
	  break;
	case ME_BRIGHTNESS:
	  d->channel[meep->event.channel].brightness=meep->event.a;
	  break;

	}

      /* Recompute time in samples*/
      if ((dt=meep->event.time - our_at) && !counting_time)
	{
	  samples_to_do=d->sample_increment * dt;
	  sample_cum += d->sample_correction * dt;
	  if (sample_cum & 0xFFFF0000)
	    {
	      samples_to_do += ((sample_cum >> 16) & 0xFFFF);
	      sample_cum &= 0x0000FFFF;
	    }
	  st += samples_to_do;
	}
      else if (counting_time==1) counting_time=0;
#ifdef tplus
      if (meep->event.type==ME_TEMPO)
	{
	  tempo=
	    meep->event.channel + meep->event.b * 256 + meep->event.a * 65536;
	  compute_sample_increment(tempo, divisions, d);
	}
#endif
      if (!skip_this_event)
	{
	  /* Add the event to the list */
	  *lp=meep->event;
	  lp->time=st;
	#ifdef POLYPHONY_COUNT
	  lp->polyphony = current_polyphony;
	  while (hind_list < lp && hind_list->time <= st - future_interval)
	    {
		hind_list->polyphony = 128 + current_polyphony - hind_list->polyphony;
		hind_list++;
	    }
	#endif
	  lp++;
	  our_event_count++;
	}
      our_at=meep->event.time;
      meep=(MidiEventList *)meep->next;
    }
  /* Add an End-of-Track event */
  lp->time=st;
  lp->type=ME_EOT;
  our_event_count++;
  free_midi_list(d);
 
  /**eventsp=our_event_count;*/
  d->event_count = our_event_count;
  d->sample_count = st;
  d->event = groomed_list;
}

void read_midi_file(struct md *d)
{
  uint32 len;
  int32 divisions;
  int16 format, tracks, divisions_tmp;
  int i;
  char tmp[4];

  d->event_count=0;
  d->at=0;
  d->evlist=0;
#ifdef INFO_ONLY
  free_metatext();
#endif
  d->GM_System_On=d->GS_System_On=d->XG_System_On=0;
  d->vol_table = def_vol_table;
  d->XG_System_reverb_type=d->XG_System_chorus_type=d->XG_System_variation_type=0;
  memset(&d->drumvolume,-1,sizeof(d->drumvolume));
  memset(&d->drumchorusdepth,-1,sizeof(d->drumchorusdepth));
  memset(&d->drumreverberation,-1,sizeof(d->drumreverberation));
  memset(&d->drumpanpot,NO_PANNING,sizeof(d->drumpanpot));
  for (i=0; i<MAXCHAN; i++)
     {
	d->channel[i].transpose = 0;
	if (ISDRUMCHANNEL(i)) d->channel[i].kit = 127;
	else d->channel[i].kit = 0;
	d->channel[i].brightness = 64;
	d->channel[i].harmoniccontent = 64;
	d->channel[i].variationbank = 0;
	d->channel[i].chorusdepth = 0;
	d->channel[i].reverberation = 0;
	d->channel[i].name = 0;
     }

  if ((fread(tmp,1,4,d->fp) != 4) || (fread(&len,4,1,d->fp) != 1))
    {
      if (ferror(d->fp))
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "\"%s\": %s", d->midi_name, 
	       sys_errlist[errno]);
	}
      else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, 
	     "\"%s\": Not a MIDI file!", d->midi_name);
      return;
    }
  len=BE_LONG(len);
  if (memcmp(tmp, "MThd", 4) || len < 6)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	   "\"%s\": Not a MIDI file!", d->midi_name);
      return;
    }

  if ( fread(&format, 2, 1, d->fp) != 1 )
    {
      if (ferror(d->fp))
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "\"%s\": %s", d->midi_name, 
	       sys_errlist[errno]);
	}
      else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, 
	     "\"%s\": Not a MIDI file!", d->midi_name);
      return;
    }
  if ( fread(&tracks, 2, 1, d->fp) != 1 )
    {
      if (ferror(d->fp))
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "\"%s\": %s", d->midi_name, 
	       sys_errlist[errno]);
	}
      else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, 
	     "\"%s\": Not a MIDI file!", d->midi_name);
      return;
    }
  if ( fread(&divisions_tmp, 2, 1, d->fp) != 1 )
    {
      if (ferror(d->fp))
	{
	  ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "\"%s\": %s", d->midi_name, 
	       sys_errlist[errno]);
	}
      else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, 
	     "\"%s\": Not a MIDI file!", d->midi_name);
      return;
    }
  /* fread(&divisions_tmp, 2, 1, d->fp); */
  format=BE_SHORT(format);
  tracks=BE_SHORT(tracks);
  d->track_info = tracks;
  d->curr_track = 0;
  d->curr_title_track = -1;
  divisions_tmp=BE_SHORT(divisions_tmp);

  if (divisions_tmp<0)
    {
      /* SMPTE time -- totally untested. Got a MIDI file that uses this? */
      divisions=
	(int32)(-(divisions_tmp/256)) * (int32)(divisions_tmp & 0xFF);
    }
  else divisions=(int32)(divisions_tmp);

  if (len > 6)
    {
      ctl->cmsg(CMSG_WARNING, VERB_NORMAL, 
	   "%s: MIDI file header size %ld bytes", 
	   d->midi_name, len);
      skip(d->fp, len-6); /* skip the excess */
    }
  if (format<0 || format >2)
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, 
	   "\"%s\": Unknown MIDI file format %d", d->midi_name, format);
      return;
    }
  ctl->cmsg(CMSG_INFO, VERB_VERBOSE, 
       "Format: %d  Tracks: %d  Divisions: %d", format, tracks, divisions);

  /* Put a do-nothing event first in the list for easier processing */
  d->evlist=(MidiEventList *)safe_malloc(sizeof(MidiEventList));
  d->evlist->event.time=0;
  d->evlist->event.type=ME_NONE;
  d->evlist->next=0;
  d->event_count++;

  switch(format)
    {
    case 0:
      if (read_track(0, d))
	{
	  free_midi_list(d);
	  return;
	}
      break;

    case 1:
      for (i=0; i<tracks; i++)
	if (read_track(0, d))
	  {
	    free_midi_list(d);
	    return;
	  }
        else d->curr_track++;
      break;

    case 2: /* We simply play the tracks sequentially */
      for (i=0; i<tracks; i++)
	if (read_track(1, d))
	  {
	    free_midi_list(d);
	    return;
	  }
        else d->curr_track++;
      break;
    }
  groom_list(divisions, d);
}
