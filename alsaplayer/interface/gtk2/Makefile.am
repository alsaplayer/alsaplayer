SUBDIRS = pixmaps glade

if HAVE_GTK2

gtk2_interfaceltlibs = libgtk2_interface.la
gtk2_interfaceldflags = -export-dynamic -avoid-version
gtk2_interfacesources = support.cpp main.cpp interface.cpp callbacks.cpp \
	scopes.cpp scopes.h \
	internal_scope.cpp internal_scope.h \
	support.h interface.h callbacks.h \
	playlist.cpp playlist.h

else

gtk2_interfaceltlibs =
gtk2_interfaceldflags =
gtk2_interfacesources =

endif

lib_LTLIBRARIES = $(gtk2_interfaceltlibs)


AM_CXXFLAGS = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		@GTK2_CFLAGS@ @DFLAGS@ $(COMMON_FLAGS) \
		-D_REENTRANT -DNO_MAIN_FUNCTION $(ALSAPLAYER_CFLAGS)


AM_CFLAGS = $(AM_CXXFLAGS)

libgtk2_interface_la_LDFLAGS = $(gtk2_interfaceldflags)

if BUILD_EXTERNAL

MY_LIBDIR = $(INTERFACEPLUGIN_DIR)
libgtk2_interface_la_LIBADD = @GTK2_LIBS@ @ALSAPLAYER_LIBS@

else

MY_LIBDIR = $(ADDON_DIR)/interface
libgtk2_interface_la_LIBADD = @GTK2_LIBS@

endif

libdir = $(MY_LIBDIR)
libgtk2_interface_la_SOURCES= $(gtk2_interfacesources)
