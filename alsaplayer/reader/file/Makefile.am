fileltlibs = libfile.la
fileldflags = -export-dynamic -avoid-version
filesources = file.c

lib_LTLIBRARIES = $(fileltlibs)

AM_CFLAGS = @DFLAGS@ -Wall -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer

libdir = $(ADDON_DIR)/reader

libfile_la_LDFLAGS = $(fileldflags)
LIBS =

libfile_la_SOURCES= $(filesources)
