//-------------------------------------------------------------------------
//  This class encapsulates a flac file stream.  Only the functions
//  needed by the alsaplayer flac plugin are implemented.
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


#ifndef _FLAC_FILE_H_
#define _FLAC_FILE_H_

#include <string>

extern "C"
{
#include <FLAC/file_decoder.h>
}

#undef DEBUG

namespace Flac
{

class FlacEngine;
class FlacTag;

class FlacFile
{
 public:

    //-----------------------------------------------------------
    // Return true if we think path points to  a valid flac file.
    //-----------------------------------------------------------

    static bool isFlacFile (const std::string & path);


 public:

    //--------------------------
    // Constructor & destructor.
    //--------------------------

    FlacFile (const std::string & path, bool md5);

    ~FlacFile ();


    //----------------------------------------------------------------
    // Initialize the file decoder, installing all callbacks from the
    // engine.  Returns false if the decoder could not be initialized.
    //----------------------------------------------------------------

    bool                open ();


    //-------------------------------------------------------
    // Get a pointer to the FlacEngine for this file decoder.
    //-------------------------------------------------------

    FlacEngine *        engine () const;


    //------------------------------------------
    // Set the FlacEngine for this file decoder.
    //------------------------------------------

    void                setEngine (FlacEngine * engine);


    //-------------------------------------------------------------------
    // Get a pointer to the FlacTag for this file, or 0 if there is none.
    //-------------------------------------------------------------------

    FlacTag *           tag () const;


    //-------------------------------
    // Set the FlacTag for this file.
    //-------------------------------

    void                setTag (FlacTag * tag);


    //---------------------------------------------------------
    // Check whether the file stream decoder is in an OK state.
    //---------------------------------------------------------

    bool                ok () const;

    
    //-----------------------------------
    // Get the pathname of the flac file.
    //-----------------------------------

    const std::string & path () const;


    //-----------------------------------------------------------------
    // Get a string from the flac library describing the status of the
    // file decoder.
    //-----------------------------------------------------------------

    const std::string & state ();


    //------------------------------------------------------
    // Get the sample rate of the uncompressed audio stream.
    //------------------------------------------------------

    unsigned int        sampleRate () const;


    //----------------------------------------
    // Number of channels in the audio stream.
    //----------------------------------------

    unsigned int        channels () const;


    //----------------------------------------------------------
    // Total number of uncompressed samples in the audio stream.
    //----------------------------------------------------------

    FLAC__uint64        totalSamples () const;


    //--------------------------------------------------------------
    // The number of uncompressd samples in one block of audio data.
    //--------------------------------------------------------------

    unsigned int        samplesPerBlock () const;


    //-----------------
    // Bits per sample.
    //-----------------

    unsigned int        bps () const;


    //----------------------------------------------------------------
    // Process the next flac frame in the stream.  This has the side-
    // effect of calling the write callback if there is data to fetch
    // in the stream.
    //
    // Returns false if there's no more data in the stream, or if
    // there's an error in the file decoder.
    //----------------------------------------------------------------

    bool                processOneFrame ();


    //-------------------------------------------------------------------
    // Seek to a particular sample.  This has the side-effect of calling
    // the write callback if there is data to fetch in the stream.
    //
    // Returns false if the seek is illegal, or if there's an error in
    // the file decoder.
    //-------------------------------------------------------------------

    bool                 seekAbsolute (FLAC__uint64 sample);


 private:

    //-----------------------------------------------------------------
    // The flac metadata callback.  It's called as a side effect of the
    // open method.  It'll be called once for each metadata block in
    // the flac file.  To check its success, look at _mcbSuccess.
    //-----------------------------------------------------------------

    static void metaCallBack (const FLAC__FileDecoder * decoder,
			      const FLAC__StreamMetadata * md,
			      void * cilent_data);

    static FLAC__StreamDecoderWriteStatus 
	writeCallBack (const FLAC__FileDecoder * decoder,
		       const FLAC__Frame * frame,
		       const FLAC__int32 * const buffer[],
		       void * client_data);

    static void errCallBack (const FLAC__FileDecoder * decoder,
			     FLAC__StreamDecoderErrorStatus status,
			     void * client_data);


 private:

    FlacFile ();


 private:

    FLAC__FileDecoder * _decoder;
    FlacEngine *        _engine;
    FlacTag *           _tag;
    const std::string   _path;
    std::string         _state;
    bool                _md5;
    bool                _mcbSuccess;  // was metaCallBack successful?


    //---------------------------------------------------------------------
    // Stream info.  "Samples" always refers to uncompressed audio samples.
    //---------------------------------------------------------------------

    unsigned int        _channels;      // number of channels in the stream
    unsigned int        _bps;           // bits per sample
    unsigned int        _sampleRate;    // number of samples per channel per sec
    unsigned int        _sampPerBlock;  // number of samples per block of data
    FLAC__uint64        _totalSamp;     // total number of samples in the stream

}; // class FlacFile


//----------------
// Inline methods.
//----------------

inline FlacTag *
FlacFile::tag () const
{
    return _tag;
}

inline FlacEngine *
FlacFile::engine () const
{
    return _engine;
}

inline void
FlacFile::setEngine (FlacEngine * engine)
{
    _engine = engine;
}

inline void
FlacFile::setTag (FlacTag * tag)
{
    _tag = tag;
}

inline bool
FlacFile::ok () const
{
    
    return _decoder ? FLAC__file_decoder_get_state (_decoder) == 
	                  FLAC__FILE_DECODER_OK :
	              0;
}

inline const std::string &
FlacFile::path () const
{
    return _path;
}


inline unsigned int
FlacFile::sampleRate () const
{
    return _sampleRate;
}

inline FLAC__uint64
FlacFile::totalSamples () const
{
    return _totalSamp;
}

inline unsigned int
FlacFile::samplesPerBlock () const
{
    return _sampPerBlock;
}

inline unsigned int
FlacFile::bps () const
{
    return _bps;
}

inline unsigned int
FlacFile::channels () const
{
    return _channels;
}

} // namespace Flac

#endif // _FLAC_FILE_H_
