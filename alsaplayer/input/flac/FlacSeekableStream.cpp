//-------------------------------------------------------------------------
//  This class encapsulates the flac seekable stream.  Only the functions
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

#include "FlacEngine.h"
#include "FlacSeekableStream.h"

#include "alsaplayer_error.h"

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT < 8
#define LEGACY_FLAC
#else
#undef LEGACY_FLAC
#endif

namespace Flac
{

FlacSeekableStream::FlacSeekableStream (const std::string & name,
					reader_type * f, bool reportErrors)
    : FlacStream (name, f, reportErrors),
      _decoder (0)
{
} // FlacSeekableStream::FlacSeekableStream


FlacSeekableStream::~FlacSeekableStream ()
{
    if (_decoder)
    {
#ifdef LEGACY_FLAC
	FLAC__seekable_stream_decoder_finish (_decoder);
	FLAC__seekable_stream_decoder_delete (_decoder);
#else
	FLAC__stream_decoder_finish (_decoder);
	FLAC__stream_decoder_delete (_decoder);
#endif
	_decoder = 0;
    }

} // FlacSeekableStream::~FlacSeekableStream


bool
FlacSeekableStream::open ()
{
    // it's illegal to call this on an already open stream
    if (_decoder) {
	apError ("FlacSeekableStream::open(): existing decoder");    
	return false;
    }

#ifdef LEGACY_FLAC
    _decoder = FLAC__seekable_stream_decoder_new ();
#else
    _decoder = FLAC__stream_decoder_new ();
#endif
    if (!_decoder) {
#ifdef LEGACY_FLAC
	apError ("FlacSeekableStream::open(): error creating FLAC__seekable_stream_decoder");
#else
	apError ("FlacSeekableStream::open(): error creating FLAC__stream_decoder");
#endif
	return false;
    }
#ifdef LEGACY_FLAC
    bool status = true;
    status &= FLAC__seekable_stream_decoder_set_read_callback (_decoder,
							       readCallBack);
    status &= FLAC__seekable_stream_decoder_set_write_callback (_decoder, 
								writeCallBack);
    status &= FLAC__seekable_stream_decoder_set_metadata_callback (_decoder,
								   metaCallBack);
    status &= FLAC__seekable_stream_decoder_set_error_callback (_decoder,
								errCallBack);
    status &= FLAC__seekable_stream_decoder_set_seek_callback (_decoder,
							       seekCallBack);
    status &= FLAC__seekable_stream_decoder_set_tell_callback (_decoder,
							       tellCallBack);
    status &= FLAC__seekable_stream_decoder_set_length_callback (_decoder,
								 lengthCallBack);
    status &= FLAC__seekable_stream_decoder_set_eof_callback (_decoder,
							      eofCallBack);
    status &= FLAC__seekable_stream_decoder_set_client_data (_decoder, (void *) this);

    if (!status) {
	apError ("FlacSeekableStream::open(): status error, huh?");    
	return false;
    }
    status = (FLAC__seekable_stream_decoder_init (_decoder) == FLAC__SEEKABLE_STREAM_DECODER_OK);
    
    if (!status) {
#else
    if (FLAC__stream_decoder_init_stream(_decoder,
					 readCallBack,
					 seekCallBack,
					 tellCallBack,
					 lengthCallBack,
					 eofCallBack,
					 writeCallBack,
					 metaCallBack,
					 errCallBack,
					 (void *) this)
	!= FLAC__STREAM_DECODER_INIT_STATUS_OK) {
#endif
	apError ("FlacSeekableStream::open(): can't initialize seekable stream decoder");    
	return false;
    }

    // this will invoke the metaCallBack
#ifdef LEGACY_FLAC
    if (!FLAC__seekable_stream_decoder_process_until_end_of_metadata (_decoder)) {
#else
    if (!FLAC__stream_decoder_process_until_end_of_metadata (_decoder)) {
#endif
	apError ("FlacSeekableStream::open(): decoder error");    
	return false;
    }

    // now that we've opened the stream, tell the engine it's safe to 
    // initialize itself.
    
    if (!_engine->init ()) {
	apError ("FlacSeekableStream::open(): engine init failed");
	return false;
    }

    // return the metaCallBack's status
    return _mcbSuccess;
    
} // FlacSeekableStream::open


bool
FlacSeekableStream::processOneFrame ()
{
    if (!_decoder)
	return false;

#ifdef LEGACY_FLAC
    return FLAC__seekable_stream_decoder_process_single (_decoder);
#else
    return FLAC__stream_decoder_process_single (_decoder);
#endif

} // FlacSeekableStream::processOneFrame


bool
FlacSeekableStream::seekAbsolute (FLAC__uint64 sample)
{
    if (!_decoder)
	return false;

#ifdef LEGACY_FLAC
    return FLAC__seekable_stream_decoder_seek_absolute (_decoder, sample);
#else
    return FLAC__stream_decoder_seek_absolute (_decoder, sample);
#endif

} // FlacSeekableStream::seekAbsolute


// static
void
#ifdef LEGACY_FLAC
FlacSeekableStream::metaCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
FlacSeekableStream::metaCallBack (const FLAC__StreamDecoder * decoder,
#endif
				  const FLAC__StreamMetadata * md,
				  void * client_data)
{
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;

    if (!f)
    {
	f->apError ("FlacSeekableStream::metaCallBack(): no client data");
	return;
    }

    f->realMetaCallBack (md);

} // FlacSeekableStream::metaCallBack


// static
void
#ifdef LEGACY_FLAC
FlacSeekableStream::errCallBack (const FLAC__SeekableStreamDecoder * decoder,
#else
FlacSeekableStream::errCallBack (const FLAC__StreamDecoder * decoder,
#endif
				 FLAC__StreamDecoderErrorStatus status,
				 void * client_data)
{
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
    {
	f->apError ("FlacStream::errCallBack (): no client data");
	return;
    }

    f->realErrCallBack ("FLAC", status);

} // FlacSeekableStream::errCallBack


// static
FLAC__StreamDecoderWriteStatus
#ifdef LEGACY_FLAC
FlacSeekableStream::writeCallBack (const FLAC__SeekableStreamDecoder * /*decoder*/,
#else
FlacSeekableStream::writeCallBack (const FLAC__StreamDecoder * /*decoder*/,
#endif
				   const FLAC__Frame * frame,
				   const FLAC__int32 * const buffer[],
				   void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

    return f->realWriteCallBack (frame, buffer);

} // FlacSeekableStream::writeCallBack


// static
#ifdef LEGACY_FLAC
FLAC__SeekableStreamDecoderReadStatus
FlacSeekableStream::readCallBack (const FLAC__SeekableStreamDecoder * /*decoder*/,
#else
FLAC__StreamDecoderReadStatus
FlacSeekableStream::readCallBack (const FLAC__StreamDecoder * /*decoder*/,
#endif
				  FLAC__byte buffer[],
				  size_t * bytes,
				  void * client_data)
{
    if (!client_data)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
#endif
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
#endif

    *bytes = reader_read (buffer, *bytes, f->_datasource);
#ifdef LEGACY_FLAC
    return *bytes > 0 ? FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK :
#else
    return *bytes > 0 ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE :
#endif
	reader_eof (f->_datasource) ? 
#ifdef LEGACY_FLAC
	FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK : 
	FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
#else
	FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : 
	FLAC__STREAM_DECODER_READ_STATUS_ABORT;
#endif

} // FlacSeekableStream::readCallBack


// static
#ifdef LEGACY_FLAC
FLAC__SeekableStreamDecoderSeekStatus 
FlacSeekableStream::seekCallBack (const FLAC__SeekableStreamDecoder * /*decoder*/,
#else
FLAC__StreamDecoderSeekStatus 
FlacSeekableStream::seekCallBack (const FLAC__StreamDecoder * /*decoder*/,
#endif
				  FLAC__uint64 offset,
				  void * client_data)
{
    if (!client_data)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
#endif
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
#endif
    
    return reader_seek (f->_datasource, offset, SEEK_SET) == 0 ?
#ifdef LEGACY_FLAC
	FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK :
	FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
#else
	FLAC__STREAM_DECODER_SEEK_STATUS_OK :
	FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
#endif

} // FlacSeekableStream::seekCallBack


// static
#ifdef LEGACY_FLAC
FLAC__SeekableStreamDecoderTellStatus 
FlacSeekableStream::tellCallBack (const FLAC__SeekableStreamDecoder * /*decoder*/,
#else
FLAC__StreamDecoderTellStatus 
FlacSeekableStream::tellCallBack (const FLAC__StreamDecoder * /*decoder*/,
#endif
				  FLAC__uint64 * offset,
				  void * client_data)
{
    if (!client_data)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
#endif
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
#endif

    long result = reader_tell (f->_datasource);
    if (result == -1)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
#endif
    *offset = result;
#ifdef LEGACY_FLAC
    return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
#else
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
#endif
    
} // FlacSeekableStream::tellCallBack


// static
#ifdef LEGACY_FLAC
FLAC__SeekableStreamDecoderLengthStatus
FlacSeekableStream::lengthCallBack (const FLAC__SeekableStreamDecoder * /*decoder*/,
#else
FLAC__StreamDecoderLengthStatus
FlacSeekableStream::lengthCallBack (const FLAC__StreamDecoder * /*decoder*/,
#endif
				    FLAC__uint64 * len,
				    void * client_data)
{
    if (!client_data)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
#endif
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
#endif

    long result = reader_length (f->_datasource);
    if (result == -1)
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
#else
	return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
#endif
    *len = result;
#ifdef LEGACY_FLAC
    return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
#else
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
#endif

} // FlacSeekableStream::lengthCallBack


// static
FLAC__bool
#ifdef LEGACY_FLAC
FlacSeekableStream::eofCallBack (const FLAC__SeekableStreamDecoder * /*decoder*/,
#else
FlacSeekableStream::eofCallBack (const FLAC__StreamDecoder * /*decoder*/,
#endif
				 void * client_data)
{
    if (!client_data)
	return 1;
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
	return 1;

    return reader_eof (f->_datasource);

} // FlacSeekableStream::eofCallBack

} // namespace Flac
