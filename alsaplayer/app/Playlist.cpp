/*  PlayList.cpp - playlist window 'n stuff
 *  Copyright (C) 1998-2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#include "Playlist.h"
#include "CorePlayer.h"
#include "utilities.h"
#include "config.h"
#include "alsaplayer_error.h"
#include "reader.h"

#define READBUFSIZE 1024
#define MAXLOADFAILURES 100
#define MAXRECURSEDEPTH 10

static void additems(std::vector<std::string> *items, std::string path, int depth);

// Pointer to sequence of soritng fields.
// This is temporary variable, it is valid while Sort function active.
// Meaning of chars:
//	a - sort by artist in descending direction,
//	A - sort by astist in ascending direction,
//	l - sort by album in descending direction,
//	L - sort by album in ascending direction,
//	t - sort by title in descending direction,
//	T - sort by title in ascending direction,
//	y - sort by year in descending direction,
//	Y - sort by year in ascending direction,
//	n - sort by track number in descending direction,
//	N - sort by track number in ascending direction,
//	c - sort by comment in descending direction,
//	C - sort by comment in ascending direction,
//	f - sort by filename in descending direction,
//	F - sort by filename in ascending direction,
//	g - sort by genre in descending direction,
//	G - sort by genre in ascending direction,
//	p - sort by playtime in descending direction,
//	P - sort by playtime in ascending direction.
static const char *sort_seq;

#define DESCENDING	0
#define ASCENDING	1
#define COMPARE(WHAT,DIRECTION)	{ int rc = a.WHAT.compare(b.WHAT); \
                                  if (rc == 0)  continue; \
                                  return (DIRECTION == DESCENDING) ? rc > 0 : rc < 0; }

// Function is similar to strcmp, but this is for PlayItem type.
// This function uses sort_seq variable. Also this function should 
// be keept optimized for speed.
static int sort_comparator (const PlayItem &a, const PlayItem &b) {
    int ai, bi;

    // For each kind of sorting field
    for (const char *t = sort_seq; *t; t++) {
	switch (*t) {
		case 't':	// Compare titles, descending
				COMPARE(title, DESCENDING);

		case 'T':	// Compare titles, ascending
				COMPARE(title, ASCENDING);

		case 'a':	// Compare artists, descending
				COMPARE(artist, DESCENDING);

		case 'A':	// Compare artists, ascending
				COMPARE(artist, ASCENDING);

		case 'l':	// Compare albums, descending
				COMPARE(album, DESCENDING);

		case 'L':	// Compare albums, ascending
				COMPARE(album, ASCENDING);

		case 'g':	// Compare genres, descending
				COMPARE(genre, DESCENDING);

		case 'G':	// Compare genres, ascending
				COMPARE(genre, ASCENDING);

		case 'f':	// Compare filenames, descending
				COMPARE(filename, DESCENDING);

		case 'F':	// Compare filenames, ascending
				COMPARE(filename, ASCENDING);

		case 'c':	// Compare comments, descending
				COMPARE(comment, DESCENDING);

		case 'C':	// Compare comments, ascending
				COMPARE(comment, ASCENDING);

		case 'y':	// Compare years, descending		
				ai = atoi (a.year.c_str ());
				bi = atoi (b.year.c_str ());

				if (ai == bi)  continue;

				return ai > bi;

		case 'Y':	// Compare years, ascending
				ai = atoi (a.year.c_str ());
				bi = atoi (b.year.c_str ());

				if (ai == bi)  continue;

				return ai < bi;

		case 'n':	// Compare tracks, descending
				ai = atoi (a.track.c_str ());
				bi = atoi (b.track.c_str ());

				if (ai == bi)  continue;

				return ai > bi;

		case 'N':	// Compare tracks, ascending
				ai = atoi (a.track.c_str ());
				bi = atoi (b.track.c_str ());

				if (ai == bi)  continue;

				return ai < bi;

		case 'p':	// Compare playtimes, descending
				if (a.playtime == b.playtime)  continue;

				return a.playtime > b.playtime;

		case 'P':	// Compare playtimes, ascending
				if (a.playtime == b.playtime)  continue;

				return a.playtime < b.playtime;
	}
    }

    // Don't sort
    return 0;
} /* end of: sort_comparator */

// Since sort_seq variable is a global variable, it should be locked when it is in use
// This variable should be initialized when the program started.
pthread_mutex_t playlist_sort_seq_mutex;

void info_looper(Playlist *playlist)
{
	CorePlayer *myplayer;
	stream_info info;
	int t_sec, count;

	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;

	if (!playlist) return;

	myplayer = new CorePlayer(NULL);

	if (!myplayer) return;

	std::vector<PlayItem>::iterator p = playlist->queue.begin();
	count = 0;
	while (playlist->active) {
		//playlist->Lock ();

		if (p >= playlist->queue.end()) {
		    /* Playlist cleared, shrinked or its an end of list */
		    //playlist->Unlock ();
		    break;
		}

		if (!(*p).Parsed()) {
			if (myplayer->Load((*p).filename.c_str())) { // Examine file
				t_sec = myplayer->GetCurrentTime(myplayer->GetFrames());
				if (t_sec) {
					t_sec /= 100;
					(*p).playtime = t_sec;
				} else {
					(*p).playtime = -1;
				}		
				if (myplayer->GetStreamInfo(&info)) {
					(*p).title = info.title;
					(*p).artist = info.artist;
					(*p).album = info.album;
					(*p).genre = info.genre;
					(*p).year = info.year;
					(*p).track = info.track;
					(*p).comment = info.comment;
				}
				myplayer->Unload();	
			}
			(*p).SetParsed();
			// Notify interface of update
			playlist->LockInterfaces();
			if (playlist->interfaces.size() > 0) {
				for(i = playlist->interfaces.begin();
						i != playlist->interfaces.end(); i++) {
					(*i)->CbUpdated((*p), count);
				}
			}
			if (playlist->cinterfaces.size() > 0) {
				for (j = playlist->cinterfaces.begin();
						j != playlist->cinterfaces.end(); j++) {
					(*j)->cbupdated((*j)->data, (*p), count);
				}
			}
			playlist->UnlockInterfaces();
		}
		p++;
		count++;
		//playlist->Unlock ();
	}	

	delete myplayer;
	//alsaplayer_error("exit info_looper()");
}


void playlist_looper(void *data)
{
#ifdef DEBUG
	printf("THREAD-%d=playlist thread\n", getpid());
#endif /* DEBUG */
	Playlist *pl = (Playlist *)data;
	CorePlayer *coreplayer;
	if(!pl) return;
	
	while(pl->active) {
		if (!pl->Paused()) {
			if (!(coreplayer = (CorePlayer *)(pl->coreplayer)))
				return;
			
			if (!coreplayer->IsActive()) {
				if (pl->Length()) {
					pl->Next();
				}	
			}
			
			if (pl->Length() && pl->Crossfading() && pl->coreplayer->GetSpeed() >= 0.0) {
				// Cross example
				if ((coreplayer->GetFrames() - coreplayer->GetPosition()) < 300) {
						if (pl->player1->IsActive() && pl->player2->IsActive()) {
							alsaplayer_error("Stopping players in playlist_looper");
							pl->player1->Stop();
							pl->player2->Stop();
						}	
						if (pl->player1->IsActive()) {
								pl->player2->SetSpeed(pl->coreplayer->GetSpeed());
								pl->coreplayer = pl->player2;
						} else {
								pl->player1->SetSpeed(pl->coreplayer->GetSpeed());
								pl->coreplayer = pl->player1;
						}		
						pl->Next();
				}
			}
		}
		pl->coreplayer->PositionUpdate(); // Update the position
		dosleep(200000);
	}
}

class PlInsertItems {
	public:
		Playlist *playlist;
		std::vector<std::string> items;
		unsigned position;

		PlInsertItems(Playlist *pl) {
			playlist = pl;
		}
};

// Thread which performs an insert to playlist
void insert_looper(void *data) {
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;

	PlInsertItems * items = (PlInsertItems *)data;
	Playlist *playlist = items->playlist;

	// Stop the list being changed while we add these items
	playlist->Lock();

	// First vetting of the list, and recurse through directories
	std::vector<std::string> vetted_items;
	std::vector<std::string>::const_iterator k = items->items.begin();
	while(k != items->items.end() && playlist->active) {
		additems(&(vetted_items), *k++, MAXRECURSEDEPTH);
	}
	std::vector<PlayItem> newitems;
	if(vetted_items.size() > 0) {
		char cwd[PATH_MAX + 1];
		std::vector<std::string>::const_iterator path;

		if (!getcwd(cwd, PATH_MAX)) {
			alsaplayer_error("Failed to get current working directory");
			cwd[0] = 0;
		}	
		// Check items for adding to list
		for(path = vetted_items.begin(); path != vetted_items.end() && playlist->active; path++) {
			// Check that item is valid
			if(!playlist->CanPlay(*path)) {
				//alsaplayer_error("Can't find a player for `%s'\n", path->c_str());
			} else {
				newitems.push_back(PlayItem(*path));
			}
		}
	}
	// Check position is valid
	if(playlist->queue.size() < items->position) {
		items->position = playlist->queue.size();
	}
	// Add to list
	playlist->queue.insert(playlist->queue.begin() + items->position,
						   newitems.begin(),
						   newitems.end());
	if(playlist->curritem > items->position)
		playlist->curritem += newitems.size();
	
	// Tell the subscribing interfaces about the changes
	playlist->LockInterfaces();
	if(playlist->interfaces.size() > 0) {
		for(i = playlist->interfaces.begin();
			i != playlist->interfaces.end(); i++) {
			(*i)->CbInsert(newitems, items->position);
			(*i)->CbSetCurrent(playlist->curritem);
		}
	}	
	if (playlist->cinterfaces.size() > 0) {	
		for (j = playlist->cinterfaces.begin();
			j != playlist->cinterfaces.end(); j++) {
			(*j)->cbinsert((*j)->data, newitems, items->position);
			(*j)->cbsetcurrent((*j)->data, playlist->curritem);
		}	
	}
	playlist->UnlockInterfaces();
	// Free the list again
	if (playlist->active)
		info_looper(playlist);
	
	playlist->Unlock();
	delete items;
}


void Playlist::LockInterfaces()
{
	pthread_mutex_lock(&interfaces_mutex);
}

void Playlist::UnlockInterfaces()
{
	pthread_mutex_unlock(&interfaces_mutex);
}



// Playlist class

Playlist::Playlist(AlsaNode *the_node) {
	player1 = new CorePlayer(the_node);
	player2 = new CorePlayer(the_node);

	if (!player1 || !player2) {
		printf("Cannot create player objects in Playlist constructor\n");
		return;
	}
	coreplayer = player1;
	curritem = 0;
	active = true;
	paused = false;
	total_time = total_size = 0;

	UnLoopSong();		// Default values
	UnLoopPlaylist();	// for looping
	UnCrossfade();		// and crossfading

	pthread_mutex_init(&playlist_mutex, NULL);
	pthread_mutex_init(&interfaces_mutex, NULL);
	pthread_mutex_init(&playlist_load_mutex, NULL);

	pthread_create(&playlist_thread, NULL, (void * (*)(void *))playlist_looper, this);
}	


Playlist::~Playlist() {
	active = false;
	pthread_join(playlist_thread, NULL);
	interfaces.clear();	// Unregister all interfaces
	
	if (player1)
		delete player1;
	if (player2)
		delete player2;

	Lock();
	pthread_mutex_destroy(&playlist_mutex);
}

// Return number of items in playlist
unsigned Playlist::Length() {
	return queue.size();
}

// Request to move to specified item
void Playlist::Play(unsigned item) {
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;


	Lock();

	if(item < 1) item = 1;
	if(item <= queue.size()) {
		curritem = item;
		PlayFile(queue[curritem - 1]);
	} else {
		curritem = queue.size();
		Stop();
	}
	LockInterfaces();
	// Tell the subscribing interfaces about the change
	if(interfaces.size() > 0) {
		for(i = interfaces.begin(); i != interfaces.end(); i++) {
			(*i)->CbSetCurrent(curritem);
		}
	}
	if (cinterfaces.size() > 0) {
		for(j = cinterfaces.begin(); j != cinterfaces.end(); j++) {
			(*j)->cbsetcurrent((*j)->data, curritem);
		}
	}
	UnlockInterfaces();
	Unlock();
}

// Request to move to next item
void Playlist::Next() {
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;

	Lock();

	unsigned olditem = curritem;
	if(queue.size() > 0) {
	  if(curritem < queue.size()) {
	    if (LoopingSong()){
	      PlayFile(queue[curritem]);
	      /*     } else if (LoopingPlaylist()){
	      curritem++;
	      PlayFile(queue[curritem - 1]); */
	    } else {
	      curritem++;
	      PlayFile(queue[curritem - 1]); 
	    }
	  } else if (curritem == queue.size()){
	    if (LoopingPlaylist()){
	      curritem = 1;
	      PlayFile(queue[curritem -1]); 
	    }
	  }
	}
	//printf("Notifying playlists.....\n");

	// Tell the subscribing interfaces about the change
	if(curritem != olditem) {
		if(interfaces.size() > 0) {
			for(i = interfaces.begin(); i != interfaces.end(); i++) {
				(*i)->CbSetCurrent(curritem);
			}
		}
		if (cinterfaces.size() > 0) {
			for(j = cinterfaces.begin(); j != cinterfaces.end(); j++) {
				(*j)->cbsetcurrent((*j)->data, curritem);
			}
		}	
	}
	Unlock();
}

// Request to move to previous item
void Playlist::Prev() {
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;

	Lock();

	unsigned olditem = curritem;
	if(curritem > queue.size()) {
		curritem = queue.size();
	}
	if(curritem > 1) {
		curritem--;
	}
	if(curritem != 0) {
		PlayFile(queue[curritem - 1]);
	}

	// Tell the subscribing interfaces about the change
	if(curritem != olditem) {
		if(interfaces.size() > 0) {
			for(i = interfaces.begin(); i != interfaces.end(); i++) {
				(*i)->CbSetCurrent(curritem);
			}
		}
		if (cinterfaces.size() > 0) {
			for(j = cinterfaces.begin(); j != cinterfaces.end(); j++) {
				(*j)->cbsetcurrent((*j)->data, curritem);
			}
		}	
	}

	Unlock();
}


void Playlist::Lock()
{
	pthread_mutex_lock(&playlist_mutex);
}


void Playlist::Unlock()
{
	pthread_mutex_unlock(&playlist_mutex);
}


// Request to put a new item at end of playlist
void Playlist::Insert(std::vector<std::string> const & paths, unsigned position, bool wait_for_insert) {
	// Prepare to do insert
	PlInsertItems * items = new PlInsertItems(this);
	items->position = position;

	// Copy list
	std::vector<std::string>::const_iterator i = paths.begin();
	while(i != paths.end()) {
		//alsaplayer_error("===== %s", i->c_str());
		items->items.push_back(*i++);
	}

	insert_looper(items);
}

// Add some items start them playing
void Playlist::AddAndPlay(std::vector<std::string> const &paths) {
	// There is a possible concurrency problem here, if we're trying
	// to insert items into the playlist at the same time as this is
	// called - the other new items could get inserted after the Play()
	// call, but before our items, causing the wrong ones to be played
	// However, this is sufficiently unlikely in practice, and fiddly
	// to fix, (and relatively harmless) that we can ignore it.

	// Move current play point to off end of list (stops play)
	int next_pos = Length() + 1;

	// Now add the new items
	Insert(paths, Length(), true); // Wait for insert to finish

	Play(next_pos);
}


void Playlist::SetCurrent(unsigned pos)
{
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;
		
	Lock();
	curritem = pos;
	// Tell the subscribing interfaces about the change
	if(interfaces.size() > 0) {
		for(i = interfaces.begin(); i != interfaces.end(); i++) {
			(*i)->CbSetCurrent(curritem);
		}
	}
	if(cinterfaces.size() > 0) {
		for(j = cinterfaces.begin(); j != cinterfaces.end(); j++) {
			(*j)->cbsetcurrent((*j)->data, curritem);
		}
	}
	Unlock();
}

// Remove tracks from position start to end inclusive
void Playlist::Remove(unsigned start, unsigned end) {
	bool restart = 0;
    
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;

	if(start > end) {
		unsigned tmp = end;
		end = start;
		start = tmp;
	}
	
	Lock();
				
	if(start < 1) start = 1;
	if(start > queue.size()) start = queue.size();
	if(end < 1) end = 1;
	if(end > queue.size()) end = queue.size();

	queue.erase(queue.begin() + start - 1, queue.begin() + end);

	if (curritem >= start) {
		if(curritem > end) {
			curritem -= (end + 1 - start);
		} else {
			curritem = start;
			restart = 1;
		}
	} else if (queue.size() == 0) {
		curritem = 0;
		restart = 1;
	}	

	// Tell the subscribing interfaces about the change
	if (interfaces.size() > 0) {
		for(i = interfaces.begin(); i != interfaces.end(); i++) {
			(*i)->CbRemove(start, end);
			if (!restart)  (*i)->CbSetCurrent(curritem);
		}
	}
	if (cinterfaces.size() > 0) {
		for(j = cinterfaces.begin(); j != cinterfaces.end(); j++) {
			(*j)->cbremove((*j)->data, start, end);
			if (!restart)  (*j)->cbsetcurrent((*j)->data, curritem);
		}
	}
	
	Unlock();

	if (restart && curritem == 0) {
	    Stop ();
	} else if (restart) {
	    Play (curritem);
	}
}


// Randomize playlist
void Playlist::Shuffle() {
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;
	std::vector<PlayItem>::iterator p;
	
	if (!queue.size ())  return;
	
	Lock();

	// Mark curritem
	(*(queue.begin() + curritem - 1)).marked_to_keep_curritem = 1;
	
	// Shuffle
	random_shuffle(queue.begin(), queue.end());

	// Search new location of the playing song
	for (p = queue.begin (), curritem = 1; p != queue.end (); p++, curritem++)
	    if ((*p).marked_to_keep_curritem == 1)
		break;
	
	(*(queue.begin() + curritem - 1)).marked_to_keep_curritem = 0;
	
	// Tell the subscribing interfaces about the change
	if(interfaces.size() > 0) {
		// Clear and repopulate
		for(i = interfaces.begin(); i != interfaces.end(); i++) {
			(*i)->CbClear();
			(*i)->CbInsert(queue, 0);
			(*i)->CbSetCurrent(curritem);
		}
	}
	if (cinterfaces.size() > 0) {
		for(j = cinterfaces.begin(); j != cinterfaces.end(); j++) {
			(*j)->cbclear((*j)->data);
			(*j)->cbinsert((*j)->data, queue, 0);
			(*j)->cbsetcurrent((*j)->data, curritem);
		}

	}

	Unlock();
}

// Empty playlist
void Playlist::Clear() {
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;

	Lock();

	queue.clear();
	curritem = 0;

	// Tell the subscribing interfaces about the change
	if(interfaces.size() > 0) {
		for(i = interfaces.begin(); i != interfaces.end(); i++) {
			(*i)->CbClear();
		}
	}
	if(cinterfaces.size() > 0) {
		for(j = cinterfaces.begin(); j != cinterfaces.end(); j++) {
			(*j)->cbclear((*j)->data);
		}
	}
	Unlock();

}

enum plist_result
Playlist::Save(std::string file, enum plist_format format) const
{
	switch(format) {
		case PL_FORMAT_M3U:
			if(file.length() < 4 ||
			   strcasecmp(file.substr(file.length() - 4).c_str(), ".m3u")) {
				file += ".m3u";
			}
			//cout << "Saving to " << file << endl;

			std::ofstream out_list(file.c_str());
			if(!out_list) return E_PL_BAD;

			//out_list << MAGIC_ID << endl;
			std::vector<PlayItem>::const_iterator p = queue.begin();
			while(p != queue.end()) {
				out_list << (*p).filename << std::endl;
				p++;
			}
	}
	return E_PL_SUCCESS;
}

// Returns:
// E_PL_SUCCESS on success,
// E_PL_DUBIOUS if file doesn't appear to be in a known format,
// E_PL_BAD if file definitely isn't in a known format.
// If "force" is true, will try to load anyway instead of returning E_PL_DUBIOUS
enum plist_result
Playlist::Load(std::string const &file, unsigned position, bool force)
{
	// Check extension
	if(!force) {
		if(file.length() < 4 ||
		   strcasecmp(file.substr(file.length() - 4).c_str(), ".m3u")) {
			return E_PL_DUBIOUS;
		}
	}

	// lstat file - allow regular files, pipes, unix sockets.
	// Don't allow anything else
	// FIXME - implement

	// Try opening file
#ifdef DEBUG
	alsaplayer_error("Loading from: %s", file.c_str());
#endif
	FILE *in_list = fopen(file.c_str(), "r");
	if(!in_list) return E_PL_BAD;
	
	// Directory of m3u file, might need it later
	std::string dir = file;
	std::string::size_type i = dir.rfind('/');
	if(i != std::string::npos) dir.erase(i);
	dir += '/';
	
	// Read the file
	char path[READBUFSIZE + 1];
	std::vector<std::string> newfiles;
	

	// Give up if too many failures (so we don't wait for almost ever if
	// someone tries clicking on an mp3 file...)
	// However, if its just that some of the files don't exist anymore,
	// don't give up.
	unsigned successes = 0;
	unsigned failures = 0;
	while(failures < MAXLOADFAILURES || failures < successes) {

		if(fscanf(in_list,
				  "%" STRINGISE(READBUFSIZE) "[^\n]\n",
				  path) != 1) break;

		if (strchr(path, '\\')) { // DOS style paths, convert
			for (int c=0; path[c] != '\0'; c++) {
				if (path[c] == '\\') path[c] = '/';
			}
			// And make sure there is no '\r' at the end
			char *p;
			if ((p = strrchr(path, '\r')) != NULL)
				*p = '\0';
		}

		std::string newfile;
		if (path[0] == '#') { // Comment, so skip
			continue;
		}	
		if (path[0] == '/') {
			// Absolute path
			newfile = std::string(path);
		} else if(path[0] == '\0') {
			// No path
			failures++;
			continue;
		} else {
			// Relative path
			newfile = dir + std::string(path);
		}

		// See if the file exists, and isn't a directory
		struct stat buf;
		if(lstat(newfile.c_str(), &buf)) {
			failures++;
			continue;
		}
		if (S_ISDIR(buf.st_mode)) {
			failures++;
			continue;
		}

		// Don't allow directories
		newfiles.push_back(newfile);
		successes++;
	}

	// Check if we read whole file OK
	if(ferror(in_list) || !feof(in_list)) {
		fclose(in_list);
		return E_PL_BAD;
	}
	fclose(in_list);

	// Do the insert
	Insert(newfiles, position);
	return E_PL_SUCCESS;
}


void Playlist::RegisterNotifier(coreplayer_notifier *notif, void *data)
{
	player1->RegisterNotifier(notif, data);
	player2->RegisterNotifier(notif, data);
}


void Playlist::UnRegisterNotifier(coreplayer_notifier *notif)
{
	player1->UnRegisterNotifier(notif);
	player2->UnRegisterNotifier(notif);
}


void Playlist::Register(playlist_interface *pl_if)
{
	LockInterfaces();
	cinterfaces.insert(pl_if);
	UnlockInterfaces();
	if (queue.size()) {
		LockInterfaces();
		pl_if->cbinsert(pl_if->data, queue, 0);
		UnlockInterfaces();
	}
	LockInterfaces();
	pl_if->cbsetcurrent(pl_if->data, curritem);
	UnlockInterfaces();
}

void Playlist::Register(PlaylistInterface * pl_if) {
	LockInterfaces();
	interfaces.insert(pl_if);
	// Tell the interfaces about the current state
	pl_if->CbClear();
	if(queue.size()) {
		pl_if->CbInsert(queue, 0);
	}
	pl_if->CbSetCurrent(curritem);
	UnlockInterfaces();
}


void Playlist::UnRegister(playlist_interface *pl_if) {
	if (!pl_if)
		return;
	LockInterfaces();
	cinterfaces.erase(cinterfaces.find(pl_if));
	UnlockInterfaces();
}


void Playlist::UnRegister(PlaylistInterface * pl_if) {
	if (!pl_if)
		return;
	LockInterfaces();
	interfaces.erase(interfaces.find(pl_if));
	UnlockInterfaces();
}

void Playlist::Stop() {
	Pause();
	player1->Stop(); 
	player2->Stop();
}

bool Playlist::PlayFile(PlayItem const & item) {
	bool result;

	coreplayer->Stop();
	result = coreplayer->Load(item.filename.c_str());
	if (result) {
		result = coreplayer->Start();
		if (coreplayer->GetSpeed() == 0.0) { // Unpause
			coreplayer->SetSpeed(1.0);
		}	
	}	
	return result;
}

// Check if we are able to play a given file
bool Playlist::CanPlay(std::string const & path) {
	bool result = (coreplayer->GetPlayer(path.c_str()) != NULL);
	//alsaplayer_error("CanPlay result = %d", result);
	return result;
}

// Sort playlist
void Playlist::Sort (std::string const &seq) {
	std::set<PlaylistInterface *>::const_iterator i;
	std::set<playlist_interface *>::const_iterator j;
	std::vector<PlayItem>::iterator p;

	if (!queue.size ())  return;
	
	Lock();

	// We will use global sort_seq variable, so lock it
	pthread_mutex_lock(&playlist_sort_seq_mutex);
	
	// Let the sort_comparator function know seq value
	sort_seq = seq.c_str ();

	// Mark curritem
	(*(queue.begin() + curritem - 1)).marked_to_keep_curritem = 1;

	// Sort
	sort (queue.begin(), queue.end(), sort_comparator);

	// Let other playlists use sort_seq variable
	pthread_mutex_unlock(&playlist_sort_seq_mutex);

	// Search new location of the playing song
	for (p = queue.begin (), curritem = 1; p != queue.end (); p++, curritem++)
	    if ((*p).marked_to_keep_curritem == 1)
		break;
	
	(*(queue.begin() + curritem - 1)).marked_to_keep_curritem = 0;
	
	// Tell the subscribing interfaces about the change
	if (interfaces.size() > 0) {
		// Clear and repopulate
		for(i = interfaces.begin(); i != interfaces.end(); i++) {
			(*i)->CbClear();
			(*i)->CbInsert(queue, 0);
			(*i)->CbSetCurrent(curritem);
		}
	}
	if (cinterfaces.size() > 0) {
		for(j = cinterfaces.begin(); j != cinterfaces.end(); j++) {
			(*j)->cbclear((*j)->data);
			(*j)->cbinsert((*j)->data, queue, 0);
			(*j)->cbsetcurrent((*j)->data, curritem);
		}
	}

	Unlock();
}	


// Add a path to list, recursing through (to a maximum of depth subdirectories)
static void additems(std::vector<std::string> *items, std::string path, int depth) {
	if(depth < 0) return;

	// Try expand this URI
	char **expanded = reader_expand (path.c_str ());
    
	if (expanded) {
		char **c_uri = expanded;
		
		while (*c_uri)
		    additems (items, *(c_uri++), depth-1);
		    
		reader_free_expanded (expanded);
	} else {
		items->push_back(path);
	}
}
