//-------------------------------------------------------------------------
//  This class encapsulates the flac seekable stream.  Only the functions
//  needed by the alsaplayer flac plugin are implemented.
//
// Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
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

#include "FlacEngine.h"
#include "FlacSeekableStream.h"

#include "alsaplayer_error.h"

using namespace std;

namespace Flac
{

FlacSeekableStream::FlacSeekableStream (const string & name, reader_type * f)
    : FlacStream (name, f),
      _decoder (0)
{
} // FlacSeekableStream::FlacSeekableStream


FlacSeekableStream::~FlacSeekableStream ()
{
    if (_decoder)
    {
	FLAC__seekable_stream_decoder_finish (_decoder);
	FLAC__seekable_stream_decoder_delete (_decoder);
	_decoder = 0;
    }

} // FlacSeekableStream::~FlacSeekableStream


bool
FlacSeekableStream::open ()
{
    // it's illegal to call this on an already open stream
    if (_decoder) {
	alsaplayer_error("FlacSeekableStream::open(): existing decoder");    
	return false;
    }

    _decoder = FLAC__seekable_stream_decoder_new ();
    if (!_decoder) {
	alsaplayer_error("FlacSeekableStream::open(): error creating FLAC__seekable_stream_decoder");
	return false;
    }
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
	alsaplayer_error("FlacSeekableStream::open(): status error, huh?");    
	return false;
    }
    status = (FLAC__seekable_stream_decoder_init (_decoder) == FLAC__SEEKABLE_STREAM_DECODER_OK);
    
    if (!status) {
	alsaplayer_error("FlacSeekableStream::open(): can't initialize seekable stream decoder");    
	return false;
    }

    // this will invoke the metaCallBack
    if (!FLAC__seekable_stream_decoder_process_until_end_of_metadata (_decoder)) {
	alsaplayer_error("FlacSeekableStream::open(): decoder error");    
	return false;
    }
    // now that we've opened the stream, tell the engine it's safe to 
    // initialize itself.
    
    if (_engine && !_engine->init ()) {
	alsaplayer_error("FlacSeekableStream::open(): engine init failed (_engine = %x)",
			_engine);    
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

    return FLAC__seekable_stream_decoder_process_single (_decoder);

} // FlacSeekableStream::processOneFrame


bool
FlacSeekableStream::seekAbsolute (FLAC__uint64 sample)
{
    if (!_decoder)
	return false;

    return FLAC__seekable_stream_decoder_seek_absolute (_decoder, sample);

} // FlacSeekableStream::seekAbsolute


// static
void
FlacSeekableStream::metaCallBack (const FLAC__SeekableStreamDecoder * decoder,
				  const FLAC__StreamMetadata * md,
				  void * client_data)
{
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;

    if (!f)
	// doh, no way to indicate failure
	return;

    if (!md || !f->_decoder)
    {
	f->_mcbSuccess = false;
	return;
    }

    // we only care about STREAMINFO metadata blocks
    if (md->type != FLAC__METADATA_TYPE_STREAMINFO)

	// don't touch _mcbSuccess, it might have been set by
	// a previous call to metaCallBack with a STREAMINFO metadata
	// block
	return;

    // assume the worst!
    f->_mcbSuccess = false;
    
    if (decoder != f->_decoder)
	return;
    
    if (!FlacEngine::supportsBlockSizes (md->data.stream_info.min_blocksize,
					 md->data.stream_info.max_blocksize))
	return;
    if (!FlacEngine::supportsChannels (md->data.stream_info.channels))
        return;
    if (!FlacEngine::supportsBps (md->data.stream_info.bits_per_sample))
	return;

    f->_sampPerBlock = md->data.stream_info.max_blocksize;
    f->_sampleRate   = md->data.stream_info.sample_rate;
    f->_channels     = md->data.stream_info.channels;
    f->_bps          = md->data.stream_info.bits_per_sample;
    f->_totalSamp    = md->data.stream_info.total_samples;

    f->_mcbSuccess   = true;
    return;

} // FlacSeekableStream::metaCallBack


// static
void
FlacSeekableStream::errCallBack (const FLAC__SeekableStreamDecoder * decoder,
				 FLAC__StreamDecoderErrorStatus status,
				 void * client_data)
{
    switch (status)
    {
    case FLAC__STREAM_DECODER_ABORTED:
	alsaplayer_error ("flac: decoder was aborted by the read callback");
	break;

    case FLAC__STREAM_DECODER_UNPARSEABLE_STREAM:
	alsaplayer_error ("flac: decoder encountered reserved fields in the stream");
	break;

    case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
	alsaplayer_error ("flac: decoder can't allocate memory");
	break;

    case FLAC__STREAM_DECODER_ALREADY_INITIALIZED:
	alsaplayer_error ("flac: decoder was initialized twice");
	break;

    case FLAC__STREAM_DECODER_INVALID_CALLBACK:
	alsaplayer_error ("flac: one or more callbacks is not set");
	break;

    case FLAC__STREAM_DECODER_UNINITIALIZED:
	alsaplayer_error ("flac: decoder is in the uninitialized state");
	break;

    default:
	alsaplayer_error ("flac: an unknown error occurred.\n");
    }

} // FlacSeekableStream::errCallBack


// static
FLAC__StreamDecoderWriteStatus
FlacSeekableStream::writeCallBack (const FLAC__SeekableStreamDecoder * decoder,
				   const FLAC__Frame * frame,
				   const FLAC__int32 * const buffer[],
				   void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f || !f->_engine)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    return f->_engine->writeBuf (frame, buffer, f->channels (), f->bps ()) ? 
	   FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE : 
	   FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    
} // FlacSeekableStream::writeCallBack


// static
FLAC__SeekableStreamDecoderReadStatus
FlacSeekableStream::readCallBack (const FLAC__SeekableStreamDecoder * decoder,
				  FLAC__byte buffer[],
				  unsigned * bytes,
				  void * client_data)
{
    if (!client_data)
	return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
	return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;

    *bytes = reader_read (buffer, *bytes, f->_datasource);
    return *bytes > 0 ? FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK :
	reader_eof (f->_datasource) ? 
	FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK : 
	FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;

} // FlacSeekableStream::readCallBack


// static
FLAC__SeekableStreamDecoderSeekStatus 
FlacSeekableStream::seekCallBack (const FLAC__SeekableStreamDecoder * decoder,
				  FLAC__uint64 offset,
				  void * client_data)
{
    if (!client_data)
	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
    
    return reader_seek (f->_datasource, offset, SEEK_SET) == 0 ?
	FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK :
	FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;

} // FlacSeekableStream::seekCallBack


// static
FLAC__SeekableStreamDecoderTellStatus 
FlacSeekableStream::tellCallBack (const FLAC__SeekableStreamDecoder * decoder,
				  FLAC__uint64 * offset,
				  void * client_data)
{
    if (!client_data)
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR;
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR;

    long result = reader_tell (f->_datasource);
    if (result == -1)
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR;
    *offset = result;
    return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
    
} // FlacSeekableStream::tellCallBack


// static
FLAC__SeekableStreamDecoderLengthStatus
FlacSeekableStream::lengthCallBack (const FLAC__SeekableStreamDecoder * decoder,
				    FLAC__uint64 * len,
				    void * client_data)
{
    if (!client_data)
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
    FlacSeekableStream * f = (FlacSeekableStream *) client_data;
    if (!f)
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;

    long result = reader_length (f->_datasource);
    if (result == -1)
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
    *len = result;
    return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;

} // FlacSeekableStream::lengthCallBack


// static
FLAC__bool
FlacSeekableStream::eofCallBack (const FLAC__SeekableStreamDecoder * decoder,
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
