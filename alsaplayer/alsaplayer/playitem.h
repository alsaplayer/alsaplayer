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

#include "object.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @file playitem.h
 */

/**
 * @brief	Returns the type ID of the #ApPlayItem type.
 */
#define AP_TYPE_PLAYITEM		(ap_playitem_get_type ())

/**
 * @brief	Cast a #ApPlayItem or derived pointer
 *		into a (ApPlayItem*) pointer.
 * 
 * Depending on the current debugging level, this function may invoke
 * certain runtime checks to identify invalid casts
 */
#define AP_PLAYITEM(playitem)		(G_TYPE_CHECK_INSTANCE_CAST ((playitem), AP_TYPE_PLAYITEM, ApPlayItem))

/**
 * @brief	Check whether a valid #ApPlayItem
 *		pointer is of type #AP_TYPE_PLAYITEM.
 */
#define AP_IS_PLAYITEM(playitem)	(G_TYPE_CHECK_INSTANCE_TYPE ((playitem), AP_TYPE_PLAYITEM))

/**
 * @brief	This is opaque structure for a playitem type.
 *
 * All the fields in the #ApPlayItem structure are private to the #ApPlayItem
 * implementation and should never be accessed directly.
 */
typedef struct _ApPlayItem	    ApPlayItem;

/**
 * @brief	This is opaque structure for a playlist class.
 *
 * All the fields in the #ApPlayItemClass structure are private
 * to the #ApPlayItemClass implementation and should never be accessed directly.
 */
typedef struct _ApPlayItemClass	    ApPlayItemClass;

struct _ApPlayItem {
    /* Parent object structure.
     * 
     * The ap_object structure needs to be the first
     * element in the playitem structure in order for
     * the object mechanism to work correctly. This
     * allows a ApPlayItem pointer to be cast to a
     * ApObject pointer.
     */

    ApObject	    ap_object;
    gchar	    *filename;
    gchar	    *title;
    gchar	    *artist;
    gchar	    *album;
    gchar	    *genre;
    gchar	    *comment;
    guint	    year;
    guint	    track;
    guint	    playtime;
};

struct _ApPlayItemClass {
    /* Parent class structure.
     * 
     * The ap_object class structure needs to be the first
     * element in the playitem class structure in order for
     * the class mechanism to work correctly. This allows a
     * ApPlayItemClass pointer to be cast to a ApObjectClass
     * pointer.
     */
    ApObjectClass    ap_object_class;
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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PLAYITEM_H__ */
