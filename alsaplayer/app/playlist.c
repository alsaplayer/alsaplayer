/*  playlist.c
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

#include <glib-object.h>

#include "playitem.h"
#include "playlist.h"

/* TODO: Document it for gtkdoc */
/* TODO: Make i18n oneday. */
#define _(s) (s)

/* Class properties. */
enum {
    PROP_0,
    PROP_PAUSE,
    PROP_LOOPING_SONG,
    PROP_LOOPING_PLAYLIST
};

/* --- prototypes --- */
static void	ap_playlist_class_init	    (ApPlaylistClass	*class);
static void	ap_playlist_init	    (ApPlaylist		*playlist);
static void	ap_playlist_set_property    (GObject		*object,
					     guint		prop_id,
					     const GValue	*value,
					     GParamSpec		*pspec);
static void	ap_playlist_get_property    (GObject		*object,
					     guint		prop_id,
					     GValue		*value,
					     GParamSpec		*pspec);
static void	ap_playlist_finalize	    (GObject		*object);
static void	ap_playlist_dispose	    (GObject		*object);
static gpointer ap_playlist_info_thread	    (gpointer		data);

/* --- variables --- */
static gpointer		parent_class = NULL;

/* --- functions --- */

static void
ap_playlist_class_init (ApPlaylistClass *class)
{
    /* Like aliases */
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    
    /* Peek parent class */
    parent_class = g_type_class_peek_parent (class);
    
    /* Init GObject Class */
    gobject_class->set_property = ap_playlist_set_property;
    gobject_class->get_property = ap_playlist_get_property;
    gobject_class->finalize = ap_playlist_finalize;
    gobject_class->dispose = ap_playlist_dispose;

    /* Install properties */
    g_object_class_install_property (gobject_class,
				     PROP_PAUSE,
				     g_param_spec_boolean ("pause",
							  _("Pause"),
							  _("Playlist pause state."),
							  FALSE,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_LOOPING_SONG,
				     g_param_spec_boolean ("looping_song",
							  _("Looping song"),
							  _("Looping song state."),
							  FALSE,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_LOOPING_PLAYLIST,
				     g_param_spec_boolean ("looping_playlist",
							  _("Looping playlist"),
							  _("Looping playlist state."),
							  FALSE,
							  G_PARAM_READWRITE)
				     );
    
    /* Register signals */
    g_signal_new ("pause-toggled",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ApPlaylistClass, pause_toggled_signal),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1, G_TYPE_BOOLEAN);

    g_signal_new ("looping-song-toggled",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ApPlaylistClass, looping_song_toggled_signal),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1, G_TYPE_BOOLEAN);

    g_signal_new ("looping-playlist-toggled",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ApPlaylistClass, looping_playlist_toggled_signal),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1, G_TYPE_BOOLEAN);
    
    g_signal_new ("playitem-updated",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ApPlaylistClass, playitem_updated_signal),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE,
		  1, G_TYPE_OBJECT);
} /* ap_playlist_class_init */

static void
ap_playlist_init (ApPlaylist *playlist)
{
    playlist->paused = FALSE;
    playlist->looping_song = FALSE;

    /* Create asynchronous queue for delivering playitems into info thread */
    playlist->info_queue = g_async_queue_new ();
    
    /* Start info thread for this playlist. */
    playlist->info_thread_active = TRUE;
    playlist->info_thread = g_thread_create (ap_playlist_info_thread,
					     playlist,
					     1,
					     NULL);
} /* ap_playlist_init */

static void
ap_playlist_dispose (GObject *object)
{
    ApPlaylist *playlist = AP_PLAYLIST (object);
    ApPlayItem *playitem;
 
    /* Join info thread.
     * We create fiction playitem (with NULL filename),
     * which is the signal to exit from info thread.
     * This item will be destroyed inside info thread.
     * Also, we set info_thread_active into zero, so
     * info_thread will skip real updating of items.
     */
    playitem = ap_playitem_new (NULL);
    g_async_queue_push (playlist->info_queue, playitem);
    playlist->info_thread_active = FALSE;
    g_thread_join (playlist->info_thread);
   
    G_OBJECT_CLASS (parent_class)->dispose (object);
} /* ap_playlist_dispose */


static void
ap_playlist_finalize (GObject *object)
{
    ApPlaylist *playlist = AP_PLAYLIST (object); 
 
    /* Unref queue */
    g_async_queue_unref (playlist->info_queue);
    
    G_OBJECT_CLASS (parent_class)->finalize (object);
} /* ap_playlist_finalize */

static void
ap_playlist_set_property (GObject		*object,
			  guint			prop_id,
			  const GValue		*value,
			  GParamSpec		*pspec)
{
    ApPlaylist *playlist = AP_PLAYLIST (object);

    switch (prop_id) {
	case PROP_PAUSE:
	    if (g_value_get_boolean (value))
		ap_playlist_pause (playlist);
	    else
		ap_playlist_unpause (playlist);
	    break;
	case PROP_LOOPING_SONG:
	    if (g_value_get_boolean (value))
		ap_playlist_loop_song (playlist);
	    else
		ap_playlist_unloop_song (playlist);
	    break;
	case PROP_LOOPING_PLAYLIST:
	    if (g_value_get_boolean (value))
		ap_playlist_loop_playlist (playlist);
	    else
		ap_playlist_unloop_playlist (playlist);
	    break;
    }
} /* ap_playlist_set_property */

static void
ap_playlist_get_property (GObject	*object,
			  guint		prop_id,
		          GValue	*value,
		          GParamSpec	*pspec)
{
    ApPlaylist *playlist = AP_PLAYLIST (object);

    switch (prop_id) {
	case PROP_PAUSE:
	    g_value_set_boolean (value, playlist->paused);
	    break;
	case PROP_LOOPING_SONG:
	    g_value_set_boolean (value, playlist->looping_song);
	    break;
	case PROP_LOOPING_PLAYLIST:
	    g_value_set_boolean (value, playlist->looping_playlist);
	    break;
    }
} /* ap_playlist_set_property */

gpointer
ap_playlist_info_thread (gpointer data)
{
    /* We don't need to ref playlist, since this thread exists
     * while playlist life only. */
    ApPlaylist	*playlist = AP_PLAYLIST (data);

    /* Ref queue. Just for fun. */
    g_async_queue_ref (playlist->info_queue);

    /* infty loop */
    while (1) {
	ApPlayItem *playitem = g_async_queue_pop (playlist->info_queue);

	/* We may not lock playitem during this, since
	 * we don't do access by this pointer and
	 * suppose that the NULL filenamed playitem
	 * could not change it filename.
	 */
	if (ap_playitem_get_filename (playitem) == NULL) {
	    /* This is the last item.
	     * It signals us to exit thread.
	     */
	    ap_playitem_unref (playitem);
	    break;
	}

	if (!playlist->info_thread_active) {
	    /* Thread is going to shutdown.
	     * So skip real updating.
	     */
	    ap_playitem_unref (playitem);
	    continue;
	}
	
	/* TODO: Make the real info filling.
	 * But now, we just emulate it ;)
	 */
	g_print ("info thread fills %p\n", playitem);

	ap_playitem_lock (playitem);
	ap_playitem_set_title (playitem, "Updated");
	ap_playitem_unlock (playitem);
	
	/* Emit "playitem-updated" signal */
	g_signal_emit_by_name (playlist, "playitem-updated", playitem);

	/* it was refed in ap_playlist_update_playitem */
	ap_playitem_unref (playitem);
    }

    /* Unref queue before exit. */
    g_async_queue_unref (playlist->info_queue);
    
    return NULL;
}

/* Public functions. */

/**
 * ap_playlist_get_type:
 *
 * Register the #ApPlaylistClass if necessary,
 * and returns the type ID associated to it.
 *
 * Return value: The type ID of the &ApPlaylistClass.
 **/
GType
ap_playlist_get_type (void)
{
    static GType type = 0;
    static const GTypeInfo playlist_info = {
	sizeof (ApPlaylistClass),
	NULL,
        NULL,
        (GClassInitFunc) ap_playlist_class_init,
        NULL,
        NULL,
        sizeof (ApPlaylist),
        0,
        (GInstanceInitFunc) ap_playlist_init,
        NULL
    };

    if (!type) {
	/* First time create */
	type = g_type_register_static (G_TYPE_OBJECT,	/* Parent Type */
				       "ApPlaylist",	/* Name */
				       &playlist_info,	/* Type Info */
				       0);		/* Flags */
    }

    return type;
}; /* ap_playlist_object_get_type */

void
ap_playlist_pause (ApPlaylist *playlist)
{
    g_return_if_fail (AP_IS_PLAYLIST (playlist));

    if (!playlist->paused) {
	playlist->paused = TRUE;
    
	/* Signal */
	g_signal_emit_by_name (playlist, "pause-toggled", TRUE);
    }
} /* ap_playitem_set_playtime */

void
ap_playlist_unpause (ApPlaylist *playlist)
{
    g_return_if_fail (AP_IS_PLAYLIST (playlist));

    if (playlist->paused) {
	playlist->paused = FALSE;

	/* Signal */
	g_signal_emit_by_name (playlist, "pause-toggled", FALSE);
    }
} /* ap_playitem_set_playtime */

gboolean
ap_playlist_is_paused (ApPlaylist *playlist)
{
    g_return_val_if_fail (AP_IS_PLAYLIST (playlist), FALSE);

    return playlist->paused;
} /* ap_playitem_set_playtime */

void
ap_playlist_loop_song (ApPlaylist *playlist)
{
    g_return_if_fail (AP_IS_PLAYLIST (playlist));

    if (!playlist->looping_song) {
	playlist->looping_song = TRUE;

	/* Signal */
	g_signal_emit_by_name (playlist, "looping-song-toggled", TRUE);
    }
} /* ap_playitem_set_playtime */

void
ap_playlist_unloop_song (ApPlaylist *playlist)
{
    g_return_if_fail (AP_IS_PLAYLIST (playlist));

    if (playlist->looping_song) {
	playlist->looping_song = FALSE;

	/* Signal */
	g_signal_emit_by_name (playlist, "looping-song-toggled", FALSE);
    }
} /* ap_playitem_set_playtime */

gboolean
ap_playlist_is_looping_song (ApPlaylist *playlist)
{
    g_return_val_if_fail (AP_IS_PLAYLIST (playlist), FALSE);

    return playlist->looping_song;
} /* ap_playitem_set_playtime */

void
ap_playlist_loop_playlist (ApPlaylist *playlist)
{
    g_return_if_fail (AP_IS_PLAYLIST (playlist));

    if (!playlist->looping_playlist) {
	playlist->looping_playlist = TRUE;

	/* Signal */
	g_signal_emit_by_name (playlist, "looping-playlist-toggled", TRUE);
    }
} /* ap_playitem_set_playtime */

void
ap_playlist_unloop_playlist (ApPlaylist *playlist)
{
    g_return_if_fail (AP_IS_PLAYLIST (playlist));

    if (playlist->looping_playlist) {
	playlist->looping_playlist = FALSE;

	/* Signal */
	g_signal_emit_by_name (playlist, "looping-playlist-toggled", FALSE);
    }
} /* ap_playitem_set_playtime */

gboolean
ap_playlist_is_looping_playlist (ApPlaylist *playlist)
{
    g_return_val_if_fail (AP_IS_PLAYLIST (playlist), FALSE);

    return playlist->looping_playlist;
} /* ap_playitem_set_playtime */

/**
 * ap_playlist_update_playitem:
 * @playlist: a #PyPlaylist
 * @playitem: playitem to update
 *
 * Queue @playitem for update. You can track end of the
 * updating process via the "playitem-updated" signal.
 *
 **/
void
ap_playlist_update_playitem (ApPlaylist	    *playlist,
			     ApPlayItem	    *playitem)
{
    g_return_if_fail (AP_IS_PLAYLIST (playlist));
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    ap_playitem_ref (playitem);
    g_async_queue_push (playlist->info_queue, playitem);
}
