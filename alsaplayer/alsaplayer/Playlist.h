/*  Playlist.h
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 *  $Id$
 *
*/ 

#ifndef __Playlist_h__
#define __Playlist_h__
#include "CorePlayer.h"
#include <vector>
#include <string>
#include <set>

#define MAGIC_ID	"# 1.0.0 (Do not edit!)"

enum plist_result {E_PL_SUCCESS = 0, E_PL_DUBIOUS, E_PL_BAD};
enum plist_format {PL_FORMAT_M3U};

class PlayItem
{
	private:
		bool parsed;
	public:
		PlayItem(std::string filename_new) {
			filename = filename_new;
			playtime = 0;
			parsed = false;
		}
		bool Parsed() { return parsed; }
		void SetParsed() { parsed = true; }
		std::string filename;
		std::string title;
		std::string author;
		int playtime;
};

class PlaylistInterface
{
	private:
	public:
		// Note: it is not permissible to call any Playlist methods in
		// one of these callback methods - will cause deadlock.
		// Callbacks - Called when:

		virtual void CbLock() = 0;
		virtual void CbUnlock() = 0;
		
		// Current position changed
		virtual void CbSetCurrent(unsigned pos) = 0;

		// Some items were inserted
		// Note: pos is the position to insert at
		// So - pos == 0  means insert items at beginning
		//      pos == n  means insert at end, ie append, where n is number of
		//      items already in list
		// Note: CbSetCurrent will be called after this callback.
		virtual void CbInsert(std::vector<PlayItem> &items, unsigned pos) = 0;

		virtual void CbUpdated(PlayItem &item, unsigned pos) = 0;
		
		// Tracks from position start to end inclusive were removed
		// Note: CbSetCurrent will be called after this callback.
		virtual void CbRemove(unsigned, unsigned) = 0;

		// List was been cleared.  Current position is now 0.
		virtual void CbClear() = 0;
};

class Playlist
{
friend void playlist_looper(void *data);
friend void insert_looper(void *);
friend void info_looper(void *);
private:
		CorePlayer *player1;
		CorePlayer *player2;


   // Mutex to stop moving onto next song while we're modifying the playlist
	pthread_mutex_t playlist_mutex;
	
	// Thread which starts new song when previous one finishes
	// -- would be nice to get rid of this eventually...
	// (perhaps by setting a callback on the player to be called
	// when the song finishes)
	pthread_t playlist_thread;

	// Added thread
	pthread_t adder;

	// Flags used by thread to exit neatly
	bool active;    // True until set to false by destructor
	bool paused;	// Playlist is paused
	bool loopingSong;	//  Loop the current song
	bool loopingPlaylist;	// Loop the Playlist
	bool crossfade; // Crossfade the playlist

	CorePlayer *coreplayer; // Core player - set this

	std::vector<PlayItem> queue;	// List of files to play
	unsigned curritem;		// Position of next file to play

	std::set<PlaylistInterface *> interfaces;  // Things to tell when things change

	void Looper(void *data);
public:	
	void Stop();
	bool PlayFile(PlayItem const &);
	bool CanPlay(std::string const &);
	
	void Lock();
	void Unlock();

	Playlist(CorePlayer *);
	Playlist(AlsaNode *);
	~Playlist();


	// Get CorePLayer object
	CorePlayer *GetCorePlayer() { return coreplayer; }
	
	// Get the number of items in the playlist (0 if playlist is empty)
	unsigned Length();

	// Move to specified item in playlist and play from there
	// Position 1 is first item, n is last item where n is length of list
	void Play(unsigned, int locking=0);

	void Next(int locking=0);    // Start playing next item in playlist
	void Prev(int locking=0);    // Start playing previous item in playlist
	int GetCurrent() { return curritem; } // Return current item
	
	// Insert items at position - 0 = beginning, 1 = after first item, etc
	void Insert(std::vector<std::string> const &, unsigned);

	// To insert just one item:
	void Insert(std::string const &, unsigned);

	// Add several items and play them immediately
	// (Avoids possible concurrency problems)
	void AddAndPlay(std::vector<std::string> const &);

	// Add just one item and play it
	void AddAndPlay(std::string const &);

	// Remove tracks from position start to end inclusive
	// Position 1 is first track, n is last track where n is length of list
	void Remove(unsigned start, unsigned end);

	// Shuffle playlist
	void Shuffle(int locking=0);
	
	// Clear playlist
	void Clear(int locking=0);

	// Pause controls
	bool Paused() { return paused; }
	void Pause() { paused = true; }
	void UnPause() { paused = false; }

	// Crossfade controls
	bool Crossfading() { return crossfade; }
	void Crossfade() { crossfade = true; }
	void UnCrossfade() { crossfade = false; }
	
	// Loop_Song controls
	bool LoopingSong() { return loopingSong; }
	void LoopSong() { loopingSong = true; }
	void UnLoopSong() { loopingSong = false; }

	// Loop_Playlist controls
	bool LoopingPlaylist() { return loopingPlaylist; }
	void LoopPlaylist() { loopingPlaylist = true; }
	void UnLoopPlaylist() { loopingPlaylist = false; }
	
	// Save playlist to file
	enum plist_result Save(std::string, enum plist_format) const;

	// Load playlist from file
	enum plist_result Load(std::string const &, unsigned, bool);

	// Register to receive callbacks
	void Register(PlaylistInterface *);

	// Unregister - must do this before a registered interface is deleted
	void UnRegister(PlaylistInterface *, int locking=0);
};

inline void Playlist::Insert(std::string const &path, unsigned pos) {
		std::vector<std::string> items;
	items.push_back(path);
	Insert(items, pos);
}

inline void Playlist::AddAndPlay(std::string const &path) {
	std::vector<std::string> items;
	items.push_back(path);
	Playlist::AddAndPlay(items);
}

#endif
