//-------------------------------------------------------------------------
//  This class encapsulates a flac seekable stream.  Only the functions
//  needed by the alsaplayer flac plugin are implemented.  It inherits from
//  class FlacStream.
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
//-------------------------------------------------------------------------


#ifndef _FLAC_SEEKABLE_STREAM_H_
#define _FLAC_SEEKABLE_STREAM_H_

#include "FlacStream.h"

extern "C"
{
#include <FLAC/seekable_stream_decoder.h>
}

namespace Flac
{

class FlacEngine;
class FlagTag;

class FlacSeekableStream : public FlacStream
{
 public:

    //-----------------------------
    // Constructor and destructor.
    //-----------------------------

    FlacSeekableStream (const std::string & name, reader_type * f);
    
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

    static void metaCallBack (const FLAC__SeekableStreamDecoder * decoder,
			      const FLAC__StreamMetadata * md,
			      void * client_data);

    static FLAC__StreamDecoderWriteStatus 
	writeCallBack (const FLAC__SeekableStreamDecoder * decoder,
		       const FLAC__Frame * frame,
		       const FLAC__int32 * const buffer[],
		       void * client_data);

    static FLAC__SeekableStreamDecoderReadStatus
	readCallBack (const FLAC__SeekableStreamDecoder * decoder,
		      FLAC__byte buffer[],
		      unsigned * bytes,
		      void * client_data);

    static void errCallBack (const FLAC__SeekableStreamDecoder * decoder,
			     FLAC__StreamDecoderErrorStatus status,
			     void * client_data);

    static FLAC__SeekableStreamDecoderSeekStatus 
	seekCallBack (const FLAC__SeekableStreamDecoder * decoder,
		      FLAC__uint64 offset,
		      void * client_data);

    static FLAC__SeekableStreamDecoderTellStatus 
	tellCallBack (const FLAC__SeekableStreamDecoder * decoder,
		      FLAC__uint64 * offset,
		      void * client_data);

    static FLAC__SeekableStreamDecoderLengthStatus
	lengthCallBack (const FLAC__SeekableStreamDecoder * decoder,
			FLAC__uint64 * len,
			void * client_data);

    static FLAC__bool eofCallBack (const FLAC__SeekableStreamDecoder * decoder,
				   void * client_data);


 private:

    FlacSeekableStream ();


 private:

    FLAC__SeekableStreamDecoder * _decoder;

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
