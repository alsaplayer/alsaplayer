//-------------------------------------------------------------------------
//  This class encapsulates the flac stream.  Only the functions
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
#include "FlacStream.h"
#include "FlacTag.h"

#include <string.h>
#include "alsaplayer_error.h"

using namespace std;

namespace Flac
{

// static
bool
FlacStream::isFlacStream (const string & name)
{
    // XXX dhess - need a better way of doing this.

    char * ext = strrchr (name.c_str (), '.');
    if (!ext)
	return false;
    ext++;
    string x (ext);
    return x == "flac" || x == "fla";
    
} // FlacStream::isFlacStream


FlacStream::FlacStream (const string & name, reader_type * f)
    : _decoder (0),
      _datasource (f),
      _engine (0),
      _tag (0),
      _name (name),
      _mcbSuccess (false),
      _channels (0),
      _bps (0),
      _sampleRate (1),
      _sampPerBlock (0),
      _totalSamp (0)
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
	alsaplayer_error("FlacStream::open(): existing decoder");    
	return false;
    }

    _decoder = FLAC__stream_decoder_new ();
    if (!_decoder) {
	alsaplayer_error("FlacStream::open(): error creating FLAC__stream_decoder");
	return false;
    }
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
	alsaplayer_error("FlacStream::open(): status error, huh?");    
	return false;
    }
    status = (FLAC__stream_decoder_init (_decoder) == FLAC__STREAM_DECODER_SEARCH_FOR_METADATA);
    
    if (!status) {
	alsaplayer_error("FlacStream::open(): can't initialize stream decoder");    
	return false;
    }

    // this will invoke the metaCallBack
    if (!FLAC__stream_decoder_process_until_end_of_metadata (_decoder)) {
	alsaplayer_error("FlacStream::open(): decoder error");    
	return false;
    }
    // now that we've opened the stream, tell the engine it's safe to 
    // initialize itself.
    
    if (_engine && !_engine->init ()) {
	alsaplayer_error("FlacStream::open(): engine init failed (_engine = %x)",
			_engine);    
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


// static
void
FlacStream::metaCallBack (const FLAC__StreamDecoder * decoder,
			const FLAC__StreamMetadata * md,
			void * client_data)
{
    FlacStream * f = (FlacStream *) client_data;

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

} // FlacStream::metaCallBack


// static
void
FlacStream::errCallBack (const FLAC__StreamDecoder * decoder,
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

} // FlacStream::errCallBack


// static
FLAC__StreamDecoderWriteStatus
FlacStream::writeCallBack (const FLAC__StreamDecoder * decoder,
			 const FLAC__Frame * frame,
			 const FLAC__int32 * const buffer[],
			 void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    
    FlacStream * f = (FlacStream *) client_data;
    if (!f || !f->_engine)
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    return f->_engine->writeBuf (frame, buffer, f->channels (), f->bps ()) ? 
	   FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE : 
	   FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    
} // FlacStream::writeCallBack


// static
FLAC__StreamDecoderReadStatus
FlacStream::readCallBack (const FLAC__StreamDecoder * decoder,
			  FLAC__byte buffer[],
			  unsigned * bytes,
			  void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    FlacStream * f = (FlacStream *) client_data;
    if (!f || !f->_engine)
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    *bytes = reader_read (buffer, *bytes, f->_datasource);
    return *bytes > 0 ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE :
	reader_eof (f->_datasource) ? 
	FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : 
	FLAC__STREAM_DECODER_READ_STATUS_ABORT;

} // FlacStream::readCallBack

} // namespace Flac
