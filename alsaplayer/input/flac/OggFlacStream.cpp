//-------------------------------------------------------------------------
//  This class encapsulates the OggFLAC stream.  Only the functions
//  needed by the alsaplayer flac plugin are implemented.
//
// Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
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

#include "OggFlacStream.h"
#include "FlacEngine.h"

#include <string.h>
#include "alsaplayer_error.h"

#include <string>

using namespace std;

namespace Flac
{

// static
bool
OggFlacStream::isOggFlacStream (const string & name)
{
  reader_type * rdr = reader_open (name.c_str (), NULL, NULL);
  if (!rdr)
      return false;

  OggFlacStream f (name, rdr, false);
  
  return f.open ();

} // OggFlacStream::isOggFlacStream


OggFlacStream::OggFlacStream (const string & name,
			      reader_type * f,
			      bool reportErrors)
    : FlacStream (name, f, reportErrors),
      _decoder (0)
{
} // OggFlacStream::OggFlacStream


OggFlacStream::~OggFlacStream ()
{
    if (_decoder)
    {
	OggFLAC__stream_decoder_finish (_decoder);
	OggFLAC__stream_decoder_delete (_decoder);
	_decoder = 0;
    }

} // OggFlacStream::~OggFlacStream


bool
OggFlacStream::open ()
{
    // it's illegal to call this on an already open stream
    if (_decoder) 
	return false;

    _decoder = OggFLAC__stream_decoder_new ();
    if (!_decoder) 
	return false;

    bool status = true;
    status &= OggFLAC__stream_decoder_set_read_callback (_decoder,
							 readCallBack);
    status &= OggFLAC__stream_decoder_set_write_callback (_decoder, 
							  writeCallBack);
    status &= OggFLAC__stream_decoder_set_metadata_callback (_decoder,
							     metaCallBack);
    status &= OggFLAC__stream_decoder_set_error_callback (_decoder,
							  errCallBack);
    status &= OggFLAC__stream_decoder_set_client_data (_decoder,
						       (void *) this);

    if (!status)
	return false;

    status = (OggFLAC__stream_decoder_init (_decoder) == 
	      OggFLAC__STREAM_DECODER_OK);
    if (!status)
	return false;

    // this will invoke the metaCallBack
    if (!OggFLAC__stream_decoder_process_until_end_of_metadata (_decoder))
	return false;

    // now that we've opened the stream, tell the engine it's safe to 
    // initialize itself.
    
    if (!_engine->init ())
	return false;

    // return the metaCallBack's status
    return _mcbSuccess;

} // OggFlacStream::open


bool
OggFlacStream::processOneFrame ()
{
    if (!_decoder)
	return false;

    return OggFLAC__stream_decoder_process_single (_decoder);

} // OggFlacStream::processOneFrame


// static
void
OggFlacStream::metaCallBack (const OggFLAC__StreamDecoder * decoder,
			     const FLAC__StreamMetadata * md,
			     void * client_data)
{
    OggFlacStream * f = (OggFlacStream *) client_data;

    if (!f)
    {
	f->apError ("OggFlacStream::metaCallBack(): no client data");
	return;
    }

    f->realMetaCallBack (md);

} // OggFlacStream::metaCallBack


// static
void
OggFlacStream::errCallBack (const OggFLAC__StreamDecoder * decoder,
			    FLAC__StreamDecoderErrorStatus status,
			    void * client_data)
{
    OggFlacStream * f = (OggFlacStream *) client_data;
    if (!f)
    {
	f->apError ("OggFlacStream::errCallBack (): no client data");
	return;
    }

    f->realErrCallBack ("OggFLAC", status);

} // OggFlacStream::errCallBack


// static
FLAC__StreamDecoderWriteStatus
OggFlacStream::writeCallBack (const OggFLAC__StreamDecoder * decoder,
			      const FLAC__Frame * frame,
			      const FLAC__int32 * const buffer[],
			      void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    
    OggFlacStream * f = (OggFlacStream *) client_data;
    if (!f)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

    return f->realWriteCallBack (frame, buffer);
    
} // OggFlacStream::writeCallBack


// static
FLAC__StreamDecoderReadStatus
OggFlacStream::readCallBack (const OggFLAC__StreamDecoder * decoder,
			     FLAC__byte buffer[],
			     size_t * bytes,
			     void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    OggFlacStream * f = (OggFlacStream *) client_data;
    if (!f)
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    return f->realReadCallBack (buffer, bytes);

} // OggFlacStream::readCallBack

} // namespace Flac
