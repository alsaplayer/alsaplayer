/*  scopes.h
 *  Copyright (C) 2003 Andy Lo A Foe <andy@alsaplayer.org>
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
#ifndef __ScopesWindow_h__
#define __ScopesWindow_h__

#include <gtk/gtk.h>
#include "scope_plugin.h"
#include "AlsaNode.h"

#define MINI_W	82
#define MINI_H	28

extern bool init_scopes(AlsaNode *node);
extern bool register_scope(scope_plugin *plugin, bool run, void *arg);
extern void unregister_scopes();
extern bool scope_feeder_func(void *, void *, int);

typedef struct _scope_entry
{
	scope_plugin *sp;
	struct _scope_entry *next;
	struct _scope_entry *prev;
	int active;
} scope_entry;

#endif
