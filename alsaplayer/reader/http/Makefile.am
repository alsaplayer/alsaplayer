httpltlibs = libhttp.la
httpldflags = -export-dynamic -avoid-version
httpsources = http.c

lib_LTLIBRARIES = $(httpltlibs)

AM_CFLAGS = @DFLAGS@ -Wall -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer

libdir = $(ADDON_DIR)/reader

libhttp_la_LDFLAGS = $(httpldflags)
LIBS =

libhttp_la_SOURCES= $(httpsources)
