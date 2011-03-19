if HAVE_FLC

flac_inltlibs = libflac_in.la
flac_inldflags = -export-dynamic -avoid-version
flac_insources = FlacEngine.cpp FlacPlugin.cpp FlacStream.cpp FlacTag.cpp \
		 FlacSeekableStream.cpp FlacEngine.h FlacId3Tag.h \
		 FlacMetadataTag.cpp FlacMetadataTag.h \
		 FlacSeekableStream.h FlacStream.h FlacTag.h FlacId3v1Tag.cpp

else

flac_inltlibs =
flac_inldflags =
flac_insources =

endif

EXTRA_DIST = FlacEngine.cpp FlacPlugin.cpp FlacStream.cpp FlacTag.cpp \
	     FlacId3v1Tag.cpp FlacEngine.h FlacStream.h FlacId3Tag.h \
	     FlacTag.h FlacSeekableStream.cpp FlacSeekableStream.h \
	     FlacMetadataTag.cpp FlacMetadataTag.h

lib_LTLIBRARIES = $(flac_inltlibs)

INCLUDES = -I$(top_builddir) -I$(top_srcdir)/libalsaplayer \
			-I$(top_srcdir)/alsaplayer

AM_CXXFLAGS = @DFLAGS@ -D_REENTRANT

libdir = $(ADDON_DIR)/input

libflac_in_la_LDFLAGS = $(flac_inldflags)
libflac_in_la_LIBADD = @FLC_LDADD@
libflac_in_la_SOURCES= $(flac_insources)
