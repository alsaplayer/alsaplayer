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
      _apFramesPerFlacFrame (1),
      _currSamp (0),
      _currApFrame (0),
      _lastDecodedFrame (-1)
{
} // FlacEngine::FlacEngine


FlacEngine::FlacEngine ()
    : _f (0),
      _buf (0),
      _apFramesPerFlacFrame (1),
      _currSamp (0),
      _currApFrame (0),
      _lastDecodedFrame (-1)
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
    // Calculate the number of AlsaPlayer frames in a flac frame.
    // This number must be chosen such that apFrameSize is no greater
    // than BUF_SIZE (see alsaplayer/CorePlayer.h).  It should
    // also be a power of 2 so that it nicely divides the stream's
    // samplesPerBlock without fractions, in order to prevent rounding
    // errors when converting AlsaPlayer frame numbers to flac sample
    // numbers and vice versa.
    //
    // I believe this algorithm meets those 2 criteria for all valid
    // block sizes in flac 1.02.

    int tryme;
    for (tryme = 1; tryme <= 32; tryme *= 2)
    {
	if ((_f->samplesPerBlock () * AP_CHANNELS * AP_BYTES_PER_SAMPLE /
	     tryme) <= BUF_SIZE)
	    break;
    }
    if (tryme <= 32)
    {
	_apFramesPerFlacFrame = tryme;
	return true;
    } else {
	// ugh, give up, too big
	alsaplayer_error("FlacEngine::init(): frame size too big"); 
	return false;
    }
} // FlacEngine::init


int
FlacEngine::apFrameSize () const
{
    if (!_f)
	return 0;

    // AlsaPlayer wants us to return a fixed-size audio frame,
    // regardless of what's actually in the audio stream.
    //
    // The flac block size is in units of samples. Each flac frame
    // we decode contains a block of samples for each channel in the
    // audio stream.
    //
    // The AlsaPlayer frame size is in units of bytes.  If the
    // audio is mono, we'll have to copy the data from the single
    // flac channel into the 2nd AlsaPlayer channel.
    //
    // So the AlsaPlayer frame size is the flack block size (samples
    // per channel) times AP_CHANNELS times AP_BYTES_PER_SAMPLE divided
    // by the number of AlsaPlayer frames per flac frame.

    return _f->samplesPerBlock () * AP_CHANNELS * AP_BYTES_PER_SAMPLE /
	   _apFramesPerFlacFrame;

} // FlacEngine::apFrameSize


int
FlacEngine::apFrames () const
{
    if (!_f)
	return 0;

    return (int) ceilf (_apFramesPerFlacFrame *
			((float) _f->totalSamples () /
			 (float) _f->samplesPerBlock ()));

} // FlacEngine::apFrames


float
FlacEngine::frameTime (int frame) const
{
    if (!_f)
	return 0;

    float pos = ((float) frame / (float) _apFramesPerFlacFrame) *
	        _f->samplesPerBlock ();
    return pos / (float) _f->sampleRate ();

} // FlacEngine::frameTime


bool
FlacEngine::seekToFrame (int frame)
{
    if (!_f || frame < 0 || frame > apFrames ())
	return false;

    // Some flac decoder interfaces allow us to seek to an exact sample,
    // so translate the frame number to the corresponding sample
    // number.  Don't actually seek to this sample yet, we do that later
    // in flac_play_frame.

    _currSamp = (FLAC__uint64) (_f->samplesPerBlock () * 
				((float) frame / 
				 (float) _apFramesPerFlacFrame));
    _currApFrame = frame;

    return true;

} // FlacEngine::seekToFrame


bool
FlacEngine::decodeFrame (char * buf)
{
    if (!_f || !buf)
	return false;

    // For some reason I haven't figured out yet, the flac library
    // won't return an error when we try to process a frame beyond
    // the total number of samples, nor will it immediately set
    // the decoder state to EOF when there are no more samples
    // to read.  So we check the current sample number here and
    // if it's greater than the total number of samples, return false.

    if (_currSamp >= _f->totalSamples ())
	return false;

    // Optimize the case where there's a 1:1 ratio of AlsaPlayer frames
    // to flac frames.

    if (_apFramesPerFlacFrame == 1)
	_buf = buf;
    else if (!_buf)
	_buf = new char[apFrameSize () * _apFramesPerFlacFrame];


    // Figure out where the next batch of AlsaPlayer samples comes from

    bool status;
    int currFrame = _currSamp / _f->samplesPerBlock ();
    if (currFrame == _lastDecodedFrame)
    {
	// Next AlsaPlayer frame is contained in the last decoded flac frame
	status = true;
    }

    else if (currFrame == _lastDecodedFrame + 1)
    {
	// Next AlsaPlayer frame is the contained in the next flac frame
	status = _f->processOneFrame ();
	if (status)
	    ++_lastDecodedFrame;
    }

    else
    {
	// Seek to find the next AlsaPlayer frame.  Note that by always
	// seeking to a flac frame boundary, we can use
	// _currApFrame % _apFramesPerFlacFrame to determine
	// the AlsaPlayer frame offset within _buf.

	status = _f->seekAbsolute (currFrame * _f->samplesPerBlock ());
	if (status)
	    _lastDecodedFrame = currFrame;
    }

    if (status)
    {
	if (_buf != buf)
	{
	    memcpy (buf, 
		    _buf + (apFrameSize () * 
			    (_currApFrame % _apFramesPerFlacFrame)),
		    apFrameSize ());
	}
	else
	{
	    // reset the internal buffer pointer
	    _buf = 0;
	}

	// If we chose _apFramesPerFlacFrame well, this conversion
	// will always be exact

	_currSamp += _f->samplesPerBlock () / _apFramesPerFlacFrame;
	_currApFrame++;
    }
    else
    {
	if (_buf == buf)
	    _buf = 0;
    }
    
    return status;

} // FlacEngine::decodeFrame


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
FlacEngine::writeBuf (const FLAC__Frame * frame,
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
    // Decode a whole flac frame's worth of AlsaPlayer frames at once.

    if (supportsBps (bps))
	writeAlsaPlayerBuf (apFrameSize () * _apFramesPerFlacFrame / 
			    AP_BYTES_PER_SAMPLE,
			    left, right, frame->header.blocksize,
			    bps == 8 ? 8 : 0);
    else
	return false;

    return true;
	
} // FlacEngine::writeBuf

} // namespace Flac
