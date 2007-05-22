fftscopeltlibs = libfftscope.la
fftscopeldflags = -export-dynamic -avoid-version @GTK_LIBS@
fftscopesources = fftscope.c

lib_LTLIBRARIES = $(fftscopeltlibs)

libdir = $(SCOPEPLUGIN_DIR)

AM CFLAGS = -D_REENTRANT -I$(top_builddir) @GTK_CFLAGS@ @ALSAPLAYER_CFLAGS@

libfftscope_la_LDFLAGS = $(fftscopeldflags)
LIBS =

libfftscope_la_SOURCES= $(fftscopesources)

EXTRA_DIST = bootstrap fftscope.png
