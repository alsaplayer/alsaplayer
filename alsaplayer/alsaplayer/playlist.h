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

/* TODO: Document it for gtkdoc */
   
/* Macro for Playlist type. */
#define AP_TYPE_PLAYLIST		(ap_playlist_get_type ())
#define AP_PLAYLIST(playlist)		(G_TYPE_CHECK_INSTANCE_CAST ((playlist), AP_TYPE_PLAYLIST, ApPlaylist))
#define AP_IS_PLAYLIST(playlist)	(G_TYPE_CHECK_INSTANCE_TYPE ((playlist), AP_TYPE_PLAYLIST))
    
/* forward declaration to avoid excessive includes (and concurrent includes) */
typedef struct _ApPlaylist		ApPlaylist;
typedef struct _ApPlaylistClass		ApPlaylistClass;

struct _ApPlaylist {
    /** Parent object structure.
     * 
     *  The gobject structure needs to be the first
     *  element in the playlist structure in order for
     *  the object mechanism to work correctly. This
     *  allows a ApPlaylist pointer to be cast to a
     *  GObject pointer.
     */
    GObject	    gobject;
    
    gboolean	    paused;
    gboolean	    looping_song;
    gboolean	    looping_playlist;

    GAsyncQueue*    info_queue;
    GThread*	    info_thread;
    gboolean	    info_thread_active;
};

struct _ApPlaylistClass {
    /** Parent class structure.
     * 
     *  The gobject class structure needs to be the first
     *  element in the playlist class structure in order for
     *  the class mechanism to work correctly. This allows a
     *  ApPlaylistClass pointer to be cast to a GObjectClass
     *  pointer.
     */
    GObjectClass    gobject_class;

    /* signals */
    void*   (*pause_toggled_signal)		(ApPlaylist	*playlist,
						 gboolean	pause,
						 gpointer	data);
    
    void*   (*looping_song_toggled_signal)	(ApPlaylist	*playlist,
						 gboolean	looping_song,
						 gpointer	data);

    void*   (*looping_playlist_toggled_signal)	(ApPlaylist	*playlist,
						 gboolean	looping_playlist,
						 gpointer	data);

    void*   (*playitem_updated_signal)		(ApPlaylist	*playlist,
						 ApPlayItem	*playitem,
						 gpointer	data);
};

GType		    ap_playlist_get_type		(void) G_GNUC_CONST;

void		    ap_playlist_pause			(ApPlaylist	*playlist);
void		    ap_playlist_unpause			(ApPlaylist	*playlist);
gboolean	    ap_playlist_is_paused		(ApPlaylist	*playlist);

void		    ap_playlist_loop_song		(ApPlaylist	*playlist);
void		    ap_playlist_unloop_song		(ApPlaylist	*playlist);
gboolean	    ap_playlist_is_looping_song		(ApPlaylist	*playlist);

void		    ap_playlist_loop_playlist		(ApPlaylist	*playlist);
void		    ap_playlist_unloop_playlist		(ApPlaylist	*playlist);
gboolean	    ap_playlist_is_looping_playlist	(ApPlaylist	*playlist);

void		    ap_playlist_update_playitem		(ApPlaylist	*playlist,
							 ApPlayItem	*playitem);
void		    ap_playlist_playitem_updated	(ApPlaylist	*playlist,
							 ApPlayItem	*playitem);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PLAYLIST_H__ */
