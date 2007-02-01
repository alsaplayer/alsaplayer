if HAVE_FLC

if HAVE_OGGFLC
oggflac_insources = OggFlacStream.cpp OggFlacStream.h

else

oggflac_insources =

endif

flac_inltlibs = libflac_in.la
flac_inldflags = -export-dynamic -avoid-version
flac_insources = FlacEngine.cpp FlacPlugin.cpp FlacStream.cpp FlacTag.cpp \
		 FlacSeekableStream.cpp FlacEngine.h FlacId3Tag.h \
		 FlacMetadataTag.cpp FlacMetadataTag.h \
		 FlacSeekableStream.h FlacStream.h FlacTag.h FlacId3v1Tag.cpp \
		 $(oggflac_insources)

else

flac_inltlibs =
flac_inldflags =
flac_insources =

endif

EXTRA_DIST = FlacEngine.cpp FlacPlugin.cpp FlacStream.cpp FlacTag.cpp \
	     FlacId3v1Tag.cpp FlacEngine.h FlacStream.h FlacId3Tag.h \
	     FlacTag.h FlacSeekableStream.cpp FlacSeekableStream.h \
	     FlacMetadataTag.cpp FlacMetadataTag.h \
	     OggFlacStream.cpp OggFlacStream.h

lib_LTLIBRARIES = $(flac_inltlibs)

INCLUDES = -I$(top_builddir) -I$(top_srcdir)/alsaplayer

COMMON_FLAGS = @ARCH_CFLAGS@

AM_CXXFLAGS = -D_REENTRANT $(COMMON_FLAGS)

libdir = $(ADDON_DIR)/input

libflac_in_la_LDFLAGS = $(flac_inldflags)
libflac_in_la_LIBADD = @OGGFLC_LDADD@ @FLC_LDADD@
libflac_in_la_SOURCES= $(flac_insources)
