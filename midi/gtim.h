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


#include "config.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef HAVE_STRING_H
#define NO_STRING_H
#endif

#ifndef TIMID_DIR
#ifdef DEFAULT_PATH
#define TIMID_DIR  DEFAULT_PATH
#else
#define TIMID_DIR  "/usr/local/lib/timidity"
#endif
#endif

/* Enable extensions taken from TiMidity++ */
#define tplus

/* Enable Nicolas Witczak's effects processing */
#define CHANNEL_EFFECT

/* Filename extension, followed by command to run decompressor so that
   output is written to stdout. Terminate the list with a 0. 

   Any file with a name ending in one of these strings will be run
   through the corresponding decompressor. If you don't like this
   behavior, you can undefine DECOMPRESSOR_LIST to disable automatic
   decompression entirely. 

   This is currently ignored for Win32. */
#define DECOMPRESSOR_LIST { \
			      ".gz", "gunzip -c %s", \
			      ".Z", "zcat %s", \
			      ".zip", "unzip -p %s", \
			      ".lha", "lha -pq %s", \
			      ".lzh", "lha -pq %s", \
			      ".shn", "shorten -x %s -", \
			      ".wav", "wav2pat %s", \
			     0 }

/* When a patch file can't be opened, one of these extensions is
   appended to the filename and the open is tried again.

   This is ignored for Win32, which uses only ".pat" (see the bottom
   of this file if you need to change this.) */
#define PATCH_EXT_LIST { \
			   ".pat", \
			   ".shn", ".pat.shn", \
			   ".gz", ".pat.gz", \
			   0 }

/* Acoustic Grand Piano seems to be the usual default instrument. */
#define DEFAULT_PROGRAM 0

/* 9 here is MIDI channel 10, which is the standard percussion channel.
   Some files (notably C:\WINDOWS\CANYON.MID) think that 16 is one too. 
   On the other hand, some files know that 16 is not a drum channel and
   try to play music on it. This is now a runtime option, so this isn't
   a critical choice anymore. */
#define DEFAULT_DRUMCHANNELS (1<<9)
/*#define DEFAULT_DRUMCHANNELS ((1<<9) | (1<<15))*/

/* type of floating point number */
typedef double FLOAT_T;
/* typedef float FLOAT_T; */

/* A somewhat arbitrary frequency range. The low end of this will
   sound terrible as no lowpass filtering is performed on most
   instruments before resampling. */
#define MIN_OUTPUT_RATE 	4000
#define MAX_OUTPUT_RATE 	65000

/* In percent. */
#ifndef DEFAULT_AMPLIFICATION
#define DEFAULT_AMPLIFICATION 	70
#endif

/* Default sampling rate, default polyphony, and maximum polyphony.
   All but the last can be overridden from the command line. */
#ifndef DEFAULT_RATE
/*#define DEFAULT_RATE	32000*/
#define DEFAULT_RATE	44100
#endif

#ifndef DEFAULT_VOICES
#ifdef ADAGIO
#define DEFAULT_VOICES	DEFAULT_DSPVOICES
#else
#define DEFAULT_VOICES	256
#endif
#endif

#ifndef MAX_VOICES
#ifdef ADAGIO
/* Should check this.  MAX_DSPVOICES should be the
   number of Timidity "channels", but the maximum number
   of polyphonic voices playing simultaneously should be
   larger.  Now the two maxima are treated the same.
*/
#define MAX_VOICES	MAX_DSPVOICES
#else
#define MAX_VOICES	256
#endif
#endif

/* The size of the internal buffer is 2^AUDIO_BUFFER_BITS samples.
   This determines maximum number of samples ever computed in a row.

   For Linux and FreeBSD users:
   
   This also specifies the size of the buffer fragment.  A smaller
   fragment gives a faster response in interactive mode -- 10 or 11 is
   probably a good number. Unfortunately some sound cards emit a click
   when switching DMA buffers. If this happens to you, try increasing
   this number to reduce the frequency of the clicks.

   For other systems:
   
   You should probably use a larger number for improved performance.

*/
#define AUDIO_BUFFER_BITS 10

/* 1000 here will give a control ratio of 22:1 with 22 kHz output.
   Higher CONTROLS_PER_SECOND values allow more accurate rendering
   of envelopes and tremolo. The cost is CPU time. */
/*#define CONTROLS_PER_SECOND 1000*/
#define CONTROLS_PER_SECOND 2000

/* Make envelopes twice as fast. Saves ~20% CPU time (notes decay
   faster) and sounds more like a GUS. There is now a command line
   option to toggle this as well. */
/*#define FAST_DECAY*/

/* How many bits to use for the fractional part of sample positions.
   This affects tonal accuracy. The entire position counter must fit
   in 32 bits, so with FRACTION_BITS equal to 12, the maximum size of
   a sample is 1048576 samples (2 megabytes in memory). The GUS gets
   by with just 9 bits and a little help from its friends...
   "The GUS does not SUCK!!!" -- a happy user :) */
/* #define FRACTION_BITS 12 */
#define FRACTION_BITS 13

/* For some reason the sample volume is always set to maximum in all
   patch files. Define this for a crude adjustment that may help
   equalize instrument volumes. */
#define ADJUST_SAMPLE_VOLUMES

/* If you have root access, you can define DANGEROUS_RENICE and chmod
   timidity setuid root to have it automatically raise its priority
   when run -- this may make it possible to play MIDI files in the
   background while running other CPU-intensive jobs. Of course no
   amount of renicing will help if the CPU time simply isn't there.

   The root privileges are used and dropped at the beginning of main()
   in timidity.c -- please check the code to your satisfaction before
   using this option. (And please check sections 11 and 12 in the
   GNU General Public License (under GNU Emacs, hit ^H^W) ;) */
/* #define DANGEROUS_RENICE -15 */

/* The number of samples to use for ramping out a dying note. Affects
   click removal. */
#define MAX_DIE_TIME 20

/* On some machines (especially PCs without math coprocessors),
   looking up sine values in a table will be significantly faster than
   computing them on the fly. Uncomment this to use lookups. */
/* #define LOOKUP_SINE */

/* If calling ldexp() is faster than a floating point multiplication
   on your machine/compiler/libm, uncomment this. It doesn't make much
   difference either way, but hey -- it was on the TODO list, so it
   got done. */
/* #define USE_LDEXP */

/**************************************************************************/
/* Anything below this shouldn't need to be changed unless you're porting
   to a new machine with other than 32-bit, big-endian words. */
/**************************************************************************/

/* change FRACTION_BITS above, not these */
#define INTEGER_BITS (32 - FRACTION_BITS)
#define INTEGER_MASK (0xFFFFFFFF << FRACTION_BITS)
#define FRACTION_MASK (~ INTEGER_MASK)

/* This is enforced by some computations that must fit in an int */
#define MAX_CONTROL_RATIO 255

/* Audio buffer size has to be a power of two to allow DMA buffer
   fragments under the VoxWare (Linux & FreeBSD) audio driver */
#define AUDIO_BUFFER_SIZE (1<<AUDIO_BUFFER_BITS)
#define BB_SIZE (AUDIO_BUFFER_SIZE*1024)

/* Byte order, defined in <machine/endian.h> for FreeBSD and DEC OSF/1 */
#ifdef __osf__
#include <machine/endian.h>
#endif

#ifdef __linux__
/*
 * Byte order is defined in <bytesex.h> as __BYTE_ORDER, that need to
 * be checked against __LITTLE_ENDIAN and __BIG_ENDIAN defined in <endian.h>
 * <endian.h> includes automagically <bytesex.h>
 * for Linux.
 */
#include <endian.h>

/*
 * We undef the two things to start with a clean situation
 * (oddly enough, <endian.h> defines under certain conditions
 * the two things below, as __LITTLE_ENDIAN and __BIG_ENDIAN, that
 * are useless for our plans)
 */
#undef LITTLE_ENDIAN
#undef BIG_ENDIAN

# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define LITTLE_ENDIAN
# elif __BYTE_ORDER == __BIG_ENDIAN
#  define BIG_ENDIAN
# else
# error No byte sex defined
# endif
# include <features.h>
# if (__GLIBC__ >= 2)
#  include <errno.h> /* needed for glibc 2 */
#  include <math.h>
#  define PI M_PI
#  include <stdio.h>
# endif        
#endif /* linux */

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/types.h>
#include <errno.h>
#include <machine/endian.h>
#if BYTE_ORDER == LITTLE_ENDIAN
#undef BIG_ENDIAN
#undef PDP_ENDIAN
#elif BYTE_ORDER == BIG_ENDIAN
#undef LITTLE_ENDIAN
#undef PDP_ENDIAN
#else
# error No valid byte sex defined
#endif
#define USE_LDEXP
#define PI M_PI
#endif /* __FreeBSD__ */

#ifdef _UNIXWARE
#undef BIG_ENDIAN
#define LITTLE_ENDIAN
#endif

/* Win32 on Intel machines */
#ifdef __WIN32__
#  define LITTLE_ENDIAN
#endif

/* DEC MMS has 64 bit long words */
#ifdef __osf__
typedef unsigned int uint32;
typedef int int32; 
#else
typedef unsigned long uint32;
typedef long int32; 
#endif
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char uint8;
typedef char int8;

/* Instrument files are little-endian, MIDI files big-endian, so we
   need to do some conversions. */

#define XCHG_SHORT(x) ((((x)&0xFF)<<8) | (((x)>>8)&0xFF))
#ifdef __i486__
# define XCHG_LONG(x) \
     ({ int32 __value; \
        asm ("bswap %1; movl %1,%0" : "=g" (__value) : "r" (x)); \
       __value; })
#else
# define XCHG_LONG(x) ((((x)&0xFF)<<24) | \
		      (((x)&0xFF00)<<8) | \
		      (((x)&0xFF0000)>>8) | \
		      (((x)>>24)&0xFF))
#endif

#ifdef LITTLE_ENDIAN
#define LE_SHORT(x) x
#define LE_LONG(x) x
#ifdef __FreeBSD__
#define BE_SHORT(x) __byte_swap_word(x)
#define BE_LONG(x) __byte_swap_long(x)
#else
#define BE_SHORT(x) XCHG_SHORT(x)
#define BE_LONG(x) XCHG_LONG(x)
#endif
#else
#define BE_SHORT(x) x
#define BE_LONG(x) x
#ifdef __FreeBSD__
#define LE_SHORT(x) __byte_swap_word(x)
#define LE_LONG(x) __byte_swap_long(x)
#else
#define LE_SHORT(x) XCHG_SHORT(x)
#define LE_LONG(x) XCHG_LONG(x)
#endif
#endif

#define MAX_AMPLIFICATION 800

/* You could specify a complete path, e.g. "/etc/timidity.cfg", and
   then specify the library directory in the configuration file. */
#ifndef CONFIG_FILE
#define CONFIG_FILE DEFAULT_PATH##"/timidity.cfg"
#endif

/* These affect general volume */
#define GUARD_BITS 3
#define AMP_BITS (15-GUARD_BITS)

#ifdef LOOKUP_HACK
   typedef int8 sample_t;
   typedef uint8 final_volume_t;
#  define FINAL_VOLUME(v) (~_l2u[v])
#  define MIXUP_SHIFT 5
#  define MAX_AMP_VALUE 4095
#else
   typedef int16 sample_t;
   typedef int32 final_volume_t;
#  define FINAL_VOLUME(v) (v)
#  define MAX_AMP_VALUE ((1<<(AMP_BITS+1))-1)
#endif

#ifdef tplus
/* #define MIN_AMP_VALUE (MAX_AMP_VALUE >> 9) */
#define MIN_AMP_VALUE (MAX_AMP_VALUE >> 10)
#endif

#ifdef USE_LDEXP
#  define FRSCALE(a,b) ldexp((double)(a),(b))
#  define FRSCALENEG(a,b) ldexp((double)(a),-(b))
#else
#  define FRSCALE(a,b) ((a) * (double)(1<<(b)))
#  define FRSCALENEG(a,b) ((a) * (1.0L / (double)(1<<(b))))
#endif

/* Vibrato and tremolo Choices of the Day */
#define SWEEP_TUNING 38
#define VIBRATO_AMPLITUDE_TUNING 1.0L
#define VIBRATO_RATE_TUNING 38
#define TREMOLO_AMPLITUDE_TUNING 1.0L
#define TREMOLO_RATE_TUNING 38

#define SWEEP_SHIFT 16
#define RATE_SHIFT 5

#define VIBRATO_SAMPLE_INCREMENTS 32


#ifdef tplus
#define MODULATION_WHEEL_RATE (1.0/6.0)
#define VIBRATO_DEPTH_TUNING (1.0/4.0)
#endif

#if defined(hpux) || defined(__hpux)
  extern char *sys_errlist[];
  #define PI 3.14159265358979323846
#endif

#ifdef sun
  #ifdef HAVE_SYS_STDTYPES_H
    #include <sys/stdtypes.h>
  #endif
  #include <errno.h>
  extern char *sys_errlist[];
  extern int opterr;
  extern int optind;
  extern int optopt;
  extern char *optarg;
  #define PI 3.14159265358979323846
  #define rindex(s,c) strrchr(s,c)
#endif

#ifdef _UNIXWARE
extern char *sys_errlist[];
#include <values.h>
#define PI M_PI
#include <sys/filio.h>
#endif


/* The path separator (D.M.) */
#  define PATH_SEP '/'
#  define PATH_STRING "/"

#ifdef __osf__
  #include <errno.h>
  #define PI 3.14159265358979323846
#endif

#ifdef __FreeBSD__
#include <math.h>
#ifndef PI
#define PI M_PI
#endif
#endif

#ifdef _SCO_DS
#include <math.h>
#ifndef PI
#define PI M_PI
#endif
#endif
