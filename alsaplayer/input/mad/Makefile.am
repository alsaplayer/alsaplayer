
EXTRA_DIST = D.dat huffman.h mad.h stream.h fixed.h \
		imdct_s.dat qc_table.dat synth.h bit.h frame.h layer12.h \
		rq_table.dat timer.h global.h layer3.h sf_table.dat

madltlibs = libmad.la
madldflags = -export-dynamic -avoid-version
madsources = bit.c frame.c layer12.c stream.c timer.c \
		fixed.c huffman.c layer3.c synth.c mad_engine.cpp

lib_LTLIBRARIES = $(madltlibs)

libdir = $(ADDON_DIR)/input

COMMON_CFLAGS = -I$(top_builddir) -I$(top_srcdir)/include \
	@ARCH_CFLAGS@ -D_REENTRANT @DFLAGS@

CFLAGS = $(COMMON_CFLAGS) $(EXTRA_CFLAGS)

CXXFLAGS = $(CFLAGS)

INCLUDES = -I$(ROOT_DIR)/app $(GLIB_CFLAGS)

libmad_la_SOURCES = $(madsources)
libmad_la_LDFLAGS = $(madldflags)

