//---------------------------------------------------------------------------
//  alsaplayer flac input plugin handlers.
//
//  Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//--------------------------------------------------------------------------


#include <string>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include "input_plugin.h"
#include "alsaplayer_error.h"

#include "FlacFile.h"
#include "FlacEngine.h"
#include "FlacTag.h"

using namespace std;

static int
flac_channels (input_object * obj)
{
    return obj->nr_channels;
}


static int
flac_sample_rate (input_object * obj)
{
    if (!obj)
	return 0;

    Flac::FlacFile * f = (Flac::FlacFile *) obj->local_data;
    if (!f)
	return 0;
    return (int) f->sampleRate ();
}


static int
flac_frame_size (input_object * obj)
{
    if (!obj)
	return 0;
    return obj->frame_size;
}


static int
flac_nr_frames (input_object * obj)
{
    if (!obj)
	return 0;
    return obj->nr_frames;
}


static long
flac_frame_to_centisec (input_object * obj, int frame)
{
    if (!obj)
	return 0;
    
    Flac::FlacFile * f = (Flac::FlacFile *) obj->local_data;
    if (!f || !f->engine ())
	return 0;

    return (long) f->engine ()->frameTime (frame) * 100;
}


static int
flac_frame_seek (input_object * obj, int frame)
{
    if (!obj)
	return 0;

    Flac::FlacFile * f = (Flac::FlacFile *) obj->local_data;
    if (!f)
      return 0;
    return f->engine ()->seekToFrame (frame);
}


static int
flac_play_frame (input_object * obj, char * buf)
{
    if (!obj || !buf)
	return 0;
    
    Flac::FlacFile * f = (Flac::FlacFile *) obj->local_data;
    if (!f)
	return 0;
    return f->engine ()->decodeFrame (buf);
}


static int
flac_open (input_object * obj, const char * path)
{
    if (!obj)
	return 0;
    if (!path)
	return 0;

    Flac::FlacFile *   f = 0;
    Flac::FlacEngine * e = 0;
    try
    {
	f = new Flac::FlacFile (string (path), false);
	e = new Flac::FlacEngine (f);
    }
    catch (...)
    {
	alsaplayer_error ("flac_open: unable to allocate memory for plugin.");
	delete f;
	delete e;
	return 0;
    }

    f->setEngine (e);

    if (f->open ())
    {
	obj->frame_size  = f->engine ()->apFrameSize ();

	// attach a song info tag
	
	if (Flac::FlacTag::hasTag (f->path ()))
	{
	    Flac::FlacTag * t = Flac::FlacTag::newTag (f->path ());
	    f->setTag (t);
	}

	obj->nr_channels = f->engine ()->apChannels ();
	obj->flags       = P_SEEK | P_REENTRANT | P_FILEBASED;
	obj->nr_frames   = f->engine ()->apFrames ();
	obj->nr_tracks   = 1;
	obj->ready       = 1;
	obj->local_data  = (void *) f;
	return 1;
    }
    else
    {
	alsaplayer_error ("flac_open: unable to open flac file or "
			  "unsupported flac file.");

	delete f;
	delete e;
	obj->frame_size  = 0;
	obj->nr_channels = 0;
	obj->flags       = 0;
	obj->nr_frames   = 0;
	obj->nr_tracks   = 0;
	obj->ready       = 0;
	obj->local_data  = 0;
	return 0;
    }
}


static int
flac_stream_info (input_object * obj, stream_info * info)
{
    if (!obj || !info)
	return 0;

    Flac::FlacFile * f = (Flac::FlacFile *) obj->local_data;
    if (!f)
	return 0;

    sprintf (info->stream_type, "%d-bit %dKhz %s flac", 
	     f->bps (), f->sampleRate () / 1000,
	     f->channels () == 1 ? "mono" :
	     f->channels () == 2 ? "stereo" : "multi-channel");

    Flac::FlacTag * t = f->tag ();
    if (t && ! t->title ().empty ())
    {
	// strncpy limits come from looking at input_plugin.h; these should
        // really be defined as constants somewhere

	strncpy (info->artist, t->artist ().c_str (), 128);
	strncpy (info->title, t->title ().c_str (), 128);
	strncpy (info->album, t->album ().c_str (), 128);
	strncpy (info->genre, t->genre ().c_str (), 128);
	strncpy (info->year, t->year ().c_str (), 10);
	strncpy (info->track, t->track ().c_str (), 10);
	strncpy (info->comment, t->comment ().c_str (), 128);
    }
    else
    {
	// use filename
	char * fname = strrchr (f->path ().c_str (), '/');
	if (fname)
	{
	    fname++;
	    strncpy (info->title, fname, 128);
	}
	else
	    info->title[0] = 0;
	
	info->artist[0]  = 0;
	info->title[0]   = 0;
	info->album[0]   = 0;
	info->genre[0]   = 0;
	info->year[0]    = 0;
	info->track[0]   = 0;
	info->comment[0] = 0;
    }
    info->status[0] = 0;

    return 1;
}


static float
flac_can_handle (const char * path)
{
    return Flac::FlacFile::isFlacFile (string (path)) ? 1.0 : 0.0;
}


static void
flac_close (input_object * obj)
{
    if (!obj)
	return;

    Flac::FlacFile * f = (Flac::FlacFile *) obj->local_data;
    delete f;
    f = 0;
}


static int
flac_init ()
{
    return 1;
}


static void
flac_shutdown ()
{
    return;
}


static input_plugin flac_plugin;

extern "C"
{

input_plugin *
input_plugin_info (void)
{
    memset (&flac_plugin, 0, sizeof(input_plugin));

    flac_plugin.version      = INPUT_PLUGIN_VERSION;
    flac_plugin.name         = "flac player v1.05";
    flac_plugin.author       = "Drew Hess";
    flac_plugin.init         = flac_init;
    flac_plugin.shutdown     = flac_shutdown;
    flac_plugin.can_handle   = flac_can_handle;
    flac_plugin.open         = flac_open;
    flac_plugin.close        = flac_close;
    flac_plugin.play_frame   = flac_play_frame;
    flac_plugin.frame_seek   = flac_frame_seek;
    flac_plugin.frame_size   = flac_frame_size;
    flac_plugin.nr_frames    = flac_nr_frames;
    flac_plugin.frame_to_sec = flac_frame_to_centisec;
    flac_plugin.sample_rate  = flac_sample_rate;
    flac_plugin.channels     = flac_channels;
    flac_plugin.stream_info  = flac_stream_info;
    
    return & flac_plugin;
}

}
