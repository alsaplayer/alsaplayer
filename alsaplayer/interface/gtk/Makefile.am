SUBDIRS = pixmaps gui

if HAVE_GTK

gtkltlibs = libgtk.la
gtkldflags = -export-dynamic -avoid-version
gtksources = EffectsWindow.cpp PlaylistWindow.cpp ScopesWindow.h  \
				gtk.cpp support.cpp EffectsWindow.h PlaylistWindow.h \
				gladesrc.cpp ScopesWindow.cpp gladesrc.h support.h \
				gtk_interface.cpp gtk_interface.h

else

gtkltlibs =
gtkldflags =
gtksources =

endif

lib_LTLIBRARIES = $(gtkltlibs)

CXXFLAGS = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		@GLIB_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@ $(COMMON_FLAGS) \
		$(EXTRA_CFLAGS) \
		-D_REENTRANT -DADDON_DIR=\"$(ADDON_DIR)\"

CFLAGS = $(CXXFLAGS)

libdir = $(ADDON_DIR)/interface

libgtk_la_LDFLAGS = $(gtkldflags)
libgtk_la_LIBADD = @GTK_LIBS@ @GLIB_LIBS@
libgtk_la_SOURCES= $(gtksources)
