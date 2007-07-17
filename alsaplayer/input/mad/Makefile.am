
EXTRA_DIST = xing.c xing.h mad_engine.c

mad_inltlibs = libmad_in.la
mad_inldflags = -export-dynamic -avoid-version

mad_insources = mad_engine.c xing.c

if HAVE_GTK2
extra_cflags = @GTK2_CFLAGS@
extra_libs = @GTK2_LIBS@
else
extra_cflags =
extra_libs =
endif

lib_LTLIBRARIES = $(mad_inltlibs)

libdir = $(ADDON_DIR)/input

COMMON_CFLAGS = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
	@ARCH_CFLAGS@ -D_REENTRANT @DFLAGS@

AM_CFLAGS = $(COMMON_CFLAGS)

AM_CXXFLAGS = $(CFLAGS)

INCLUDES = -I$(ROOT_DIR)/app $(extra_cflags)

libmad_in_la_SOURCES = $(mad_insources)

libmad_in_la_LDFLAGS = $(mad_inldflags) @MAD_LDADD@ $(extra_cflags)
libmad_in_la_LIBADD = $(extra_libs)

CLEANFILES = libmad.la
