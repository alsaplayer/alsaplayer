wavltlibs = libwav.la
wavldflags = -export-dynamic -avoid-version
wavsources = wav_engine.c formats.h

lib_LTLIBRARIES = $(wavltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include

libdir = $(ADDON_DIR)/input

libwav_la_LDFLAGS = $(wavldflags)
LIBS =

libwav_la_SOURCES= $(wavsources)
