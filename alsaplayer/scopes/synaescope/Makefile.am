if HAVE_GTK

synaescopeltlibs = libsynaescope.la
synaescopeldflags = -export-dynamic -avoid-version @GTK_LIBS@
synaescopesources = synaescope.c

lib_LTLIBRARIES = $(synaescopeltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@

else

synaescopeltlibs =
synaescopeldflags =
synaescopesources =

endif

libdir = $(ADDON_DIR)/scopes

libsynaescope_la_LDFLAGS = $(synaescopeldflags)
LIBS =

libsynaescope_la_SOURCES= $(synaescopesources)
