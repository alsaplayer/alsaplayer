/*  object.h - Threads safe version of functions from GObject.
 *  Copyright (C) 2002 Evgeny Chukreev <codedj@echo.ru>
 *
 *  This file is part of AlsaPlayer.
 *
 *  AlsaPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AlsaPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  $Id$
 *
*/ 

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include <glib-object.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @file object.h
 *
 * @brief	Declarations for #ApObject and #ApObjectClass.
 * 
 */

/**
 * @brief	Returns the type ID of the #ApObject type.
 */
#define AP_TYPE_OBJECT			(ap_object_get_type ())

/**
 * @brief	Cast a #ApObject or derived pointer
 *		into a (ApObject*) pointer.
 * 
 * Depending on the current debugging level, this function may invoke
 * certain runtime checks to identify invalid casts
 */
#define AP_OBJECT(object)		(G_TYPE_CHECK_INSTANCE_CAST ((object), AP_TYPE_OBJECT, ApObject))

/**
 * @brief	Check whether a valid #ApObject
 *		pointer is of type #AP_TYPE_OBJECT.
 */
#define AP_IS_OBJECT(object)		(G_TYPE_CHECK_INSTANCE_TYPE ((object), AP_TYPE_OBJECT))

/**
 * @brief	This is opaque structure for an object type.
 *
 * All the fields in the #ApObject structure are private to the #ApObject
 * implementation and should never be accessed directly.
 */
typedef struct _ApObject	    ApObject;

/**
 * @brief	This is opaque structure for a object class.
 *
 * All the fields in the #ApObjectClass structure are private
 * to the #ApObjectClass implementation and should never be accessed directly.
 */
typedef struct _ApObjectClass	    ApObjectClass;

struct _ApObject {
    /* Parent object structure.
     * 
     * The gobject structure needs to be the first
     * element in the playitem structure in order for
     * the object mechanism to work correctly. This
     * allows a ApPlayItem pointer to be cast to a
     * GObject pointer.
     */
    GObject		    gobject;

    guint		    ref;
    GStaticRecMutex	    mutex;
};

struct _ApObjectClass {
    /* Parent class structure.
     * 
     * The gobject class structure needs to be the first
     * element in the playitem class structure in order for
     * the class mechanism to work correctly. This allows a
     * ApPlayItemClass pointer to be cast to a GObjectClass
     * pointer.
     */
    GObjectClass    gobject_class;
};

GType			    ap_object_get_type		(void) G_GNUC_CONST;

void			    ap_object_ref		(ApObject	*object);
void			    ap_object_unref		(ApObject	*object);

void			    ap_object_lock		(ApObject	*object);
void			    ap_object_unlock		(ApObject	*object);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OBJECT_H__ */
