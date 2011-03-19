fftscopeltlibs = libfftscope.la
fftscopeldflags = -export-dynamic -avoid-version @GTK2_LIBS@
fftscopesources = fftscope.c

lib_LTLIBRARIES = $(fftscopeltlibs)

libdir = $(PLUGIN_DIR)/scopes2

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) @GTK2_CFLAGS@ @ALSAPLAYER_CFLAGS@

libfftscope_la_LDFLAGS = $(fftscopeldflags)
LIBS =

libfftscope_la_SOURCES= $(fftscopesources)

EXTRA_DIST = bootstrap fftscope.png
