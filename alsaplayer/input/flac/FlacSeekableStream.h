//-------------------------------------------------------------------------
//  This class encapsulates a flac seekable stream.  Only the functions
//  needed by the alsaplayer flac plugin are implemented.  It inherits from
//  class FlacStream.
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


#ifndef _FLAC_SEEKABLE_STREAM_H_
#define _FLAC_SEEKABLE_STREAM_H_

#include "FlacStream.h"

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT < 8
#define LEGACY_FLAC
#else
#undef LEGACY_FLAC
#endif

extern "C"
{
#ifdef LEGACY_FLAC
#include <FLAC/seekable_stream_decoder.h>
#else
#include <FLAC/stream_decoder.h>
#endif
}

namespace Flac
{

class FlacEngine;
class FlagTag;

class FlacSeekableStream : public FlacStream
{
 public:

    //------------------------------------------------------------------
    // Constructor and destructor.  The reader_type f belongs to the
    // FlacSeekableStream after construction, and it will be closed
    // upon deletion of the FlacSeekableStream object.  If reportErrors 
    // is false, the object will squelch all alsaplayer_error messages.
    // This is particularly useful when attempting to open streams
    // to determine whether they're FLAC streams.
    //------------------------------------------------------------------

    FlacSeekableStream (const std::string & name,
			reader_type * f,
			bool reportErrors = true);
    
    virtual ~FlacSeekableStream ();


    //--------------------------------------------------------
    // See FlacStream.h for a description of these functions.
    //--------------------------------------------------------

    virtual bool open ();
    virtual bool processOneFrame ();


    //---------------------------------------------
    // Seek to the specified sample in the stream.
    //---------------------------------------------

    virtual bool seekAbsolute (FLAC__uint64 sample);


 private:

    //---------------------------------------
    // flac callbacks for a seekable stream.
    //---------------------------------------

#ifdef LEGACY_FLAC
    static void metaCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
    static void metaCallBack (const FLAC__StreamDecoder * decoder,
#endif
		      const FLAC__StreamMetadata * md,
			      void * client_data);

    static FLAC__StreamDecoderWriteStatus 
#ifdef LEGACY_FLAC
	writeCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
	writeCallBack (const FLAC__StreamDecoder * decoder,
#endif
		       const FLAC__Frame * frame,
		       const FLAC__int32 * const buffer[],
		       void * client_data);

#ifdef LEGACY_FLAC
    static FLAC__SeekableStreamDecoderReadStatus
	readCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
    static FLAC__StreamDecoderReadStatus
	readCallBack (const FLAC__StreamDecoder * decoder,
#endif
		      FLAC__byte buffer[],
		      size_t * bytes,
		      void * client_data);

#ifdef LEGACY_FLAC
    static void errCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
    static void errCallBack (const FLAC__StreamDecoder * decoder,
#endif
			     FLAC__StreamDecoderErrorStatus status,
			     void * client_data);

#ifdef LEGACY_FLAC
    static FLAC__SeekableStreamDecoderSeekStatus 
	seekCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
    static FLAC__StreamDecoderSeekStatus 
	seekCallBack (const FLAC__StreamDecoder * decoder,
#endif
		      FLAC__uint64 offset,
		      void * client_data);

#ifdef LEGACY_FLAC
    static FLAC__SeekableStreamDecoderTellStatus 
	tellCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
    static FLAC__StreamDecoderTellStatus 
	tellCallBack (const FLAC__StreamDecoder * decoder,
#endif
		      FLAC__uint64 * offset,
		      void * client_data);

#ifdef LEGACY_FLAC
    static FLAC__SeekableStreamDecoderLengthStatus
	lengthCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
    static FLAC__StreamDecoderLengthStatus
	lengthCallBack (const FLAC__StreamDecoder * decoder,
#endif
			FLAC__uint64 * len,
			void * client_data);


#ifdef LEGACY_FLAC
   static FLAC__bool eofCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
    static FLAC__bool eofCallBack (const FLAC__StreamDecoder * decoder,
#endif
				   void * client_data);


 private:

    FlacSeekableStream ();


 private:

#ifdef LEGACY_FLAC
    FLAC__SeekableStreamDecoder * _decoder;
#else
    FLAC__StreamDecoder * _decoder;
#endif

}; // class FlacSeekableStream


//-----------------
// Inline methods.
//-----------------

inline
FlacSeekableStream::FlacSeekableStream ()
    : _decoder (0)
{
}

} // namespace Flac

#endif // _FLAC_SEEKABLE_STREAM_H_
