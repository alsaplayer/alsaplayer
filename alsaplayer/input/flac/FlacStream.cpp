//-------------------------------------------------------------------------
//  This class encapsulates the flac stream.  Only the functions
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
#include "FlacStream.h"
#include "FlacTag.h"

#include <string.h>
#include <stdarg.h>
#include "alsaplayer_error.h"

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT < 8
#define LEGACY_FLAC
#else
#undef LEGACY_FLAC
#endif

namespace Flac
{

// static
void
FlacStream::apError (const char * msg)
{
    if (!_reportErrors)
	return;
    alsaplayer_error (msg);
    
} // FlacStream::apError


// static
void
FlacStream::apError (const char * fmt, const char * str)
{
    if (!_reportErrors)
	return;
    alsaplayer_error (fmt, str);

} // FlacStream::apError


// static
bool
FlacStream::isFlacStream (const std::string & name)
{
    reader_type * rdr = reader_open (name.c_str (), NULL, NULL);
    if (!rdr)
        return false;
    FlacStream f (name, rdr, false);
    return f.open ();
    
} // FlacStream::isFlacStream


FlacStream::FlacStream (const std::string & name,
			reader_type * f,
			bool reportErrors)
     : _engine (new FlacEngine (this)),
      _mcbSuccess (false),
      _datasource (f),
      _reportErrors (false),
      _channels (0),
      _bps (0),
      _sampleRate (1),
      _sampPerBlock (0),
      _totalSamp (0),
      _decoder (0),
      _tag (0),
      _name (name)
{
} // FlacStream::FlacStream


FlacStream::~FlacStream ()
{
    if (_decoder)
    {
	FLAC__stream_decoder_finish (_decoder);
	FLAC__stream_decoder_delete (_decoder);
	_decoder = 0;
    }
    delete _engine;
    _engine = 0;
    delete _tag;
    _tag = 0;
    reader_close (_datasource);

} // FlacStream::~FlacStream


bool
FlacStream::open ()
{
    // it's illegal to call this on an already open stream
    if (_decoder) {
	apError ("FlacStream::open(): existing decoder");    
	return false;
    }

    _decoder = FLAC__stream_decoder_new ();
    if (!_decoder) {
	apError("FlacStream::open(): error creating FLAC__stream_decoder");
	return false;
    }
#ifdef LEGACY_FLAC
    bool status = true;
    status &= FLAC__stream_decoder_set_read_callback (_decoder,
						      readCallBack);
    status &= FLAC__stream_decoder_set_write_callback (_decoder, 
						     writeCallBack);
    status &= FLAC__stream_decoder_set_metadata_callback (_decoder,
							metaCallBack);
    status &= FLAC__stream_decoder_set_error_callback (_decoder,
						     errCallBack);
    status &= FLAC__stream_decoder_set_client_data (_decoder, (void *) this);

    if (!status) {
	apError("FlacStream::open(): status error, huh?");    
	return false;
    }
    status = (FLAC__stream_decoder_init (_decoder) == FLAC__STREAM_DECODER_SEARCH_FOR_METADATA);
    
    if (!status) {
	apError("FlacStream::open(): can't initialize stream decoder");
#else
    if (FLAC__stream_decoder_init_stream(_decoder,
					 readCallBack,
					 NULL,  /* seek */
					 NULL,  /* tell */
					 NULL,  /* length */
					 NULL,  /* eof */
					 writeCallBack,
					 metaCallBack,
					 errCallBack,
					 (void *) this)
	!= FLAC__STREAM_DECODER_INIT_STATUS_OK) {
	apError("FlacStream::open(): can't initialize stream decoder");
#endif
	return false;
    }

    // this will invoke the metaCallBack
    if (!FLAC__stream_decoder_process_until_end_of_metadata (_decoder)) {
	apError("FlacStream::open(): decoder error");    
	return false;
    }
    // now that we've opened the stream, tell the engine it's safe to 
    // initialize itself.
    
    if (!_engine->init ()) {
	apError("FlacStream::open(): engine init failed");
	return false;
    }
    // return the metaCallBack's status
    return _mcbSuccess;
    
} // FlacStream::open


bool
FlacStream::processOneFrame ()
{
    if (!_decoder)
	return false;

    return FLAC__stream_decoder_process_single (_decoder);

} // FlacStream::processOneFrame


void
FlacStream::realMetaCallBack (const FLAC__StreamMetadata * md)
{
    if (!md)
    {
	apError ("FlacStream::realMetaCallBack(): no stream metadata");
	_mcbSuccess = false;
	return;
    }

    // we only care about STREAMINFO metadata blocks
    if (md->type != FLAC__METADATA_TYPE_STREAMINFO)

	// don't touch _mcbSuccess, it might have been set by
	// a previous call to metaCallBack with a STREAMINFO metadata
	// block
	return;

    // assume the worst!
    _mcbSuccess = false;
    
    if (!FlacEngine::supportsBlockSizes (md->data.stream_info.min_blocksize,
					 md->data.stream_info.max_blocksize))
	return;
    if (!FlacEngine::supportsChannels (md->data.stream_info.channels))
        return;
    if (!FlacEngine::supportsBps (md->data.stream_info.bits_per_sample))
	return;

    _sampPerBlock = md->data.stream_info.max_blocksize;
    _sampleRate   = md->data.stream_info.sample_rate;
    _channels     = md->data.stream_info.channels;
    _bps          = md->data.stream_info.bits_per_sample;
    _totalSamp    = md->data.stream_info.total_samples;

    _mcbSuccess   = true;
    return;

} // FlacStream::realMetaCallBack


// static
void
FlacStream::metaCallBack (const FLAC__StreamDecoder * decoder,
			  const FLAC__StreamMetadata * md,
			  void * client_data)
{
    FlacStream * f = (FlacStream *) client_data;

    if (!f)
    {
	f->apError ("FlacStream::metaCallBack(): no client data");
	return;
    }

    f->realMetaCallBack (md);
    
} // FlacStream::metaCallBack


void
FlacStream::realErrCallBack (const char * name,
			     FLAC__StreamDecoderErrorStatus status)
{
    switch (status)
    {
    case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
	apError ("%s: the decoder lost synchronization", name);
	break;

    case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
	apError ("%s: corrupted frame header", name);
	break;

    case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
	apError ("%s: frame CRC error", name);
	break;

    default:
	apError ("%s: an unknown error occurred", name);
    }

} // FlacStream::realErrCallBack


// static
void
FlacStream::errCallBack (const FLAC__StreamDecoder * decoder,
			 FLAC__StreamDecoderErrorStatus status,
			 void * client_data)
{
    FlacStream * f = (FlacStream *) client_data;
    if (!f)
    {
	f->apError ("FlacStream::errCallBack (): no client data");
	return;
    }

    f->realErrCallBack ("FLAC", status);

} // FlacStream::errCallBack


FLAC__StreamDecoderWriteStatus
FlacStream::realWriteCallBack (const FLAC__Frame * frame,
			       const FLAC__int32 * const buffer[])
{
    return _engine->writeBuf (frame, buffer, channels (), bps ()) ? 
	FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE : 
	FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

} // FlacStream::realWriteCallBack


// static
FLAC__StreamDecoderWriteStatus
FlacStream::writeCallBack (const FLAC__StreamDecoder *,
			 const FLAC__Frame * frame,
			 const FLAC__int32 * const buffer[],
			 void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    
    FlacStream * f = (FlacStream *) client_data;
    if (!f)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

    return f->realWriteCallBack (frame, buffer);
    
} // FlacStream::writeCallBack


FLAC__StreamDecoderReadStatus
FlacStream::realReadCallBack (FLAC__byte buffer[], size_t * bytes)
{
    *bytes = reader_read (buffer, *bytes, _datasource);
    return *bytes > 0 ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE :
	reader_eof (_datasource) ? 
	FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : 
	FLAC__STREAM_DECODER_READ_STATUS_ABORT;

} // FlacStream::realReadCallBack


// static
FLAC__StreamDecoderReadStatus
FlacStream::readCallBack (const FLAC__StreamDecoder *,
			  FLAC__byte buffer[],
			  size_t * bytes,
			  void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    FlacStream * f = (FlacStream *) client_data;
    if (!f)
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    return f->realReadCallBack (buffer, bytes);

} // FlacStream::readCallBack

} // namespace Flac
