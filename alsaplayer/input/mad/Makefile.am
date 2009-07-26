if HAVE_LIBMAD
mad_inltlibs = libmad_in.la
mad_inldflags = -export-dynamic -avoid-version
mad_insources = mad_engine.c xing.c
else
mad_inltlibs =
mad_inldflags =
mad_insources =
endif

if HAVE_LIBMAD
if HAVE_GLIB2
extra_cflags = @GLIB2_CFLAGS@
extra_libs = @GLIB2_LIBS@
else
extra_cflags =
extra_libs =
endif
endif

EXTRA_DIST = xing.c xing.h mad_engine.c


lib_LTLIBRARIES = $(mad_inltlibs)

libdir = $(ADDON_DIR)/input

COMMON_CFLAGS = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
	@ARCH_CFLAGS@ -D_REENTRANT @DFLAGS@

AM_CFLAGS = $(COMMON_CFLAGS)

INCLUDES = -I$(ROOT_DIR)/app $(extra_cflags)

libmad_in_la_SOURCES = $(mad_insources)

libmad_in_la_LDFLAGS = $(mad_inldflags) @MAD_LDADD@ $(extra_cflags)
libmad_in_la_LIBADD = $(extra_libs)

CLEANFILES = libmad.la
