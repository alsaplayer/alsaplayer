#ifndef __ScopesWindow_h__
#define __ScopesWindow_h__

#include <gtk/gtk.h>
#include "scope_plugin.h"

extern GtkWidget *init_scopes_window();
extern int apRegisterScopePlugin(scope_plugin *plugin);
extern void apUnregiserScopePlugins();
extern bool scope_feeder_func(void *, void *, int);
#endif
