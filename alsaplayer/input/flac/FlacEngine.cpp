//---------------------------------------------------------------------------
//  This class handles the interface between flac audio streams and
//  AlsaPlayer buffers.
//
//  Copyright (c) 2002 by Drew Hess <dhess@bothan.net>
//
//  Based on wav_engine.c (C) 1999 by Andy Lo A Foe
//  and vorbis_engine.c (C) 2002 by Andy Lo A Foe
//  and mad_engine.c (C) 2002 by Andy Lo A Foe
//  and the flac xmms plugin (C) 2001 by Josh Coalson
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
//--------------------------------------------------------------------------


#include "FlacEngine.h"

#include <iostream>
#include <cmath>
#include <cstring>
#include "FlacStream.h"
#include "CorePlayer.h"
#include "alsaplayer_error.h"

namespace Flac
{

FlacEngine::FlacEngine (FlacStream * f)
    : _f (f),
      _buf (0),
      _apBlocksPerFlacBlock (1),
      _currSamp (0),
      _currApBlock (0),
      _lastDecodedBlock (-1)
{
} // FlacEngine::FlacEngine


FlacEngine::FlacEngine ()
    : _f (0),
      _buf (0),
      _apBlocksPerFlacBlock (1),
      _currSamp (0),
      _currApBlock (0),
      _lastDecodedBlock (-1)
{
} // FlacEngine::FlacEngine


FlacEngine::~FlacEngine ()
{
    delete [] _buf;
    _buf = 0;

    // don't delete _f

} // FlacEngine::~FlacEngine


bool
FlacEngine::init ()
{
    // Calculate the number of AlsaPlayer blocks in a flac block.
    // This number must be chosen such that apBlockSize is no greater
    // than BUF_SIZE (see alsaplayer/CorePlayer.h).  It should
    // also be a power of 2 so that it nicely divides the stream's
    // samplesPerBlock without fractions, in order to prevent rounding
    // errors when converting AlsaPlayer block numbers to flac sample
    // numbers and vice versa.
    //
    // I believe this algorithm meets those 2 criteria for all valid
    // block sizes in flac 1.02.

    int tryme;
    for (tryme = 1; tryme <= 32; tryme *= 2)
    {
	if ((_f->samplesPerBlock () * AP_CHANNELS / tryme) <= BUF_SIZE)
	    break;
    }
    if (tryme <= 32)
    {
	_apBlocksPerFlacBlock = tryme;
	return true;
    } else {
	// ugh, give up, too big
	alsaplayer_error("FlacEngine::init(): block size too big");
	return false;
    }
} // FlacEngine::init


int
FlacEngine::apBlockSize () const
{
    if (!_f)
	return 0;

    // AlsaPlayer wants us to return a fixed-size audio block,
    // regardless of what's actually in the audio stream.
    //
    // The flac block size is in units of samples. Each flac block
    // we decode contains a block of samples for each channel in the
    // audio stream.
    //
    // The AlsaPlayer block size is in units of bytes.  If the
    // audio is mono, we'll have to copy the data from the single
    // flac channel into the 2nd AlsaPlayer channel.
    //
    // So the AlsaPlayer block size is the flack block size (samples
    // per channel) times AP_CHANNELS divided
    // by the number of AlsaPlayer blocks per flac block.

    return _f->samplesPerBlock () * AP_CHANNELS / _apBlocksPerFlacBlock;

} // FlacEngine::apBlockSize


int
FlacEngine::apBlocks () const
{
    if (!_f)
	return 0;

    return (int) ceilf (_apBlocksPerFlacBlock *
			((float) _f->totalSamples () /
			 (float) _f->samplesPerBlock ()));

} // FlacEngine::apBlocks


float
FlacEngine::blockTime (int block) const
{
    if (!_f)
	return 0;

    float pos = ((float) block / (float) _apBlocksPerFlacBlock) *
	        _f->samplesPerBlock ();
    return pos / (float) _f->sampleRate ();

} // FlacEngine::blockTime


bool
FlacEngine::seekToBlock (int block)
{
    if (!_f || block < 0 || block > apBlocks ())
	return false;

    // Some flac decoder interfaces allow us to seek to an exact sample,
    // so translate the block number to the corresponding sample
    // number.  Don't actually seek to this sample yet, we do that later
    // in flac_play_block.

    _currSamp = (FLAC__uint64) (_f->samplesPerBlock () *
				((float) block /
				 (float) _apBlocksPerFlacBlock));
    _currApBlock = block;

    return true;

} // FlacEngine::seekToBlock


bool
FlacEngine::decodeBlock (short * buf)
{
    if (!_f || !buf)
	return false;

    // For some reason I haven't figured out yet, the flac library
    // won't return an error when we try to process a block beyond
    // the total number of samples, nor will it immediately set
    // the decoder state to EOF when there are no more samples
    // to read.  So we check the current sample number here and
    // if it's greater than the total number of samples, return false.

    if (_currSamp >= _f->totalSamples ())
	return false;

    // Optimize the case where there's a 1:1 ratio of AlsaPlayer blocks
    // to flac blocks.

    if (_apBlocksPerFlacBlock == 1)
	_buf = buf;
    else if (!_buf)
	_buf = new short [apBlockSize () * _apBlocksPerFlacBlock];


    // Figure out where the next batch of AlsaPlayer samples comes from

    bool status;
    int currBlock = _currSamp / _f->samplesPerBlock ();
    if (currBlock == _lastDecodedBlock)
    {
	// Next AlsaPlayer block is contained in the last decoded flac block
	status = true;
    }

    else if (currBlock == _lastDecodedBlock + 1)
    {
	// Next AlsaPlayer block is the contained in the next flac block
	status = _f->processOneBlock ();
	if (status)
	    ++_lastDecodedBlock;
    }

    else
    {
	// Seek to find the next AlsaPlayer block.  Note that by always
	// seeking to a flac block boundary, we can use
	// _currApBlock % _apBlocksPerFlacBlock to determine
	// the AlsaPlayer block offset within _buf.

	status = _f->seekAbsolute (currBlock * _f->samplesPerBlock ());
	if (status)
	    _lastDecodedBlock = currBlock;
    }

    if (status)
    {
	if (_buf != buf)
	{
	    memcpy (buf,
		    _buf + (apBlockSize () *
			    (_currApBlock % _apBlocksPerFlacBlock)),
		    apBlockSize ());
	}
	else
	{
	    // reset the internal buffer pointer
	    _buf = 0;
	}

	// If we chose _apBlocksPerFlacBlock well, this conversion
	// will always be exact

	_currSamp += _f->samplesPerBlock () / _apBlocksPerFlacBlock;
	_currApBlock++;
    }
    else
    {
	if (_buf == buf)
	    _buf = 0;
    }

    return status;

} // FlacEngine::decodeBlock


static const FLAC__int16 PCM_SILENCE = 0;

void
FlacEngine::writeAlsaPlayerBuf (unsigned int apSamps,
				const FLAC__int32 * ch0,
				const FLAC__int32 * ch1,
				unsigned int flacSamps,
				int shift)
{
    FLAC__int16 * pcm = (FLAC__int16 *) _buf;
    unsigned int asample, fsample;

    for (asample = 0, fsample = 0; fsample < flacSamps; fsample++)
    {
	pcm[asample++] = (FLAC__int16) (ch0[fsample] << shift);
	pcm[asample++] = (FLAC__int16) (ch1[fsample] << shift);
    }
    for (; asample < apSamps; )
    {
	pcm[asample++] = PCM_SILENCE;
	pcm[asample++] = PCM_SILENCE;
    }

} // FlacEngine::writeAlsaPlayerBuf


bool
FlacEngine::writeBuf (const FLAC__Frame * block,
		      const FLAC__int32 * const buffer[],
		      unsigned int flacChannels,
		      unsigned int bps)
{
    if (!_buf || !_f)
	return false;

    // In flac stereo streams, channel 0 is always left and channel 1
    // is always right.  This matches AlsaPlayer.
    //
    // flac 1.02 doesn't define channel order for > 2 channel sound,
    // so grab the first two channels and hope for the best.

    const FLAC__int32 * left = buffer[0];
    const FLAC__int32 * right = flacChannels == 1 ?
	                        buffer[0] : buffer[1];


    // aww... flac gives us signed PCM data, regardless of the signed-ness
    // of the original audio stream.  How thoughtful.
    //
    // Decode a whole flac block's worth of AlsaPlayer blocks at once.

    if (supportsBps (bps))
	writeAlsaPlayerBuf (apBlockSize () * _apBlocksPerFlacBlock,
			    left, right, block->header.blocksize,
			    bps == 8 ? 8 : 0);
    else
	return false;

    return true;

} // FlacEngine::writeBuf

} // namespace Flac
