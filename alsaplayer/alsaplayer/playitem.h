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
#define AP_TYPE_PLAYITEM		(ap_playitem_get_type ())
#define AP_PLAYITEM(playitem)		(G_TYPE_CHECK_INSTANCE_CAST ((playitem), AP_TYPE_PLAYITEM, ApPlayItem))
#define AP_IS_PLAYITEM(playitem)	(G_TYPE_CHECK_INSTANCE_TYPE ((playitem), AP_TYPE_PLAYITEM))
    
/* forward declaration to avoid excessive includes (and concurrent includes) */
typedef struct _ApPlayItem	    ApPlayItem;
typedef struct _ApPlayItemClass	    ApPlayItemClass;

/*! \brief This structure represents one PlayList entry. */
struct _ApPlayItem {
    /*! \brief Parent object structure.
     * 
     *  The gobject structure needs to be the first
     *  element in the playitem structure in order for
     *  the object mechanism to work correctly. This
     *  allows a ApPlayItem pointer to be cast to a
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

    GMutex	    *mutex;
};

struct _ApPlayItemClass {
    /*! \brief Parent class structure.
     * 
     *  The gobject class structure needs to be the first
     *  element in the playitem class structure in order for
     *  the class mechanism to work correctly. This allows a
     *  ApPlayItemClass pointer to be cast to a GObjectClass
     *  pointer.
     */
    GObjectClass    gobject_class;
};

GType			    ap_playitem_get_type	(void) G_GNUC_CONST;
ApPlayItem*		    ap_playitem_new		(const gchar	*filename);
void			    ap_playitem_set_filename	(ApPlayItem	*playitem,
							 const gchar    *filename);
void			    ap_playitem_set_title	(ApPlayItem	*playitem,
							 const gchar    *title);
void			    ap_playitem_set_artist	(ApPlayItem	*playitem,
							 const gchar    *artist);
void			    ap_playitem_set_album	(ApPlayItem	*playitem,
							 const gchar    *album);
void			    ap_playitem_set_genre	(ApPlayItem	*playitem,
							 const gchar    *genre);
void			    ap_playitem_set_comment	(ApPlayItem	*playitem,
							 const gchar    *comment);
void			    ap_playitem_set_year	(ApPlayItem	*playitem,
							 guint		year);
void			    ap_playitem_set_track	(ApPlayItem	*playitem,
							 guint		track);
void			    ap_playitem_set_playtime	(ApPlayItem	*playitem,
							 guint		playtime);
G_CONST_RETURN gchar*	    ap_playitem_get_filename	(ApPlayItem	*playitem);
G_CONST_RETURN gchar*	    ap_playitem_get_title	(ApPlayItem	*playitem);
G_CONST_RETURN gchar*	    ap_playitem_get_artist	(ApPlayItem	*playitem);
G_CONST_RETURN gchar*	    ap_playitem_get_album	(ApPlayItem	*playitem);
G_CONST_RETURN gchar*	    ap_playitem_get_genre	(ApPlayItem	*playitem);
G_CONST_RETURN gchar*	    ap_playitem_get_comment	(ApPlayItem	*playitem);
guint			    ap_playitem_get_year	(ApPlayItem	*playitem);
guint			    ap_playitem_get_track	(ApPlayItem	*playitem);
guint			    ap_playitem_get_playtime	(ApPlayItem	*playitem);

void			    ap_playitem_lock		(ApPlayItem	*playitem);
void			    ap_playitem_unlock		(ApPlayItem	*playitem);
void			    ap_playitem_ref		(ApPlayItem	*playitem);
void			    ap_playitem_unref		(ApPlayItem	*playitem);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PLAYITEM_H__ */
