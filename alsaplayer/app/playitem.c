/*  playitem.c
 *  Copyright (C) 2002 Evgeny Chukreev <codedj@echo.ru>
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

/* TODO: Document it for gtkdoc */
/* TODO: Make i18n oneday. */
#define _(s) (s)

/* PlayItem class properties. */
enum {
    PROP_0,
    PROP_FILENAME,
    PROP_TITLE,
    PROP_ARTIST,
    PROP_ALBUM,
    PROP_GENRE,
    PROP_COMMENT,
    PROP_YEAR,
    PROP_TRACK,
    PROP_PLAYTIME
};

/* --- prototypes --- */
static void	ap_playitem_class_init	    (ApPlayItemClass	*class);
static void	ap_playitem_init	    (ApPlayItem		*playitem);
static void	ap_playitem_set_property    (GObject		*object,
					     guint		prop_id,
					     const GValue	*value,
					     GParamSpec		*pspec);
static void	ap_playitem_get_property    (GObject		*object,
					     guint		prop_id,
					     GValue		*value,
					     GParamSpec		*pspec);
static void	ap_playitem_finalize	    (GObject		*object);

/* --- variables --- */
static gpointer		parent_class = NULL;

/* --- functions --- */
GType
ap_playitem_get_type (void)
{
    static GType type = 0;
    static const GTypeInfo playitem_info = {
	sizeof (ApPlayItemClass),
	NULL,
        NULL,
        (GClassInitFunc) ap_playitem_class_init,
        NULL,
        NULL,
        sizeof (ApPlayItem),
        0,
        (GInstanceInitFunc) ap_playitem_init,
        NULL
    };

    if (!type) {
	/* First time create */
	type = g_type_register_static (G_TYPE_OBJECT,	/* Parent Type */
				"ApPlayItem",		/* Name */
				&playitem_info,		/* Type Info */
				0);			/* Flags */
    }

    return type;
}; /* ap_playitem_object_get_type */

static void
ap_playitem_class_init (ApPlayItemClass *class)
{
    /* Like aliases */
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    
    /* Peek parent class */
    parent_class = g_type_class_peek_parent (class);
    
    /* Init GObject Class */
    gobject_class->set_property = ap_playitem_set_property;
    gobject_class->get_property = ap_playitem_get_property;
    gobject_class->finalize = ap_playitem_finalize;

    /* Install properties */
    g_object_class_install_property (gobject_class,
				     PROP_FILENAME,
				     g_param_spec_string ("filename",
							  _("File name"),
							  _("File name of this song."),
							  NULL,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_TITLE,
				     g_param_spec_string ("title",
							  _("Title"),
							  _("Title of this song."),
							  NULL,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_ARTIST,
				     g_param_spec_string ("artist",
							  _("Artist"),
							  _("Artist of this song."),
							  NULL,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_ALBUM,
				     g_param_spec_string ("album",
							  _("Album"),
							  _("Album of this song."),
							  NULL,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_GENRE,
				     g_param_spec_string ("genre",
							  _("Genre"),
							  _("Genre of this song."),
							  NULL,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_COMMENT,
				     g_param_spec_string ("comment",
							  _("Comment"),
							  _("Comment of this song."),
							  NULL,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_YEAR,
				     g_param_spec_uint ("year",
							  _("Year"),
							  _("Year of this song."),
							  0,	    /* minimum */
							  10000,    /* maximum */
							  0,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_TRACK,
				     g_param_spec_uint ("track",
							  _("Track number"),
							  _("Track number of this song."),
							  0,	    /* minimum */
							  10000,    /* maximum */
							  0,
							  G_PARAM_READWRITE)
				     );

    g_object_class_install_property (gobject_class,
				     PROP_PLAYTIME,
				     g_param_spec_uint ("playtime",
							  _("Playtime"),
							  _("Playtime of this song."),
							  0,	    /* minimum */
							  0,	    /* maximum */
							  0,
							  G_PARAM_READWRITE)
				     );


} /* ap_playitem_class_init */

static void
ap_playitem_init (ApPlayItem *playitem)
{
    playitem->filename = NULL;
    playitem->title = NULL;
    playitem->artist = NULL;
    playitem->album = NULL;
    playitem->genre = NULL;
    playitem->comment = NULL;
    playitem->year = 0;
    playitem->track = 0;
    playitem->playtime = 0;
} /* playitem_init */

static void
ap_playitem_finalize (GObject *object)
{
    ApPlayItem *playitem = AP_PLAYITEM (object); 
    
    if (playitem->filename)
	g_free (playitem->filename);
    
    if (playitem->title)
	g_free (playitem->title);

    if (playitem->artist)
	g_free (playitem->artist);

    if (playitem->album)
	g_free (playitem->album);

    if (playitem->genre)
	g_free (playitem->genre);

    if (playitem->comment)
	g_free (playitem->comment);
    
    G_OBJECT_CLASS (parent_class)->finalize (object);
} /* ap_playitem_finalize */

static void
ap_playitem_set_property (GObject	*object,
		       guint		prop_id,
		       const GValue	*value,
		       GParamSpec	*pspec)
{
    ApPlayItem *playitem = AP_PLAYITEM (object);

    switch (prop_id) {
	case PROP_FILENAME:
	    ap_playitem_set_filename (playitem, g_value_get_string (value));
	    break;
	case PROP_TITLE:
	    ap_playitem_set_title (playitem, g_value_get_string (value));
	    break;
	case PROP_ARTIST:
	    ap_playitem_set_artist (playitem, g_value_get_string (value));
	    break;
	case PROP_ALBUM:
	    ap_playitem_set_album (playitem, g_value_get_string (value));
	    break;
	case PROP_GENRE:
	    ap_playitem_set_genre (playitem, g_value_get_string (value));
	    break;
	case PROP_COMMENT:
	    ap_playitem_set_comment (playitem, g_value_get_string (value));
	    break;
	case PROP_YEAR:
	    ap_playitem_set_year (playitem, g_value_get_uint (value));
	    break;
	case PROP_TRACK:
	    ap_playitem_set_track (playitem, g_value_get_uint (value));
	    break;
	case PROP_PLAYTIME:
	    ap_playitem_set_playtime (playitem, g_value_get_uint (value));
	    break;
    }
} /* ap_playitem_set_property */

static void
ap_playitem_get_property (GObject	*object,
		          guint		prop_id,
		          GValue	*value,
		          GParamSpec	*pspec)
{
    ApPlayItem *playitem = AP_PLAYITEM (object);

    switch (prop_id) {
	case PROP_FILENAME:
	    g_value_set_string (value, playitem->filename);
	    break;
	case PROP_TITLE:
	    g_value_set_string (value, playitem->title);
	    break;
	case PROP_ARTIST:
	    g_value_set_string (value, playitem->artist);
	    break;
	case PROP_ALBUM:
	    g_value_set_string (value, playitem->album);
	    break;
	case PROP_GENRE:
	    g_value_set_string (value, playitem->genre);
	    break;
	case PROP_COMMENT:
	    g_value_set_string (value, playitem->comment);
	    break;
	case PROP_YEAR:
	    g_value_set_uint (value, playitem->year);
	    break;
	case PROP_TRACK:
	    g_value_set_uint (value, playitem->track);
	    break;
	case PROP_PLAYTIME:
	    g_value_set_uint (value, playitem->playtime);
	    break;
    }
} /* ap_playitem_set_property */

ApPlayItem*
ap_playitem_new (const gchar *filename)
{
    ApPlayItem *playitem = g_object_new (AP_TYPE_PLAYITEM,
				         "filename", filename,
				          NULL);

    if (!playitem)
	return NULL;

    ap_playitem_set_filename (playitem, filename);

    return playitem;
} /* ap_playitem_new */

void
ap_playitem_set_filename (ApPlayItem *playitem, const gchar *filename)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    if (playitem->filename)
	g_free (playitem->filename);

    playitem->filename = g_strdup (filename);
} /* ap_playitem_set_filename */

G_CONST_RETURN gchar *
ap_playitem_get_filename (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), NULL);

    return playitem->filename;
} /* playitem_get_filename */

void
ap_playitem_set_title (ApPlayItem *playitem, const gchar *title)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    if (playitem->title)
	g_free (playitem->title);

    playitem->title = g_strdup (title);
} /* ap_playitem_set_title */

G_CONST_RETURN gchar *
ap_playitem_get_title (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), NULL);

    return playitem->title;
} /* playitem_get_title */

void
ap_playitem_set_artist (ApPlayItem *playitem, const gchar *artist)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    if (playitem->artist)
	g_free (playitem->artist);

    playitem->artist = g_strdup (artist);
} /* ap_playitem_set_artist */

G_CONST_RETURN gchar *
ap_playitem_get_artist (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), NULL);

    return playitem->artist;
} /* ap_playitem_get_artist */

void
ap_playitem_set_album (ApPlayItem *playitem, const gchar *album)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    if (playitem->album)
	g_free (playitem->album);

    playitem->album = g_strdup (album);
} /* ap_playitem_set_album */

G_CONST_RETURN gchar *
ap_playitem_get_album (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), NULL);

    return playitem->album;
} /* ap_playitem_get_album */

void
ap_playitem_set_genre (ApPlayItem *playitem, const gchar *genre)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    if (playitem->genre)
	g_free (playitem->genre);

    playitem->genre = g_strdup (genre);
} /* ap_playitem_set_genre */

G_CONST_RETURN gchar *
ap_playitem_get_genre (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), NULL);

    return playitem->genre;
} /* ap_playitem_get_genre */

void
ap_playitem_set_comment (ApPlayItem *playitem, const gchar *comment)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    if (playitem->comment)
	g_free (playitem->comment);

    playitem->comment = g_strdup (comment);
} /* ap_playitem_set_comment */

G_CONST_RETURN gchar *
ap_playitem_get_comment (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), NULL);

    return playitem->comment;
} /* ap_playitem_get_comment */

void
ap_playitem_set_year (ApPlayItem *playitem, guint year)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    playitem->year = year;
} /* ap_playitem_set_year */

guint
ap_playitem_get_year (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), 0);

    return playitem->year;
} /* ap_playitem_get_year */

void
ap_playitem_set_track (ApPlayItem *playitem, guint track)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    playitem->track = track;
} /* ap_playitem_set_track */

guint
ap_playitem_get_track (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), 0);

    return playitem->track;
} /* ap_playitem_get_track */

void
ap_playitem_set_playtime (ApPlayItem *playitem, guint playtime)
{
    g_return_if_fail (AP_IS_PLAYITEM (playitem));

    playitem->playtime = playtime;
} /* ap_playitem_set_playtime */

guint
ap_playitem_get_playtime (ApPlayItem *playitem)
{
    g_return_val_if_fail (AP_IS_PLAYITEM (playitem), 0);

    return playitem->playtime;
} /* ap_playitem_get_playtime */
