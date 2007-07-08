/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

 *  This file is part of the MIDI input plugin for AlsaPlayer.
 *
 *  The MIDI input plugin for AlsaPlayer is free software;
 *  you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software
 *  Foundation; either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  The MIDI input plugin for AlsaPlayer is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.

   readmidi.h 
   
   */

/*
typedef struct {
  MidiEvent event;
  void *next;
} MidiEventList;
*/
/*extern int32 quietchannels;*/

extern void read_midi_file(struct md *d);

#ifdef INFO_ONLY
struct meta_text_type {
    unsigned long time;
    unsigned char type;
    char *text;
    struct meta_text_type *next;
};

extern struct meta_text_type *meta_text_list;
#endif
