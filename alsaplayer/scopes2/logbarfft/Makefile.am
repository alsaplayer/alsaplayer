if HAVE_GTK2

logbarfftltlibs = liblogbarfft.la
logbarfftldflags = -export-dynamic -avoid-version @GTK2_LIBS@
logbarfftsources = logbarfft.c

lib_LTLIBRARIES = $(logbarfftltlibs)

else

logbarfftltlibs =
logbarfftldflags =
logbarfftsources =

endif

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			-I$(top_srcdir)/app/scopes \
			@ARCH_CFLAGS@ @GTK2_CFLAGS@ @DFLAGS@

libdir = $(ADDON_DIR)/scopes2

liblogbarfft_la_LDFLAGS = $(logbarfftldflags)
LIBS =

liblogbarfft_la_SOURCES= $(logbarfftsources)
