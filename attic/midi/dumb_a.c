/* 
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
 *
*/

#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "effects.h"
#include "md.h"
#include "output.h"
#include "controls.h"

static int open_output(struct md *d); /* 0=success, 1=warning, -1=fatal error */
static void close_output(struct md *d);
static void output_data(int32 *buf, uint32 count, struct md *d);
static int driver_output_data(unsigned char *buf, uint32 count, struct md *d);
static void flush_output(struct md *d);
static void purge_output(struct md *d);
static int output_count(uint32 ct, struct md *d);

/* export the playback mode */

#undef dpm
#define dpm dumb_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED,
  -1,
  {0}, /* default: get all the buffer fragments you can */
  "Dumb dsp device", 'a',
  "/dev/dsp",
  open_output,
  close_output,
  output_data,
  driver_output_data,
  flush_output,
  purge_output,
  output_count
};


static int open_output(struct md *d)
{

  dpm.fd=0;
  return 0;
}


static int output_count(uint32 ct, struct md *d)
{
  int samples = (int)ct;
  return samples;
}

static int driver_output_data(unsigned char *buf, uint32 count, struct md *d)
{
  return count;
}

static void output_data(int32 *buf, uint32 count, struct md *d)
{
  int ocount;

  if (!(dpm.encoding & PE_MONO)) count*=2; /* Stereo samples */
  ocount = (int)count;

  if (ocount) {
    if (dpm.encoding & PE_16BIT)
      {
        /* Convert data to signed 16-bit PCM */
        s32tos16(buf, count);
        ocount *= 2;
      }
    else
      {
        /* Convert to 8-bit unsigned and write out. */
        s32tou8(buf, count);
      }
  }

  b_out(dpm.id_character, dpm.fd, (int *)buf, ocount, d);
}


static void close_output(struct md *d)
{
}

static void flush_output(struct md *d)
{
  output_data(0, 0, d);
}

static void purge_output(struct md *d)
{
/*fprintf(stderr,"dumb purge\n");*/
  b_out(dpm.id_character, dpm.fd, 0, -1, d);
}

