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


#ifndef _FLAC_ENGINE_H_
#define _FLAC_ENGINE_H_

extern "C"
{
#include <FLAC/stream_decoder.h>
}

namespace Flac
{

class FlacStream;

class FlacEngine
{
 public:

    //--------------------------------------------------------
    // Does the engine support these min and max block sizes?
    //--------------------------------------------------------

    static bool supportsBlockSizes (unsigned int min, unsigned int max);


    //-----------------------------------------------------------------
    // Does the engine support a flac stream with the indicated number
    // of channels?
    //-----------------------------------------------------------------

    static bool supportsChannels (unsigned int nchannels);


    //--------------------------------------------------
    // Supports the indicated number of bits per sample?
    //--------------------------------------------------

    static bool supportsBps (unsigned int bps);


 public:

    FlacEngine (FlacStream * f);
    ~FlacEngine ();


    //---------------------------------------------------------
    // Get the number of channels in an AlsaPlayer audio frame.
    //---------------------------------------------------------

    int   apChannels () const;


    //------------------------------------------------------------------
    // Get the number of bytes per sample in an AlsaPlayer audio stream.
    //------------------------------------------------------------------

    int   apBytesPerSample () const;


    //------------------------------------------------------
    // Get the number of bytes in an AlsaPlayer audio frame.
    //------------------------------------------------------

    int   apFrameSize () const;


    //-----------------------------------------------------------------
    // Get the number of AlsaPlayer audio frames contained in the flac
    // audio stream to which the engine is attached.
    //-----------------------------------------------------------------

    int   apFrames () const;


    //-------------------------------------------------------------------
    // Given an AlsaPlayer frame number, return the time in seconds at
    // which the frame is played relative to the beginning of the stream.
    //-------------------------------------------------------------------

    float frameTime (int frame) const;


    //----------------------------------------------------------------
    // Seek to an AlsaPlayer frame in the stream.  The real seek is 
    // deferred until the next time the frame method is invoked.
    //
    // Returns false if the seek is illegal, in which case the current
    // frame position is not changed.
    //----------------------------------------------------------------

    bool  seekToFrame (int frame);


    //----------------------------------------------------------------------
    // Decode the current AlsaPlayer frame, placing the uncompressed audio 
    // samples info a pre-allocated buffer buf.  The buffer must be at 
    // least as large as an AlsaPlayer frame (use apFrameSize to get this
    // information from the engine).
    //
    // Returns false if the decode process fails, or if the stream is
    // out of samples.
    //----------------------------------------------------------------------

    bool  decodeFrame (char * buf);


 private:

    FlacEngine ();


    //--------------------------------------------------------------------
    // Initialize the engine.  Called by a flac stream object after it
    // opens the stream.
    //
    // Returns true if successfully initialized, false otherwise.
    //------------------------------------------------------------------

    bool  init ();


    //-------------------------------------------------------------------
    // Write the uncompressed audio data into the buffer set with setBuf.
    //-------------------------------------------------------------------

    bool  writeBuf (const FLAC__Frame * frame, const FLAC__int32 * const buffer[],
		    unsigned int flacSamples, unsigned int bps);


    //-------------------------------------------------------------------
    // Fill an AlsaPlayer buffer (16-bit 2-channel PCM audio, left/right
    // interleave) with decoded flac audio data.  Pad the AlsaPlayer
    // buffer with silence if necessary.
    //-------------------------------------------------------------------

    void writeAlsaPlayerBuf (unsigned int apSamps, const FLAC__int32 * ch0,
			     const FLAC__int32 * ch1, unsigned int flacSamps,
			     int shift);


    friend class FlacStream;
    friend class FlacSeekableStream;
    friend class OggFlacStream;

 private:

    FlacStream *       _f;
    char *           _buf;
    int              _apFramesPerFlacFrame;


    //-------------------
    // Stream accounting.
    //-------------------

    FLAC__uint64     _currSamp;          // current sample position
    int              _currApFrame;       // current AlsaPlayer frame
    int              _lastDecodedFrame;  // last flac frame that was
                                         // successfully decoded


    //-----------------------------------------------------------
    // AlsaPlayer always wants signed 16-bit data and 2 channels.
    //-----------------------------------------------------------

    static const unsigned int AP_BYTES_PER_SAMPLE = 2;
    static const unsigned int AP_CHANNELS = 2;

}; // class FlacEngine


//----------------
// Inline methods.
//----------------


// static
inline bool
FlacEngine::supportsBlockSizes (unsigned int min, unsigned int max)
{
    // The flac format doesn't require a uniform block size for the stream,
    // but the reference encoder is implemented this way, which makes our
    // life much simpler.  Let's do a sanity check just to make sure.

    return min == max && min >= FLAC__MIN_BLOCK_SIZE && 
	   max <= FLAC__MAX_BLOCK_SIZE;
}

// static
inline bool
FlacEngine::supportsChannels (unsigned int nchannels)
{
    return nchannels > 0 && nchannels <= FLAC__MAX_CHANNELS;
}

// static
inline bool
FlacEngine::supportsBps (unsigned int bps)
{
    return bps == 8 || bps == 16;
}

inline int
FlacEngine::apChannels () const
{
    return AP_CHANNELS;
}

inline int
FlacEngine::apBytesPerSample () const
{
    return AP_BYTES_PER_SAMPLE;
}

} // namespace Flac

#endif /* _FLAC_ENGINE_H_ */
