/*  playlist.h
 *
 *  Copyright (C) 1999-2002 Andy Lo A Foe <andy@alsaplayer.org>
 *  Rewritten for glibc-2.0 by Evgeny Chukreev <codedj@echo.ru> 
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
 *  $Id$
 *
*/ 

#ifndef __PLAYLIST_H__
#define __PLAYLIST_H__

#include <glib-object.h>
#include <glib.h>

#include "playitem.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @file playlist.h
 *
 * @brief	Declarations for #ApPlaylist and #ApPlaylistClass.
 *
 * @par List of signals defined in ApPlaylistClass:
 *	- @b "updated". Emitted when updating process for a one #ApPlayItem is done.
 *		     Handler for this signal takes three arguments:
 *		     @code void handler (ApPlaylist *playlist, ApPlayItem *plaitem, gpointer data); @endcode
 *		     First argument is a playlist which did item updating.
 *		     Next one is an updated item. This playitem was queued for
 *		     update by ap_playlist_update() function. Last argument is
 *		     a data pointer which was passed for g_signal_connect().
 *	- @b "inserted".
 *	- @b "cleared".
 *	- @b "pause-toggled".
 *	- @b "looping-playlist-toggled".
 *	- @b "looping-song-toggled".
 */

/**
 * @brief	Returns the type ID of the #ApPlaylist type.
 */
#define AP_TYPE_PLAYLIST		(ap_playlist_get_type ())

/**
 * @brief	Cast a #ApPlaylist or derived pointer
 *		into a (ApPlaylist*) pointer.
 * 
 * Depending on the current debugging level, this function may invoke
 * certain runtime checks to identify invalid casts
 */
#define AP_PLAYLIST(playlist)		(G_TYPE_CHECK_INSTANCE_CAST ((playlist), AP_TYPE_PLAYLIST, ApPlaylist))

/**
 * @brief	Check whether a valid #ApPlaylist
 *		pointer is of type #AP_TYPE_PLAYLIST.
 */
#define AP_IS_PLAYLIST(playlist)	(G_TYPE_CHECK_INSTANCE_TYPE ((playlist), AP_TYPE_PLAYLIST))
 
/**
 * @brief	This is opaque structure for a playlist type.
 *
 * All the fields in the #ApPlaylist structure are private to the #ApPlaylist
 * implementation and should never be accessed directly.
 */
typedef struct _ApPlaylist		ApPlaylist;

/**
 * @brief	This is opaque structure for a playlist class.
 *
 * All the fields in the #ApPlaylistClass structure are private
 * to the #ApPlaylistClass implementation and should never be accessed directly.
 */
typedef struct _ApPlaylistClass		ApPlaylistClass;

struct _ApPlaylist {
    /* Parent object structure.
     * 
     * The ap_object structure needs to be the first
     * element in the playlist structure in order for
     * the object mechanism to work correctly. This
     * allows a ApPlaylist pointer to be cast to a
     * ApObject pointer.
     */
    ApObject	    ap_object;
 
    GArray*	    queue;
    
    gboolean	    active;
   
    gboolean	    paused;
    gboolean	    looping_song;
    gboolean	    looping_playlist;

    GAsyncQueue*    info_queue;
    GThread*	    info_thread;

    GAsyncQueue*    insert_queue;
    GThread*	    insert_thread;
};

struct _ApPlaylistClass {
    /* Parent class structure.
     * 
     * The ap_object_class structure needs to be the first
     * element in the playlist class structure in order for
     * the class mechanism to work correctly. This allows a
     * ApPlaylistClass pointer to be cast to a ApObjectClass
     * pointer.
     */
    ApObjectClass    ap_object_class;

    /* defualt handlers for signals */
    void*   (*pause_toggled_signal)		(ApPlaylist	*playlist,
						 gboolean	pause,
						 gpointer	data);
    
    void*   (*looping_song_toggled_signal)	(ApPlaylist	*playlist,
						 gboolean	looping_song,
						 gpointer	data);

    void*   (*looping_playlist_toggled_signal)	(ApPlaylist	*playlist,
						 gboolean	looping_playlist,
						 gpointer	data);

    void*   (*updated_signal)			(ApPlaylist	*playlist,
						 ApPlayItem	*playitem,
						 gpointer	data);

    void*   (*inserted_signal)			(ApPlaylist	*playlist,
						 guint		pos,
						 GPtrArray	*playitem,
						 gpointer	data);

    void*   (*cleared_signal)			(ApPlaylist	*playlist,
						 gpointer	data);
};

GType		    ap_playlist_get_type		(void) G_GNUC_CONST;
void		    ap_playlist_set_pause		(ApPlaylist	*playlist,
							 gboolean	pause);
gboolean	    ap_playlist_is_paused		(ApPlaylist	*playlist);
void		    ap_playlist_set_loop_song		(ApPlaylist	*playlist,
							 gboolean	loop_song);
gboolean	    ap_playlist_is_looping_song		(ApPlaylist	*playlist);
void		    ap_playlist_set_loop_playlist	(ApPlaylist	*playlist,
							 gboolean	loop_playlist);
gboolean	    ap_playlist_is_looping_playlist	(ApPlaylist	*playlist);
void		    ap_playlist_update			(ApPlaylist	*playlist,
							 ApPlayItem	*playitem);
void		    ap_playlist_insert			(ApPlaylist	*playlist,
							 GPtrArray	*array,
							 guint		pos);
void		    ap_playlist_clear			(ApPlaylist    *playlist);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PLAYLIST_H__ */
