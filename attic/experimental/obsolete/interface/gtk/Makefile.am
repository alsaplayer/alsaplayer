SUBDIRS = pixmaps gui

if HAVE_GTK

gtk_interfaceltlibs = libgtk_interface.la
gtk_interfaceldflags = -export-dynamic -avoid-version
gtk_interfacesources = EffectsWindow.cpp PlaylistWindow.cpp ScopesWindow.h  \
	gtk.cpp support.cpp EffectsWindow.h PlaylistWindow.h \
	gladesrc.cpp ScopesWindow.cpp gladesrc.h support.h \
	gtk_interface.cpp gtk_interface.h

else

gtk_interfaceltlibs =
gtk_interfaceldflags =
gtk_interfacesources =

endif

lib_LTLIBRARIES = $(gtk_interfaceltlibs)

COMMON_FLAGS = @DFLAGS@ @ARCH_CFLAGS@

AM_CXXFLAGS = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		@GLIB_CFLAGS@ @GTK_CFLAGS@ $(COMMON_FLAGS) \
		-D_REENTRANT -DADDON_DIR=\"$(ADDON_DIR)\"

AM_CFLAGS = $(CXXFLAGS)

libdir = $(ADDON_DIR)/interface

libgtk_interface_la_LDFLAGS = $(gtk_interfaceldflags)
libgtk_interface_la_LIBADD = @GTK_LIBS@ @GLIB_LIBS@
libgtk_interface_la_SOURCES= $(gtk_interfacesources)
