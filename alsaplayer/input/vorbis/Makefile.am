if HAVE_OGGVORBIS

vorbisltlibs = libvorbis.la
vorbisldflags = -export-dynamic -avoid-version
vorbissources = vorbis_engine.cpp

else

vorbisltlibs =
vorbisldflags =
vorbissources =

endif

EXTRA_DIST = vorbis_engine.cpp

lib_LTLIBRARIES = $(vorbisltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include

CXXFLAGS = $(CFLAGS)

libdir = $(ADDON_DIR)/input

libvorbis_la_LDFLAGS = $(vorbisldflags)
libvorbis_la_LIBADD = @VORBIS_LIBS@ @OGG_LIBS@ @VORBISFILE_LIBS@
libvorbis_la_SOURCES= $(vorbissources)
