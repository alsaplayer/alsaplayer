/*  EffectsWindow.h
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
 *  Copyright (C) 2007 Madej
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
*/ 
#ifndef __ScopesWindow_h__
#define __ScopesWindow_h__

#include <gtk/gtk.h>
#include "scope_plugin.h"

extern GtkWidget *init_scopes_window(GtkWidget *main_window);
extern void destroy_scopes_window();
extern int apRegisterScopePlugin(scope_plugin *plugin);
extern void apUnregiserScopePlugins();
extern bool scope_feeder_func(void *, void *, int);

typedef struct _scope_entry
{
	scope_plugin *sp;
	struct _scope_entry *next;
	struct _scope_entry *prev;
	int active;
} scope_entry;

#endif
