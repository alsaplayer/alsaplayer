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
#include "effects.h"
#include "md.h"
#include "output.h"
#include "controls.h"

#define PRESUMED_FULLNESS 20

/*static int bboffset = 0;*/
/*int bbcount = 0;*/
/*static int outchunk = 0;*/
/*static int starting_up = 1, flushing = 0;*/
/*static int out_count = 0;*/
/*static int total_bytes = 0;*/

/*
#if defined(AU_OSS) || defined(AU_SUN) || defined(AU_BSDI) || defined(AU_ESD)
#define WRITEDRIVER(fd,buf,cnt) write(fd,buf,cnt)
#else
*/
#define WRITEDRIVER(fd,buf,cnt) play_mode->driver_output_data(buf,cnt)
/*
#endif
*/


int plug_output(unsigned char *buf, struct md *d) {
	int ret = output_fragsize;

	if (buf && d->bbuf && d->bbcount >= ret) {
		memcpy(buf, d->bbuf + d->bboffset, ret);
		d->out_count += ret;
		d->bboffset += ret;
		d->bbcount -= ret;
	}
	else ret = 0;
if (!ret) fprintf(stderr,"something's wrong\n");

	return ret;
}


int b_out_count(struct md *d)
{
  return d->out_count;
}

void b_out(char id, int fd, int *buf, int ocount, struct md *d)
{
  int ret;
  uint32 ucount;

  if (ocount < 0) {
	if (d->bbuf && d->bbcount >= output_fragsize) {
		if (d->bbcount > 2 * output_fragsize) d->bbcount = 2 * output_fragsize;
		else d->bbcount = output_fragsize;
		return;
	}
	d->out_count = d->bboffset = d->bbcount = d->outchunk = 0;
	d->starting_up = 1;
	d->flushing = 0;
	/*d->output_buffer_full = PRESUMED_FULLNESS;*/
	d->total_bytes = 0;
	return;
  }

  ucount = (uint32)ocount;

  if (!d->bbuf) {
    d->bbcount = d->bboffset = 0;
    d->bbuf = (unsigned char *)malloc(BB_SIZE);
    if (!d->bbuf) {
	    fprintf(stderr, "malloc output error");
    }
  }

  if (!d->total_bytes) {
    if (output_fragsize > 0) d->outchunk = output_fragsize;
    if (output_frags > 0) d->total_bytes = output_frags * d->outchunk;
    if (!d->total_bytes) d->total_bytes = AUDIO_BUFFER_SIZE*2;
  }

  if (ucount && !d->outchunk) d->outchunk = ucount;
  if (d->starting_up && ucount + d->bboffset + d->bbcount >= BB_SIZE) d->starting_up = 0;
  if (!ucount) { d->starting_up = 0; d->flushing = 1; }
  else d->flushing = 0;

  ret = 0;

  if (d->bboffset) {
	memcpy(d->bbuf, d->bbuf + d->bboffset, d->bbcount);
	d->bboffset = 0;
  }

  if (!ucount) { d->flushing = 0; d->starting_up = 1; d->out_count = d->bbcount = d->bboffset = 0; return; }

  if (d->bboffset + d->bbcount + ucount >= BB_SIZE) {
	fprintf(stderr,"buffer overflow\n");
	d->bboffset = d->bbcount = 0;
	return;
  }

  memcpy(d->bbuf + d->bboffset + d->bbcount, buf, ucount);
  d->bbcount += ucount;

}
/* #endif */
