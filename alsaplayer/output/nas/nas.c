/*  nas.c - Output driver for The Network Audio System (NAS)
 *  Copyright (C) 2000 Erik Inge Bolsø <knan@mo.himolde.no>
 *    Based on the NAS audio module for mpg123.
 *
 *  This file is part of AlsaPlayer.
 *
 *  AlsaPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AlsaPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
*/ 

/* Note: This plugin has a kind of weird structure right now - most of its
 *       initialization takes place in nas_set_buffer. Sample rate cannot
 *       be changed while running, a player restart is needed.
 *
 *       The receiving server must support signed 16-bit in the client's
 *       byte order, stereo.
 *
 *       Alsaplayer converts everything to 16-bit stereo anyway, so this is not
 *       a specific problem of this plugin :-)
*/

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <audio/audiolib.h>
#include <audio/soundlib.h>
#include "output_plugin.h"
#include "alsaplayer_error.h"

typedef struct
{
    AuServer            *aud;
    AuFlowID            flow;
    AuDeviceAttributes  *da;
    int                 numDevices;
    char                *buf;
    AuUint32            buf_size;
    AuUint32            buf_cnt;
    AuBool              data_sent;
    AuBool              finished;
    int			freq;
} nas_info;

static nas_info Nas_Info = {NULL,0,NULL,0,NULL,0,0,0,0,44100};

#define NAS_SOUND_LOW_WATER_MARK 25 /* percent */
#define NAS_MAX_FORMAT 10 /* currently, there are 7 supported formats */

static void
nas_sendData(AuServer *aud, nas_info *i, AuUint32 numBytes)
{
    if (numBytes < i->buf_cnt) {
        AuWriteElement(aud, i->flow, 0, numBytes, i->buf, AuFalse, NULL);
        memmove(i->buf, i->buf + numBytes, i->buf_cnt - numBytes);
        i->buf_cnt = i->buf_cnt - numBytes;
    }
    else {
         AuWriteElement(aud, i->flow, 0, i->buf_cnt, i->buf,
                        (numBytes > i->buf_cnt), NULL);
         i->buf_cnt = 0;
    }
    i->data_sent = AuTrue;
}


/* Main routine - remove this and no playback happens. */
static AuBool
nas_eventHandler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *handler)
{
    nas_info *i = (nas_info *) handler->data;

    switch (ev->type)
    {
        case AuEventTypeMonitorNotify:
            i->finished = AuTrue;
            break;
       case AuEventTypeElementNotify:
           {
               AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;

               switch (event->kind)
               {
                   case AuElementNotifyKindLowWater:
                       nas_sendData(aud, i, event->num_bytes);
                       break;
                   case AuElementNotifyKindState:
                       switch (event->cur_state)
                       {
                           case AuStatePause:
                               if (event->reason != AuReasonUser)
                                   nas_sendData(aud, i, event->num_bytes);
                               break;
                            case AuStateStop:
                                i->finished = AuTrue;
                                break;
                       }
               }
           }
    }
    return AuTrue;
}

void nas_flush()
{
    AuEvent         ev;
    
    while ((!Nas_Info.data_sent) && (!Nas_Info.finished)) {
        AuNextEvent(Nas_Info.aud, AuTrue, &ev);
        AuDispatchEvent(Nas_Info.aud, &ev);
    }
    Nas_Info.data_sent = AuFalse;
}

static int nas_init()
{
	// Always return ok for now
	return 1;
}

static int nas_open(const char *device)
{
	char *server = NULL;

	server = getenv("AUDIOSERVER");
	if (server == NULL)
	{
	    fprintf(stderr, "No $AUDIOSERVER, falling back on $DISPLAY\n");

	    server = getenv("DISPLAY");
	    if (server == NULL)
	    {
	        fprintf(stderr, "No $DISPLAY, falling back on tcp/localhost:8000\n");
	    	server = "tcp/localhost:8000";
	    }
	}

	Nas_Info.aud = AuOpenServer(server, 0, NULL, 0, NULL, NULL);

	if (Nas_Info.aud == NULL)
	{
		fprintf(stderr, "NAS server not available\n");
		return 0;
	}
	return 1;
}


static void nas_close()
{
	AuCloseServer(Nas_Info.aud);
	return;
}


static int nas_write(void *buf,int len)
{
    int buf_cnt = 0;

    /* alsaplayer_error("nas_write(%x, %d)", buf, len); */

    while ((Nas_Info.buf_cnt + (len - buf_cnt)) >  Nas_Info.buf_size) {
        memcpy(Nas_Info.buf + Nas_Info.buf_cnt,
               (int *)buf + buf_cnt,
               (Nas_Info.buf_size - Nas_Info.buf_cnt));
        buf_cnt += (Nas_Info.buf_size - Nas_Info.buf_cnt);
        Nas_Info.buf_cnt += (Nas_Info.buf_size - Nas_Info.buf_cnt);
        nas_flush();
    }
    memcpy(Nas_Info.buf + Nas_Info.buf_cnt,
           (int *)buf + buf_cnt,
           (len - buf_cnt));
    Nas_Info.buf_cnt += (len - buf_cnt);
    //nas_flush();
    return 1;
}

static int nas_set_buffer(int *fragment_size, int *fragment_count, int *channels)
{
    AuDeviceID      device = AuNone;
    AuElement       elements[2];
    unsigned char   format;
    AuUint32        buf_samples;
    int             i;
 
    /* Sample format hardcoded to 16-bit signed, of native
       byte order, stereo */

    /* alsaplayer_error("nas_set_buffer(%d, %d, %d)", fragment_size, fragment_count, channels); */
   
    *fragment_count = 3;
    
    if (((char) *(short *)"x")=='x') /* ugly, but painless */
    {
	format = AuFormatLinearSigned16LSB; /* little endian */
    }
    else
    {
	format = AuFormatLinearSigned16MSB; /* big endian */
    }

    /* look for an physical, stereo, output device */
    for (i = 0; i < AuServerNumDevices(Nas_Info.aud); i++)
       if
       ((AuDeviceKind(AuServerDevice(Nas_Info.aud, i)) ==  AuComponentKindPhysicalOutput)
       &&
       (AuDeviceNumTracks(AuServerDevice(Nas_Info.aud, i)) ==  *channels ))
       {
            device = AuDeviceIdentifier(AuServerDevice(Nas_Info.aud, i));
            break;
       }
    if (device == AuNone) {
       fprintf(stderr,
                "Couldn't find an output device providing %d channels\n", 2);
        exit(1);
    }

    if (!(Nas_Info.flow = AuCreateFlow(Nas_Info.aud, NULL))) {
        fprintf(stderr, "Couldn't create flow\n");
        exit(1);
    }

    buf_samples = *fragment_size * *fragment_count;

/* POSSIBLE FIXME: Rate cannot be changed run-time :-( */

    AuMakeElementImportClient(&elements[0],        /* element */
                              (unsigned short) Nas_Info.freq /* ai->rate*/ ,
                                                   /* rate */
                              format,              /* format */
                              *channels /*ai->channels*/ ,        /* channels */
                              AuTrue,              /* ??? */
                              buf_samples,         /* max samples */
                              (AuUint32) (buf_samples / 100
                                  * NAS_SOUND_LOW_WATER_MARK),
                                                   /* low water mark */
                              0,                   /* num actions */
                              NULL);               /* actions */
    AuMakeElementExportDevice(&elements[1],        /* element */
                              0,                   /* input */
                              device,              /* device */
                              (unsigned short) Nas_Info.freq /*ai->rate*/,
                                                   /* rate */
                              AuUnlimitedSamples,  /* num samples */
                              0,                   /* num actions */
                              NULL);               /* actions */
    AuSetElements(Nas_Info.aud,                        /* Au server */
                  Nas_Info.flow,                       /* flow ID */
                  AuTrue,                          /* clocked */
                  2,                               /* num elements */
                  elements,                        /* elements */
                  NULL);                           /* return status */

    AuRegisterEventHandler(Nas_Info.aud,               /* Au server */
                           AuEventHandlerIDMask,   /* value mask */
                           0,                      /* type */
                           Nas_Info.flow,              /* id */
                           nas_eventHandler,       /* callback */
                           (AuPointer) &Nas_Info);     /* data */

    Nas_Info.buf_size = buf_samples *  *channels * AuSizeofFormat(format);
    Nas_Info.buf = (char *) malloc(Nas_Info.buf_size);
    if (Nas_Info.buf == NULL) {
        fprintf(stderr, "Unable to allocate input/output buffer of size %u\n",
             Nas_Info.buf_size);
        exit(1);
    }
    Nas_Info.buf_cnt = 0;
    Nas_Info.data_sent = AuFalse;
    Nas_Info.finished = AuFalse;
    
    AuStartFlow(Nas_Info.aud,	/* Au server */
                Nas_Info.flow,  /* id */
                NULL);          /* status */
	return 1;	
}

static unsigned int nas_set_sample_rate(unsigned int rate)
{
	/* alsaplayer_error("nas_set_sample_rate(%d)", rate); */
	/* Must be called before nas_set_buffer, or rate change will be ignored */
	Nas_Info.freq = rate;
	return rate;
}


output_plugin nas_output;


output_plugin *output_plugin_info(void)
{
	memset(&nas_output, 0, sizeof(output_plugin));
	nas_output.version = OUTPUT_PLUGIN_VERSION;
	nas_output.name = "NAS output v0.2 (30-Mar-2000)";
	nas_output.author = "Erik Inge Bolsø";
	nas_output.init = nas_init;
	nas_output.open = nas_open;
	nas_output.close = nas_close;
	nas_output.write = nas_write;
	nas_output.set_buffer = nas_set_buffer;
	nas_output.set_sample_rate = nas_set_sample_rate;
	return &nas_output;
}
