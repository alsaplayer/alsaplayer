if HAVE_FLAC

flac_inltlibs = libflac_in.la
flac_inldflags = -export-dynamic -avoid-version
flac_insources = FlacEngine.cpp FlacPlugin.cpp FlacFile.cpp FlacTag.cpp

else

flac_inltlibs =
flac_inldflags =
flac_insources =

endif

if USE_LIBID3TAG

flac_inid3sources = FlacId3tagTag.cpp

else

flac_inid3sources = FlacId3v1Tag.cpp

endif

EXTRA_DIST = FlacEngine.cpp FlacPlugin.cpp FlacFile.cpp FlacTag.cpp \
	     FlacId3v1Tag.cpp FlacId3tagTag.cpp FlacEngine.h FlacFile.h \
	     FlacId3Tag.h FlacTag.h

lib_LTLIBRARIES = $(flac_inltlibs)

INCLUDES = -I$(top_builddir) -I$(top_srcdir)/alsaplayer

CXXFLAGS = -D_REENTRANT $(EXTRA_CFLAGS)

libdir = $(ADDON_DIR)/input

libflac_in_la_LDFLAGS = $(flac_inldflags)
libflac_in_la_LIBADD = @FLAC_LDADD@
libflac_in_la_SOURCES= $(flac_insources) $(flac_inid3sources)
