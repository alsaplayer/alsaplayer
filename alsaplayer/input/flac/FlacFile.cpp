//-------------------------------------------------------------------------
//  This class encapsulates the flac file stream.  Only the functions
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
#include "FlacFile.h"
#include "FlacTag.h"

#include <string.h>
#include "alsaplayer_error.h"

using namespace std;

namespace Flac
{

// static
bool
FlacFile::isFlacFile (const string & path)
{
    // Odd, I tried to make this more accurate by creating a file decoder and
    // processing the metadata, but FLAC__file_decoder_process_metadata
    // seems to succeed regardless of the file type, so for now I just
    // do extension checking.

    char * ext = strrchr (path.c_str (), '.');
    if (!ext)
	return false;
    ext++;
    string x (ext);
    return x == "flac" || x == "fla";
    
} // FlacFile::isFlacFile


FlacFile::FlacFile (const string & path, bool md5)
    : _decoder (0),
      _engine (0),
      _tag (0),
      _path (path),
      _state ("uninitialized"),
      _md5 (md5),
      _mcbSuccess (false),
      _channels (0),
      _bps (0),
      _sampleRate (1),
      _sampPerBlock (0),
      _totalSamp (0)
{
} // FlacFile::FlacFile


FlacFile::~FlacFile ()
{
    if (_decoder)
    {
	FLAC__file_decoder_finish (_decoder);
	FLAC__file_decoder_delete (_decoder);
	_decoder = 0;
    }
    delete _engine;
    _engine = 0;
    delete _tag;
    _tag = 0;

} // FlacFile::~FlacFile


bool
FlacFile::open ()
{
    // it's illegal to call this on an already open file
    if (_decoder)
	return false;

    _decoder = FLAC__file_decoder_new ();
    if (!_decoder)
	return false;

    bool status = true;
    status &= FLAC__file_decoder_set_filename (_decoder, _path.c_str ());
    status &= FLAC__file_decoder_set_md5_checking (_decoder, _md5);
    status &= FLAC__file_decoder_set_write_callback (_decoder, 
						     writeCallBack);
    status &= FLAC__file_decoder_set_metadata_callback (_decoder,
							metaCallBack);
    status &= FLAC__file_decoder_set_error_callback (_decoder,
						     errCallBack);
    status &= FLAC__file_decoder_set_client_data (_decoder, (void *) this);

    if (!status)
	return false;

    status = (FLAC__file_decoder_init (_decoder) == FLAC__FILE_DECODER_OK);
    
    if (!status)
	return false;

    // this will invoke the metaCallBack
    if (!FLAC__file_decoder_process_metadata (_decoder))
	return false;

    // now that we've opened the file, tell the engine it's safe to 
    // initialize itself.
    
    if (_engine && !_engine->init ())
	return false;
    
    // return the metaCallBack's status
    return _mcbSuccess;
    
} // FlacFile::open


const string &
FlacFile::state ()
{
    if (_decoder)
	_state = FLAC__FileDecoderStateString[FLAC__file_decoder_get_state (_decoder)];
    else
	_state = "uninitialized";
    return _state;

} // FlacFile::state


bool
FlacFile::processOneFrame ()
{
    if (!_decoder)
	return false;

    return FLAC__file_decoder_process_one_frame (_decoder);

} // FlacFile::processOneFrame


bool
FlacFile::seekAbsolute (FLAC__uint64 sample)
{
    if (!_decoder)
	return false;

    return FLAC__file_decoder_seek_absolute (_decoder, sample);

} // FlacFile::seekAbsolute


// static
void
FlacFile::metaCallBack (const FLAC__FileDecoder * decoder,
			const FLAC__StreamMetaData * md,
			void * client_data)
{
    FlacFile * f = (FlacFile *) client_data;

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

} // FlacFile::metaCallBack


// static
void
FlacFile::errCallBack (const FLAC__FileDecoder * decoder,
		       FLAC__StreamDecoderErrorStatus status,
		       void * client_data)
{
    switch (status)
    {
    case FLAC__STREAM_DECODER_ERROR_LOST_SYNC:
	alsaplayer_error ("flac: decoder lost sync.\n");
	break;

    case FLAC__STREAM_DECODER_ERROR_BAD_HEADER:
	alsaplayer_error ("flac: the decoder encountered a bad header.\n");
	break;

    case FLAC__STREAM_DECODER_ERROR_FRAME_CRC_MISMATCH:
	alsaplayer_error ("flac: frame CRC error.\n");
	break;

    default:
	alsaplayer_error ("flac: an unknown error occurred.\n");
    }

} // FlacFile::errCallBack


// static
FLAC__StreamDecoderWriteStatus
FlacFile::writeCallBack (const FLAC__FileDecoder * decoder,
			 const FLAC__Frame * frame,
			 const FLAC__int32 * buffer [],
			 void * client_data)
{
    if (!client_data)
	return FLAC__STREAM_DECODER_WRITE_ABORT;
    
    FlacFile * f = (FlacFile *) client_data;
    if (!f || !f->_engine)
	return FLAC__STREAM_DECODER_WRITE_ABORT;
    return f->_engine->writeBuf (frame, buffer, f->channels (), f->bps ()) ? 
	   FLAC__STREAM_DECODER_WRITE_CONTINUE : 
	   FLAC__STREAM_DECODER_WRITE_ABORT;
    
} // FlacFile::writeCallBack

} // namespace Flac
