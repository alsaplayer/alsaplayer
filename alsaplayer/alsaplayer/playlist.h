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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* TODO: Document it for gtkdoc */
   
/* Macro for Playlist type. */
#define TYPE_PLAYLIST			(playlist_get_type ())
#define PLAYLIST(playlist)		(G_TYPE_CHECK_INSTANCE_CAST ((object), TYPE_PLAYLIST, Playlist))
#define IS_PLAYLIST(playlist)		(G_TYPE_CHECK_INSTANCE_TYPE ((playlist), TYPE_PLAYLIST))
    
/* forward declaration to avoid excessive includes (and concurrent includes) */
typedef struct _Playlist   Playlist;
typedef struct _PlaylistClass   PlaylistClass;

struct _Playlist {
    /** Parent object structure.
     * 
     *  The gobject structure needs to be the first
     *  element in the playlist structure in order for
     *  the object mechanism to work correctly. This
     *  allows a Playlist pointer to be cast to a
     *  GObject pointer.
     */
    GObject	    gobject;
    
    gboolean	    paused;
    gboolean	    looping_song;
};

struct _PlaylistClass {
    /** Parent class structure.
     * 
     *  The gobject class structure needs to be the first
     *  element in the playlist class structure in order for
     *  the class mechanism to work correctly. This allows a
     *  PlaylistClass pointer to be cast to a GObjectClass
     *  pointer.
     */
    GObjectClass    gobject_class;

    /* signals */
    void*   (*pause_toggled_signal)		(Playlist	*playlist,
						 gboolean	pause,
						 gpointer	data);
    
    void*   (*looping_song_toggled_signal)	(Playlist	*playlist,
						 gboolean	looping_song,
						 gpointer	data);
};

GType			    playlist_get_type		(void) G_GNUC_CONST;

void			    playlist_pause		(Playlist	*playlist);
void			    playlist_unpause		(Playlist	*playlist);
gboolean		    playlist_is_paused		(Playlist	*playlist);

void			    playlist_loop_song		(Playlist	*playlist);
void			    playlist_unloop_song	(Playlist	*playlist);
gboolean		    playlist_is_looping_song	(Playlist	*playlist);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PLAYLIST_H__ */
