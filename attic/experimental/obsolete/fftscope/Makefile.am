fftscopeltlibs = libfftscope.la
fftscopeldflags = -export-dynamic -avoid-version @GTK_LIBS@
fftscopesources = fftscope.c

SUBDIRS = gtk2

lib_LTLIBRARIES = $(fftscopeltlibs)

libdir = $(SCOPEPLUGIN_DIR)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) @GTK_CFLAGS@ @ALSAPLAYER_CFLAGS@

libfftscope_la_LDFLAGS = $(fftscopeldflags)
LIBS =

libfftscope_la_SOURCES= $(fftscopesources)

EXTRA_DIST = bootstrap fftscope.png gtk2

