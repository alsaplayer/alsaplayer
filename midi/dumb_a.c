/* 

*/

#include "gtim.h"
#include "output.h"
#include "controls.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, uint32 count);
static int driver_output_data(unsigned char *buf, uint32 count);
static void flush_output(void);
static void purge_output(void);
static int output_count(uint32 ct);

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


static int open_output(void)
{

  dpm.fd=0;
  return 0;
}


static int output_count(uint32 ct)
{
  int samples = (int)ct;
  return samples;
}

static int driver_output_data(unsigned char *buf, uint32 count)
{
  return count;
}

static void output_data(int32 *buf, uint32 count)
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

  b_out(dpm.id_character, dpm.fd, (int *)buf, ocount);
}


static void close_output(void)
{
}

static void flush_output(void)
{
  output_data(0, 0);
}

static void purge_output(void)
{
/*fprintf(stderr,"dumb purge\n");*/
  b_out(dpm.id_character, dpm.fd, 0, -1);
}

