if HAVE_OGGVORBIS

vorbis_inltlibs = libvorbis_in.la
vorbis_inldflags = -export-dynamic -avoid-version
vorbis_insources = vorbis_engine.c

else

vorbis_inltlibs =
vorbis_inldflags =
vorbis_insources =

endif

EXTRA_DIST = vorbis_engine.c

lib_LTLIBRARIES = $(vorbis_inltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) \
		-I$(top_srcdir)/libalsaplayer -I$(top_srcdir)/alsaplayer

libdir = $(ADDON_DIR)/input

libvorbis_in_la_LDFLAGS = $(vorbis_inldflags)
libvorbis_in_la_LIBADD = @VORBIS_LIBS@ @OGG_LIBS@ @VORBISFILE_LIBS@
libvorbis_in_la_SOURCES= $(vorbis_insources)
