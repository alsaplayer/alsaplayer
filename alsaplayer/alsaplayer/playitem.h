/*  playitem.h
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

#ifndef __PLAYITEM_H__
#define __PLAYITEM_H__

#include <glib-object.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* TODO: Document it for gtkdoc */

/* Macro for PlayItem type. */
#define TYPE_PLAYITEM			(playitem_get_type ())
#define PLAYITEM(playitem)		(G_TYPE_CHECK_INSTANCE_CAST ((object), TYPE_PLAYITEM, PlayItem))
#define IS_PLAYITEM(playitem)		(G_TYPE_CHECK_INSTANCE_TYPE ((playitem), TYPE_PLAYITEM))
    
/* forward declaration to avoid excessive includes (and concurrent includes) */
typedef struct _PlayItem   PlayItem;
typedef struct _PlayItemClass   PlayItemClass;

/*! \brief PlayItem object structure which represents one PlayList entry. */
struct _PlayItem {
    /*! \brief Parent object structure.
     * 
     *  The gobject structure needs to be the first
     *  element in the playitem structure in order for
     *  the object mechanism to work correctly. This
     *  allows a PlayItem pointer to be cast to a
     *  GObject pointer.
     */
    GObject	    gobject;

    /*! \brief Filename of this song. */
    gchar	    *filename;
    
    /*! \brief Title of this song. */
    gchar	    *title;

    /*! \brief Artist of this song. */
    gchar	    *artist;

    /*! \brief Album of this song. */
    gchar	    *album;

    /*! \brief Genre of this song. */
    gchar	    *genre;

    /*! \brief Comment of this song. */
    gchar	    *comment;

    /*! \brief Year of this song. */
    guint	    year;

    /*! \brief Track number of this song. */
    guint	    track;

    /*! \brief Playtime of this song. */
    guint	    playtime;
};

struct _PlayItemClass {
    /*! \brief Parent class structure.
     * 
     *  The gobject class structure needs to be the first
     *  element in the playitem class structure in order for
     *  the class mechanism to work correctly. This allows a
     *  PlayItemClass pointer to be cast to a GObjectClass
     *  pointer.
     */
    GObjectClass    gobject_class;
};

GType			    playitem_get_type		(void) G_GNUC_CONST;
PlayItem*		    playitem_new		(const gchar	*filename);
void			    playitem_set_filename	(PlayItem	*playitem,
							 const gchar    *filename);
void			    playitem_set_title		(PlayItem	*playitem,
							 const gchar    *title);
void			    playitem_set_artist		(PlayItem	*playitem,
							 const gchar    *artist);
void			    playitem_set_album		(PlayItem	*playitem,
							 const gchar    *album);
void			    playitem_set_genre		(PlayItem	*playitem,
							 const gchar    *genre);
void			    playitem_set_comment	(PlayItem	*playitem,
							 const gchar    *comment);
void			    playitem_set_year		(PlayItem	*playitem,
							 guint		year);
void			    playitem_set_track		(PlayItem	*playitem,
							 guint		track);
void			    playitem_set_playtime	(PlayItem	*playitem,
							 guint		playtime);
G_CONST_RETURN gchar*	    playitem_get_filename	(PlayItem	*playitem);
G_CONST_RETURN gchar*	    playitem_get_title		(PlayItem	*playitem);
G_CONST_RETURN gchar*	    playitem_get_artist		(PlayItem	*playitem);
G_CONST_RETURN gchar*	    playitem_get_album		(PlayItem	*playitem);
G_CONST_RETURN gchar*	    playitem_get_genre		(PlayItem	*playitem);
G_CONST_RETURN gchar*	    playitem_get_comment	(PlayItem	*playitem);
guint			    playitem_get_year		(PlayItem	*playitem);
guint			    playitem_get_track		(PlayItem	*playitem);
guint			    playitem_get_playtime	(PlayItem	*playitem);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PLAYITEM_H__ */
