/*  ScopesWindow.h
 *  Copyright (C) 1998 Andy Lo A Foe <andy@alsa-project.org>
 *  Copyright (C) 2007 Madej
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
