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
    PROP_LOOPING_SONG
};

/* --- prototypes --- */
static void	playlist_class_init	    (PlaylistClass	*class);
static void	playlist_init		    (Playlist		*playlist);
static void	playlist_set_property	    (GObject		*object,
					     guint		prop_id,
					     const GValue	*value,
					     GParamSpec		*pspec);
static void	playlist_get_property	    (GObject		*object,
					     guint		prop_id,
					     GValue		*value,
					     GParamSpec		*pspec);
static void	playlist_finalize	    (GObject		*object);

/* --- variables --- */
static gpointer		parent_class = NULL;

/* --- functions --- */
GType
playlist_get_type (void)
{
    static GType type = 0;
    static const GTypeInfo playlist_info = {
	sizeof (PlaylistClass),
	NULL,
        NULL,
        (GClassInitFunc) playlist_class_init,
        NULL,
        NULL,
        sizeof (Playlist),
        0,
        (GInstanceInitFunc) playlist_init,
        NULL
    };

    if (!type) {
	/* First time create */
	type = g_type_register_static (G_TYPE_OBJECT,	/* Parent Type */
				"Playlist",		/* Name */
				&playlist_info,		/* Type Info */
				0);			/* Flags */
    }

    return type;
}; /* playlist_object_get_type */

static void
playlist_class_init (PlaylistClass *class)
{
    /* Like aliases */
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    
    /* Peek parent class */
    parent_class = g_type_class_peek_parent (class);
    
    /* Init GObject Class */
    gobject_class->set_property = playlist_set_property;
    gobject_class->get_property = playlist_get_property;
    gobject_class->finalize = playlist_finalize;

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

    /* Register signals */
    g_signal_new ("pause-toggled",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (PlaylistClass, pause_toggled_signal),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1, G_TYPE_BOOLEAN);

    g_signal_new ("looping-song-toggled",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (PlaylistClass, looping_song_toggled_signal),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1, G_TYPE_BOOLEAN);
} /* playlist_class_init */

static void
playlist_init (Playlist	*playlist)
{
    playlist->paused = FALSE;
    playlist->looping_song = FALSE;
} /* playlist_init */

static void
playlist_finalize (GObject *object)
{
    Playlist *playlist = PLAYLIST (object); 
 
    G_OBJECT_CLASS (parent_class)->finalize (object);
} /* playlist_finalize */

static void
playlist_set_property (GObject		*object,
		       guint		prop_id,
		       const GValue	*value,
		       GParamSpec	*pspec)
{
    Playlist *playlist = PLAYLIST (object);

    switch (prop_id) {
	case PROP_PAUSE:
	    if (g_value_get_boolean (value))
		playlist_pause (playlist);
	    else
		playlist_unpause (playlist);
	    break;
	case PROP_LOOPING_SONG:
	    if (g_value_get_boolean (value))
		playlist_loop_song (playlist);
	    else
		playlist_unloop_song (playlist);
	    break;
    }
} /* playlist_set_property */

static void
playlist_get_property (GObject		*object,
		       guint		prop_id,
		       GValue		*value,
		       GParamSpec	*pspec)
{
    Playlist *playlist = PLAYLIST (object);

    switch (prop_id) {
	case PROP_PAUSE:
	    g_value_set_boolean (value, playlist->paused);
	    break;
	case PROP_LOOPING_SONG:
	    g_value_set_boolean (value, playlist->looping_song);
	    break;
    }
} /* playlist_set_property */

void
playlist_pause (Playlist *playlist)
{
    g_return_if_fail (IS_PLAYLIST (playlist));

    if (!playlist->paused) {
	playlist->paused = TRUE;
    
	/* Signal */
	g_signal_emit_by_name (playlist, "pause-toggled", TRUE);
    }
} /* playitem_set_playtime */

void
playlist_unpause (Playlist *playlist)
{
    g_return_if_fail (IS_PLAYLIST (playlist));

    if (playlist->paused) {
	playlist->paused = FALSE;

	/* Signal */
	g_signal_emit_by_name (playlist, "pause-toggled", FALSE);
    }
} /* playitem_set_playtime */

gboolean
playlist_is_paused (Playlist *playlist)
{
    g_return_val_if_fail (IS_PLAYLIST (playlist), FALSE);

    return playlist->paused;
} /* playitem_set_playtime */

void
playlist_loop_song (Playlist *playlist)
{
    g_return_if_fail (IS_PLAYLIST (playlist));

    if (!playlist->looping_song) {
	playlist->looping_song = TRUE;

	/* Signal */
	g_signal_emit_by_name (playlist, "looping-song-toggled", TRUE);
    }
} /* playitem_set_playtime */

void
playlist_unloop_song (Playlist *playlist)
{
    g_return_if_fail (IS_PLAYLIST (playlist));

    if (playlist->looping_song) {
	playlist->looping_song = FALSE;

	/* Signal */
	g_signal_emit_by_name (playlist, "looping-song-toggled", FALSE);
    }
} /* playitem_set_playtime */

gboolean
playlist_is_looping_song (Playlist *playlist)
{
    g_return_val_if_fail (IS_PLAYLIST (playlist), FALSE);

    return playlist->looping_song;
} /* playitem_set_playtime */
