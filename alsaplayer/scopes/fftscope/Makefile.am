if HAVE_GTK

fftscopeltlibs = libfftscope.la
fftscopeldflags = -export-dynamic -avoid-version @GTK_LIBS@
fftscopesources = fftscope.c

lib_LTLIBRARIES = $(fftscopeltlibs)

CFLAGS += -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@

else

fftscopeltlibs =
fftscopeldflags =
fftscopesources =

endif

libdir = $(ADDON_DIR)/scopes


libfftscope_la_LDFLAGS = $(fftscopeldflags)
LIBS =

libfftscope_la_SOURCES= $(fftscopesources)
