if HAVE_SNDFILE

sndfileltlibs = libsndfile_in.la
sndfileldflags = -export-dynamic -avoid-version
sndfilesources = sndfile_engine.c

else

sndfileltlibs =
sndfileldflags =
sndfilesources =

endif

EXTRA_DIST = sndfile_engine.c

lib_LTLIBRARIES = $(sndfileltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@SNDFILE_CFLAGS@

libdir = $(ADDON_DIR)/input

libsndfile_in_la_LDFLAGS = $(sndfileldflags)
LIBS = @SNDFILE_LIBS@

libsndfile_in_la_SOURCES= $(sndfilesources)
