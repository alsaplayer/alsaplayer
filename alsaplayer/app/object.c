/*  object.c - Threads safe version of functions from GObject.
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
*/

#include <glib-object.h>

#include "object.h"

/* --- prototypes --- */
static void	ap_object_class_init	    (ApObjectClass	*class);
static void	ap_object_init		    (ApObject		*object);
static void	ap_object_finalize	    (GObject		*object);

/* --- variables --- */
static gpointer			    parent_class = NULL;
static GStaticMutex		    ref_mutex = G_STATIC_MUTEX_INIT;

/* --- functions --- */
static void
ap_object_class_init (ApObjectClass *class)
{
    /* Like aliases */
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    
    /* Peek parent class */
    parent_class = g_type_class_peek_parent (class);
    
    /* Init GObject Class */
    gobject_class->finalize = ap_object_finalize;
} /* ap_object_class_init */

static void
ap_object_init (ApObject *object)
{
    /* Local reference counter */
    object->ref = 1;

    /* Init mutex for this item */
    g_static_rec_mutex_init (&object->mutex);
} /* ap_object_init() */

static void
ap_object_finalize (GObject *gobject)
{
    ApObject *object = AP_OBJECT (gobject); 

    /* Destroy local mutex */
    g_static_rec_mutex_free (&object->mutex);

    G_OBJECT_CLASS (parent_class)->finalize (gobject);
} /* ap_object_finalize */

/* ******************************************************************** */
/* Public functions.                                                    */

/**
 * @brief	    Register the #ApObjectClass if necessary,
 *		    and returns the type ID associated to it.
 *
 * @return	    The type ID of the #ApObjectClass.
 **/
GType
ap_object_get_type (void)
{
    static GType type = 0;
    static const GTypeInfo object_info = {
	sizeof (ApObjectClass),
	NULL,
        NULL,
	(GClassInitFunc) ap_object_class_init,
        NULL,
        NULL,
        sizeof (ApObject),
        0,
        (GInstanceInitFunc) ap_object_init,
        NULL
    };

    if (!type) {
	/* First time create */
	type = g_type_register_static (G_TYPE_OBJECT,	/* Parent Type */
				"ApObject",		/* Name */
				&object_info,		/* Type Info */
				0);			/* Flags */
    }

    return type;
}; /* ap_object_get_type */

/**
 * @param object	An #ApObject.
 * 
 * @brief		Adds a reference to the given object.
 * 
 * This function is the thread safe version of the g_object_ref().
 *
 * @see			ap_object_unref()
 **/
void
ap_object_ref (ApObject *object)
{
    g_return_if_fail (AP_IS_OBJECT (object));

    g_static_mutex_lock (&ref_mutex);
    object->ref++;
    g_static_mutex_unlock (&ref_mutex);
}

/**
 * @param object	An #ApObject.
 *
 * @brief		Inverse of ap_object_unref().
 *
 * This functions is the thread safe version of the g_object_unref().
 *
 * @see			ap_object_ref()
 **/
void
ap_object_unref (ApObject *object)
{
    g_return_if_fail (AP_IS_OBJECT (object));

    g_static_mutex_lock (&ref_mutex);
    if (object->ref == 1) {
	g_static_mutex_unlock (&ref_mutex);
	g_object_unref (object);
    } else {
	object->ref--;
	g_static_mutex_unlock (&ref_mutex);
    }
}

/**
 * @param object    An #ApObject.
 *
 * @brief	    Locks object.
 *
 * If object is already locked by another thread,
 * the current thread will block until object
 * in unlocked by the other thread.
 *
 * @note	    Object can be locked multiple times
 *		    by one thread. If you enter it n times
 *		    you have to unlock it n times again
 *		    to let other threads lock it.
 *
 * @see		    ap_object_unlock()
 **/
void
ap_object_lock (ApObject *object)
{
    g_return_if_fail (AP_IS_OBJECT (object));

    g_static_rec_mutex_lock (&object->mutex);
} /* ap_object_lock */

/**
 * @param object    An #ApObject.
 *
 * @brief	    Unlocks object.
 *
 * If another thread is blocked in a ap_object_lock()
 * call for object, it will be woken and can lock object itself.
 *
 * @note	    Object can be locked multiple times
 *		    by one thread. If you enter it n times
 *		    you have to unlock it n times again
 *		    to let other threads lock it.
 *
 * @see		    ap_object_lock()
 **/
void
ap_object_unlock (ApObject *object)
{
    g_return_if_fail (AP_IS_OBJECT (object));

    g_static_rec_mutex_unlock (&object->mutex);
} /* ap_object_lock */
