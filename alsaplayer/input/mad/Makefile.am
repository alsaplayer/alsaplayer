
EXTRA_DIST = D.dat huffman.h mad.h stream.h fixed.h \
		imdct_s.dat qc_table.dat synth.h bit.h frame.h layer12.h \
		rq_table.dat timer.h global.h layer3.h sf_table.dat bit.c \
		frame.c layer12.c stream.c timer.c fixed.c huffman.c \
		layer3.c synth.c xing.c xing.h mad_engine.c mad_engine2.c

mad_inltlibs = libmad_in.la
mad_inldflags = -export-dynamic -avoid-version

if USE_LIBMAD

mad_insources = mad_engine2.c xing.c

else

mad_insources = bit.c frame.c layer12.c stream.c timer.c \
		fixed.c huffman.c layer3.c synth.c mad_engine2.c xing.c

endif

lib_LTLIBRARIES = $(mad_inltlibs)

libdir = $(ADDON_DIR)/input

COMMON_CFLAGS = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
	@ARCH_CFLAGS@ -D_REENTRANT @DFLAGS@

CFLAGS = $(COMMON_CFLAGS) $(EXTRA_CFLAGS)

CXXFLAGS = $(CFLAGS)

INCLUDES = -I$(ROOT_DIR)/app $(GLIB_CFLAGS)

libmad_in_la_SOURCES = $(mad_insources)
libmad_in_la_LDFLAGS = $(mad_inldflags) @MAD_LDADD@

CLEANFILES = libmad.la
