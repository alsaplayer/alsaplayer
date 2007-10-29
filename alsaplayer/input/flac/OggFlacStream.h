//-------------------------------------------------------------------------
//  This class encapsulates an OggFLAC stream.  Only the functions
//  needed by the alsaplayer flac plugin are implemented.
//
//  Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
//
/*  This file is part of AlsaPlayer.
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
 */
//-------------------------------------------------------------------------


#ifndef _OGG_FLAC_STREAM_H_
#define _OGG_FLAC_STREAM_H_

#include <string>
#include "reader.h"
#include "FlacStream.h"

extern "C"
{
#include <OggFLAC/stream_decoder.h>
}

#undef DEBUG

namespace Flac
{

class FlacEngine;
class FlacTag;

class OggFlacStream : public FlacStream
{
 public:

    //-----------------------------------------------------------------
    // Return true if we think name points to  a valid OggFLAC stream.
    //-----------------------------------------------------------------

    static bool isOggFlacStream (const std::string & name);

    
 public:

    //---------------------------------------------------------------
    // Constructor & destructor.  The reader_type f belongs to the
    // OggFlacStream after construction, and it will be closed upon
    // deletion of the OggFlacStream object.  If reportErrors is
    // false, the object will squelch all alsaplayer_error messages.
    // This is particularly useful when attempting to open streams
    // to determine whether they're OggFLAC streams.
    //---------------------------------------------------------------

    OggFlacStream (const std::string & name,
		   reader_type * f,
		   bool reportErrors = true);

    virtual ~OggFlacStream ();


    //--------------------------------------------------------
    // See FlacStream.h for a description of these functions.
    //--------------------------------------------------------

    virtual bool open ();
    virtual bool processOneFrame ();


    //----------------------------------------
    // Seeks are unsupported.  Returns false.
    //----------------------------------------

    virtual bool seekAbsolute (FLAC__uint64 sample);


 private:

    //-----------------------------------------------------------------
    // The flac metadata callback.  It's called as a side effect of the
    // open method.  It'll be called once for each metadata block in
    // the flac stream.  To check its success, look at _mcbSuccess.
    //-----------------------------------------------------------------

    static void metaCallBack (const OggFLAC__StreamDecoder * decoder,
			      const FLAC__StreamMetadata * md,
			      void * cilent_data);

    static FLAC__StreamDecoderWriteStatus 
	writeCallBack (const OggFLAC__StreamDecoder * decoder,
		       const FLAC__Frame * frame,
		       const FLAC__int32 * const buffer[],
		       void * client_data);

    static FLAC__StreamDecoderReadStatus
	readCallBack (const OggFLAC__StreamDecoder * decoder,
		      FLAC__byte buffer[],
		      size_t * bytes,
		      void * client_data);

    static void errCallBack (const OggFLAC__StreamDecoder * decoder,
			     FLAC__StreamDecoderErrorStatus status,
			     void * client_data);


 private:

    OggFlacStream ();


 private:

    OggFLAC__StreamDecoder * _decoder;

}; // class OggFlacStream


//----------------
// Inline methods.
//----------------

inline bool
OggFlacStream::seekAbsolute (FLAC__uint64 sample)
{
    return false;
}

inline
OggFlacStream::OggFlacStream ()
  : _decoder (0)
{
}

} // namespace Flac

#endif // _OGG_FLAC_STREAM_H_
