if HAVE_AUDIOFILE

audiofileltlibs = libaf.la
audiofileldflags = -export-dynamic -avoid-version
audiofilesources = audiofile_engine.c

else

audiofileltlibs =
audiofileldflags =
audiofilesources =

endif

EXTRA_DIST = audiofile_engine.c

lib_LTLIBRARIES = $(audiofileltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@AUDIOFILE_CFLAGS@

libdir = $(ADDON_DIR)/input

libaf_la_LDFLAGS = $(audiofileldflags)
LIBS = @AUDIOFILE_LIBS@

libaf_la_SOURCES= $(audiofilesources)
