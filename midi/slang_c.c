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

    slang_c.c, Riccardo Facchetti (riccardo@cdc8g5.cdc.polimi.it)

      based on ncurses_ctl.c
      slang library is more efficient than ncurses one.

    04/04/1995
      - Initial, working version.

    15/04/1995
      - Works with no-trace playing too; not the best way, but
        it is the only way: slang 0.99.1 don't have a window management interface
        and I don't want write one for it! :)
        The problem is that I have set the no-scroll slang option so
        when there are too much messages, the last ones are not displayed at
        all. Tipically the last messages are warning for instruments not found
        so this is no real problem (I hope :)
      - Get the real size of screen we are running on

    */
#ifdef IA_SLANG

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "gtim.h"

#ifdef HAVE_SLANG_SLANG_H
#include <slang/slang.h>
#else
#ifdef HAVE_SLANG_H
#include <slang.h>
#else
#include <slang.h>
#endif
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif


#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "version.h"

#define NCCC (char *)
/*
 * For the set_color pairs (so called 'objects')
 * see the ctl_open()
 */
#define SLsmg_normal()                  SLsmg_set_color(20)
#define SLsmg_bold()           		SLsmg_set_color(21)
#define SLsmg_reverse()                 SLsmg_set_color(22)

static void ctl_refresh(void);
static void ctl_help_mode(void);
static void ctl_total_time(uint32 tt);
static void ctl_master_volume(int mv);
static void ctl_file_name(char *name);
static void ctl_current_time(uint32 ct);
static void ctl_note(int v);
static void ctl_note_display(void);
static void ctl_program(int ch, int val, const char *name);
static void ctl_volume(int ch, int val);
static void ctl_expression(int ch, int val);
static void ctl_panning(int ch, int val);
static void ctl_sustain(int ch, int val);
static void ctl_pitch_bend(int ch, int val);
static void ctl_reset(void);
static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, const char *fmt, ...);

/**********************************************/
/* export the interface functions */

#undef ctl
#define ctl slang_control_mode

ControlMode ctl= 
{
  "slang interface", 's',
  1,0,0,
  ctl_open, dumb_pass_playing_list, ctl_close, ctl_read, cmsg,
  ctl_refresh, ctl_reset, ctl_file_name, ctl_total_time, ctl_current_time, 
  ctl_note, 
  ctl_master_volume, ctl_program, ctl_volume, 
  ctl_expression, ctl_panning, ctl_sustain, ctl_pitch_bend
};


/***********************************************************************/
/* foreground/background checks disabled since switching to curses */
/* static int in_foreground=1; */
static int ctl_helpmode=0;

static int msg_row = 0;
static uint32 current_centiseconds = 0;
static int songoffset = 0, current_voices = 0;

extern MidiEvent *current_event;

typedef struct {
  uint8 status, channel, note, velocity, voices;
  uint32 time;
} OldVoice;
#define MAX_OLD_NOTES 500
static OldVoice old_note[MAX_OLD_NOTES];
static int leading_pointer=0, trailing_pointer=0;

static int tr_lm, tr_nw;
#define MAX_PNW 12
static char patch_name[16][MAX_PNW];

static void _ctl_refresh(void)
{
  SLsmg_gotorc(0,0);
  SLsmg_refresh();
}

static void SLsmg_printfrc( int r, int c, const char *fmt, ...) {
  char p[1000];
  va_list ap;

  SLsmg_gotorc( r, c);
  va_start(ap, fmt);
  vsprintf(p, fmt, ap);
  va_end(ap);

  SLsmg_write_string (p);
}

static void ctl_head(void)
{
  SLsmg_printfrc(0, 0, "TiMidity v" TIMID_VERSION);
  SLsmg_printfrc(0,SLtt_Screen_Cols-52, "(C) 1995 Tuukka Toivonen <toivonen@clinet.fi>");
  SLsmg_printfrc(1,0, "Press 'h' for help with keys, or 'q' to quit.");
}
static void ctl_refresh(void)
{
  if (ctl.trace_playing)
    {
       ctl_current_time(0);
       ctl_note_display();
       SLsmg_gotorc(4,48);
       /*SLsmg_bold();*/
       SLsmg_printf(NCCC"%2d", current_voices);
       /*SLsmg_normal();*/
       _ctl_refresh();
    }
}

static void ctl_help_mode(void)
{
  if (ctl_helpmode)
    {
      ctl_helpmode=0;

/*
 * Clear the head zone and reprint head message.
 */
      SLsmg_gotorc(0,0);
      SLsmg_erase_eol();
      SLsmg_gotorc(1,0);
      SLsmg_erase_eol();
        ctl_head();
      _ctl_refresh();
    }
  else
    {
      ctl_helpmode=1;
        SLsmg_reverse();
      SLsmg_gotorc(0,0);
      SLsmg_erase_eol();
      SLsmg_write_string(
            NCCC"V=Louder    b=Skip back      "
            "n=Next file      r=Restart file");

      SLsmg_gotorc(1,0);
      SLsmg_erase_eol();
      SLsmg_write_string(
            NCCC"v=Softer    f=Skip forward   "
            "p=Previous file  q=Quit program");
      SLsmg_normal();
      _ctl_refresh();
    }
}

static void ctl_total_time(uint32 tt)
{
  int mins, secs=(int)tt/play_mode->rate;
  mins=secs/60;
  secs-=mins*60;

  SLsmg_gotorc(4,6+6+3);
  SLsmg_bold();
  SLsmg_printf(NCCC"%3d:%02d", mins, secs);
  SLsmg_normal();
  _ctl_refresh();

  songoffset = 0;
  current_centiseconds = 0;
  for (secs=0; secs<16; secs++) patch_name[secs][0] = '\0';
}

static void ctl_master_volume(int mv)
{
  SLsmg_gotorc(4,SLtt_Screen_Cols-5);
  SLsmg_bold();
  SLsmg_printf(NCCC"%03d %%", mv);
  SLsmg_normal();
  _ctl_refresh();
}

static void ctl_file_name(char *name)
{
  SLsmg_gotorc(3,6);
  SLsmg_erase_eol();
  SLsmg_bold();
  SLsmg_write_string(name);
  SLsmg_normal();
  _ctl_refresh();
}

static void ctl_current_time(uint32 ct)
{
  /*int i,v;*/
  int centisecs, realct;
  int mins, secs;

  if (!ctl.trace_playing) 
    return;

  realct = play_mode->output_count(ct);
  if (realct < 0) realct = 0;
  else realct += songoffset;
  centisecs = realct / (play_mode->rate/100);
  current_centiseconds = (uint32)centisecs;

  secs=centisecs/100;
  mins=secs/60;
  secs-=mins*60;
  SLsmg_gotorc(4,6);
  SLsmg_bold();
  SLsmg_printf(NCCC"%3d:%02d", mins, secs);
/*
  v=0;
  i=voices;
  while (i--)
    if (voice[i].status!=VOICE_FREE) v++;
  SLsmg_gotorc(4,48);
  SLsmg_printf(NCCC"%2d", v);
*/
  SLsmg_normal();
  _ctl_refresh();
}

static void ctl_note(int v)
{
  int i, n;
  if (!ctl.trace_playing) 
    return;
  if (voice[v].clone_type != 0) return;

  old_note[leading_pointer].status = voice[v].status;
  old_note[leading_pointer].channel = voice[v].channel;
  old_note[leading_pointer].note = voice[v].note;
  old_note[leading_pointer].velocity = voice[v].velocity;
  old_note[leading_pointer].time = current_event->time / (play_mode->rate/100);
  n=0;
  i=voices;
  while (i--)
    if (voice[i].status!=VOICE_FREE) n++;
  old_note[leading_pointer].voices = n;
  leading_pointer++;
  if (leading_pointer == MAX_OLD_NOTES) leading_pointer = 0;

}

#define LOW_CLIP 32
#define HIGH_CLIP 59

static void ctl_note_display(void)
{
  int v = trailing_pointer;
  uint32 then;

  if (tr_nw < 10) return;

  then = old_note[v].time;

  while (then <= current_centiseconds && v != leading_pointer)
    {
  	int xl, new_note;
	new_note = old_note[v].note - LOW_CLIP;
	if (new_note < 0) new_note = 0;
	if (new_note > HIGH_CLIP) new_note = HIGH_CLIP;
	xl = (new_note * tr_nw) / (HIGH_CLIP+1);
	SLsmg_gotorc(8+(old_note[v].channel&0x0f), xl + tr_lm);
	switch(old_note[v].status)
	  {
	    case VOICE_DIE:
	      SLsmg_write_char(',');
	      break;
	    case VOICE_FREE: 
	      SLsmg_write_char('.');
	      break;
	    case VOICE_ON:
	      SLsmg_bold();
	      SLsmg_write_char('0'+(10*old_note[v].velocity)/128); 
	      SLsmg_normal();
	      break;
	    case VOICE_OFF:
	    case VOICE_SUSTAINED:
	      SLsmg_write_char('0'+(10*old_note[v].velocity)/128);
	      break;
	  }
	current_voices = old_note[v].voices;
	v++;
	if (v == MAX_OLD_NOTES) v = 0;
	then = old_note[v].time;
    }
  trailing_pointer = v;
}

static void ctl_program(int ch, int val, const char *name)
{
  int realch = ch;
  if (!ctl.trace_playing) 
    return;
  ch &= 0x0f;

  if (name && realch<16)
    {
      strncpy(patch_name[ch], name, MAX_PNW);
      patch_name[ch][MAX_PNW-1] = '\0';
    }
  if (tr_lm > 3)
    {
      SLsmg_gotorc(8+ch, 3);
      if (patch_name[ch][0])
        SLsmg_printf(NCCC"%-12s", patch_name[ch]);
      else SLsmg_write_string(NCCC"            ");
    }


  SLsmg_gotorc(8+ch, SLtt_Screen_Cols-20);
  if (channel[ch].kit)
    {
      SLsmg_bold();
      SLsmg_printf(NCCC"%03d", val);
      SLsmg_normal();
    }
  else
    SLsmg_printf(NCCC"%03d", val);
}

static void ctl_volume(int ch, int val)
{
  if (!ctl.trace_playing) 
    return;
  ch &= 0x0f;
  SLsmg_gotorc(8+ch, SLtt_Screen_Cols-16);
  SLsmg_printf(NCCC"%3d", (val*100)/127);
}

static void ctl_expression(int ch, int val)
{
  if (!ctl.trace_playing) 
    return;
  ch &= 0x0f;
  SLsmg_gotorc(8+ch, SLtt_Screen_Cols-12);
  SLsmg_printf(NCCC"%3d", (val*100)/127);
}

static void ctl_panning(int ch, int val)
{
  if (!ctl.trace_playing) 
    return;
  ch &= 0x0f;
  SLsmg_gotorc(8+ch, SLtt_Screen_Cols-8);
  if (val==NO_PANNING)
    SLsmg_write_string(NCCC"   ");
  else if (val<5)
    SLsmg_write_string(NCCC" L ");
  else if (val>123)
    SLsmg_write_string(NCCC" R ");
  else if (val>60 && val<68)
    SLsmg_write_string(NCCC" C ");
  else
    {
      /* wprintw(dftwin, "%+02d", (100*(val-64))/64); */
      val = (100*(val-64))/64; /* piss on curses */
      if (val<0)
      {
        SLsmg_write_char('-');
        val=-val;
      }
      else SLsmg_write_char('+');
      SLsmg_printf(NCCC"%02d", val);
    }
}

static void ctl_sustain(int ch, int val)
{
  if (!ctl.trace_playing) 
    return;
  ch &= 0x0f;
  SLsmg_gotorc(8+ch, SLtt_Screen_Cols-4);
  if (val) SLsmg_write_char('S');
  else SLsmg_write_char(' ');
}

static void ctl_pitch_bend(int ch, int val)
{
  if (!ctl.trace_playing) 
    return;
  ch &= 0x0f;
  SLsmg_gotorc(8+ch, SLtt_Screen_Cols-2);
  if (val>0x2000) SLsmg_write_char('+');
  else if (val<0x2000) SLsmg_write_char('-');
  else SLsmg_write_char(' ');
}

static void ctl_reset(void)
{
  int i,j;
  if (!ctl.trace_playing) 
    return;
  for (i=0; i<16; i++)
    {
      SLsmg_gotorc(8+i, tr_lm);
      if (tr_nw >= 10)
        for (j=0; j<tr_nw; j++) SLsmg_write_char('.');
      ctl_program(i, channel[i].program, channel[i].name);
      ctl_volume(i, channel[i].volume);
      ctl_expression(i, channel[i].expression);
      ctl_panning(i, channel[i].panning);
      ctl_sustain(i, channel[i].sustain);
      ctl_pitch_bend(i, channel[i].pitchbend);
    }
  for (i=0; i<MAX_OLD_NOTES; i++)
    {
      old_note[i].time = 0;
    }
  leading_pointer = trailing_pointer = current_voices = 0;
  _ctl_refresh();
}

/***********************************************************************/


static int ctl_open(int using_stdin, int using_stdout)
{
#ifdef TIOCGWINSZ
  struct winsize size;
#endif
  int i;
  int save_lines, save_cols;

  SLtt_get_terminfo();
/*
 * Save the terminfo values for lines and cols
 * then detect the real values.
 */
  save_lines = SLtt_Screen_Rows;
  save_cols = SLtt_Screen_Cols;
#ifdef TIOCGWINSZ
  if (!ioctl(0, TIOCGWINSZ, &size)) {
    SLtt_Screen_Cols=size.ws_col;
    SLtt_Screen_Rows=size.ws_row;
  } else
#endif
  {
    SLtt_Screen_Cols=atoi(getenv("COLUMNS"));
    SLtt_Screen_Rows=atoi(getenv("LINES"));
  }
  if (!SLtt_Screen_Cols || !SLtt_Screen_Rows) {
    SLtt_Screen_Rows = save_lines;
      SLtt_Screen_Cols = save_cols;
  }

  tr_lm = 3;
  tr_nw = SLtt_Screen_Cols - 25;
  if (tr_nw > MAX_PNW + 1 + 20)
    {
	tr_nw -= MAX_PNW + 1;
	tr_lm += MAX_PNW + 1;
    }

  SLang_init_tty(7, 0, 0);
  SLsmg_init_smg();
  SLtt_set_color (20, NCCC"Normal", NCCC"lightgray", NCCC"black");
  SLtt_set_color (21, NCCC"HighLight", NCCC"white", NCCC"black");
  SLtt_set_color (22, NCCC"Reverse", NCCC"black", NCCC"white");
  SLtt_Use_Ansi_Colors = 1;
  SLtt_Term_Cannot_Scroll = 1;

  ctl.opened=1;

  SLsmg_cls();
 
  ctl_head();

  SLsmg_printfrc(3,0, "File:");
  if (ctl.trace_playing)
    {
      SLsmg_printfrc(4,0, "Time:");
      SLsmg_gotorc(4,6+6+1);
      SLsmg_write_char('/');
      SLsmg_gotorc(4,40);
      SLsmg_printf(NCCC"Voices:    / %d", voices);
    }
  else
    {
      SLsmg_printfrc(4,0, "Playing time:");
    }
  SLsmg_printfrc(4,SLtt_Screen_Cols-20, "Master volume:");
  SLsmg_gotorc(5,0);
  for (i=0; i<SLtt_Screen_Cols; i++)
    SLsmg_write_char('_');
  if (ctl.trace_playing)
    {
      SLsmg_printfrc(6,0, "Ch");
      SLsmg_printfrc(6,SLtt_Screen_Cols-20, "Prg Vol Exp Pan S B");
      SLsmg_gotorc(7,0);
      for (i=0; i<SLtt_Screen_Cols; i++)
      SLsmg_write_char('-');
      for (i=0; i<16; i++)
      {
        SLsmg_printfrc(8+i, 0, "%02d", i+1);
      }
    }
  else
    msg_row = 6;
  _ctl_refresh();
  
  return 0;
}

static void ctl_close(void)
{
  if (ctl.opened)
    {
        SLsmg_normal();
        SLsmg_gotorc(SLtt_Screen_Rows - 1, 0);
        SLsmg_refresh();
        SLsmg_reset_smg();
        SLang_reset_tty();
      ctl.opened=0;
    }
}

static int ctl_read(int32 *valp)
{
  int c;
  if (last_rc_command)
    {
	*valp = last_rc_arg;
	c = last_rc_command;
	last_rc_command = 0;
	return c;
    }
  if (!SLang_input_pending(0))
    return RC_NONE;

  c=SLang_getkey();
    switch(c)
      {
      case 'h':
      case '?':
        ctl_help_mode();
        return RC_NONE;
        
      case 'V':
        *valp=10;
        return RC_CHANGE_VOLUME;
      case 'v':
        *valp=-10;
        return RC_CHANGE_VOLUME;
      case 'q':
        return RC_QUIT;
      case 'n':
        return RC_NEXT;
      case 'p':
        return RC_REALLY_PREVIOUS;
      case 'r':
        return RC_RESTART;

      case 'f':
        *valp=play_mode->rate;
        return RC_FORWARD;
      case 'b':
        *valp=play_mode->rate;
        return RC_BACK;
        /* case ' ':
           return RC_TOGGLE_PAUSE; */
      }
  return RC_NONE;
}

static int cmsg(int type, int verbosity_level, const char *fmt, ...)
{
  va_list ap;
  char p[1000];
  int flagnl = 1;
  static int msg_col = 0;
  if (*fmt == '~')
    {
      flagnl = 0;
      fmt++;
    }
  if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
      ctl.verbosity<verbosity_level)
    return 0;
  va_start(ap, fmt);
  if (!ctl.opened)
    {
      vfprintf(stderr, fmt, ap);
      fprintf(stderr, "\n");
    }
  else if (ctl.trace_playing)
    {
      switch(type)
      {
        /* Pretty pointless to only have one line for messages, but... */
      case CMSG_WARNING:
      case CMSG_ERROR:
      case CMSG_FATAL:
        SLsmg_gotorc(2,0);
      SLsmg_erase_eol();
      SLsmg_bold();
        vsprintf(p, fmt, ap);
        SLsmg_write_string(p);
      SLsmg_normal();
        _ctl_refresh();
        if (type==CMSG_WARNING)
          sleep(1); /* Don't you just _HATE_ it when programs do this... */
        else
          sleep(2);
        SLsmg_gotorc(2,0);
      SLsmg_erase_eol();
        _ctl_refresh();
        break;
      }
    }
  else
    {
      if (msg_row > SLtt_Screen_Rows-1)
       {
	int i;
	for (i = 6; i < SLtt_Screen_Rows; i++)
	 {
	   SLsmg_gotorc(i,0);
           SLsmg_erase_eol();
	 }
	msg_row = 6;
       }
      SLsmg_gotorc(msg_row,msg_col);
      switch(type)
      {
      default:
        vsprintf(p, fmt, ap);
        SLsmg_write_string(p);
	msg_col += strlen(p);
        _ctl_refresh();
        break;

      case CMSG_WARNING:
        SLsmg_bold();
        vsprintf(p, fmt, ap);
        SLsmg_write_string(p);
	msg_col += strlen(p);
        SLsmg_normal();
        _ctl_refresh();
        break;
        
      case CMSG_ERROR:
      case CMSG_FATAL:
        SLsmg_bold();
        vsprintf(p, fmt, ap);
        SLsmg_write_string(p);
	msg_col += strlen(p);
        SLsmg_normal();
        _ctl_refresh();
        if (type==CMSG_FATAL)
          sleep(2);
        break;
      }
      if (flagnl)
       {
	 msg_row++;
	 msg_col = 0;
       }
    }

  va_end(ap);
  return 0;
}

#endif
