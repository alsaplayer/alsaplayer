SUBDIRS = pixmaps

if HAVE_GTK2

gtk2_interfaceltlibs = libgtk2_interface.la
gtk2_interfaceldflags = -export-dynamic -avoid-version
gtk2_interfacesources = EffectsWindow.cpp PlaylistWindow.cpp ScopesWindow.h  \
	gtk.cpp EffectsWindow.h PlaylistWindow.h ScopesWindow.cpp \
	gtk_interface.cpp gtk_interface.h AboutWindow.cpp AboutWindow.h

else

gtk2_interfaceltlibs =
gtk2_interfaceldflags =
gtk2_interfacesources =

endif

lib_LTLIBRARIES = $(gtk2_interfaceltlibs)

COMMON_FLAGS = @DFLAGS@ @ARCH_CFLAGS@

AM_CXXFLAGS = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		@GTK2_CFLAGS@ $(COMMON_FLAGS) \
		-D_REENTRANT -DADDON_DIR=\"$(ADDON_DIR)\"

AM_CFLAGS = $(CXXFLAGS)

libdir = $(ADDON_DIR)/interface

libgtk2_interface_la_LDFLAGS = $(gtk2_interfaceldflags)
libgtk2_interface_la_LIBADD = @GTK2_LIBS@
libgtk2_interface_la_SOURCES= $(gtk2_interfacesources)
