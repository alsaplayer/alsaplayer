/*
	$Id$
*/

/* #if defined(__linux__) || defined(__FreeBSD__) || defined(sun) */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef AU_OSS
#ifdef __linux__
#include <sys/ioctl.h> /* new with 1.2.0? Didn't need this under 1.1.64 */
#include <linux/soundcard.h>
#endif

#ifdef __FreeBSD__
#include <machine/soundcard.h>
#endif
#endif

#include <malloc.h>
#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"

#define PRESUMED_FULLNESS 20

/* #define BB_SIZE (AUDIO_BUFFER_SIZE*128) */
/* #define BB_SIZE (AUDIO_BUFFER_SIZE*256) */
static unsigned char *bbuf = 0;
static int bboffset = 0;
int bbcount = 0;
static int outchunk = 0;
static int starting_up = 1, flushing = 0;
static int out_count = 0;
static int total_bytes = 0;

/*
#if defined(AU_OSS) || defined(AU_SUN) || defined(AU_BSDI) || defined(AU_ESD)
#define WRITEDRIVER(fd,buf,cnt) write(fd,buf,cnt)
#else
*/
#define WRITEDRIVER(fd,buf,cnt) play_mode->driver_output_data(buf,cnt)
/*
#endif
*/


int plug_output(unsigned char *buf) {
	int ret = output_fragsize;

	if (buf && bbuf && bbcount >= ret) {
		memcpy(buf, bbuf + bboffset, ret);
		out_count += ret;
		bboffset += ret;
		bbcount -= ret;
	}
	else ret = 0;
if (!ret) fprintf(stderr,"something's wrong\n");

	return ret;
}


int b_out_count()
{
  return out_count;
}

void b_out(char id, int fd, int *buf, int ocount)
{
  int ret;
  uint32 ucount;

  if (ocount < 0) {
	out_count = bboffset = bbcount = outchunk = 0;
	starting_up = 1;
	flushing = 0;
	output_buffer_full = PRESUMED_FULLNESS;
	total_bytes = 0;
	return;
  }

  ucount = (uint32)ocount;

  if (!bbuf) {
    bbcount = bboffset = 0;
    bbuf = (unsigned char *)malloc(BB_SIZE);
    if (!bbuf) {
	    fprintf(stderr, "malloc output error");
	    exit(1);
    }
  }

  if (!total_bytes) {
    if (output_fragsize > 0) outchunk = output_fragsize;
    if (output_frags > 0) total_bytes = output_frags * outchunk;
    if (!total_bytes) total_bytes = AUDIO_BUFFER_SIZE*2;
  }

  if (ucount && !outchunk) outchunk = ucount;
  if (starting_up && ucount + bboffset + bbcount >= BB_SIZE) starting_up = 0;
  if (!ucount) { starting_up = 0; flushing = 1; }
  else flushing = 0;

  if (starting_up || flushing) output_buffer_full = PRESUMED_FULLNESS;
  else {
	int samples_queued;
#ifdef AU_OSS
#ifdef SNDCTL_DSP_GETODELAY
        if ( (id == 'd' || id == 'D') &&
	     (ioctl(fd, SNDCTL_DSP_GETODELAY, &samples_queued) == -1))
#endif
#endif
	    samples_queued = 0;

	if (!samples_queued) output_buffer_full = PRESUMED_FULLNESS;
	else output_buffer_full = ((bbcount+samples_queued) * 100) / (BB_SIZE + total_bytes);
/* fprintf(stderr," %d",output_buffer_full); */
  }

  ret = 0;

  if (bboffset) {
	memcpy(bbuf, bbuf + bboffset, bbcount);
	bboffset = 0;
  }

  if (!ucount) { flushing = 0; starting_up = 1; out_count = bbcount = bboffset = 0; return; }

  if (bboffset + bbcount + ucount >= BB_SIZE) {
	fprintf(stderr,"buffer overflow\n");
	bboffset = bbcount = 0;
	return;
  }

  memcpy(bbuf + bboffset + bbcount, buf, ucount);
  bbcount += ucount;

}
/* #endif */
