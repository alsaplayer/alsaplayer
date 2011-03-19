if HAVE_GTK

synaescopeltlibs = libsynaescope.la
synaescopeldflags = -export-dynamic -avoid-version @GTK_LIBS@
synaescopesources = synaescope.c

lib_LTLIBRARIES = $(synaescopeltlibs)

else

synaescopeltlibs =
synaescopeldflags =
synaescopesources =

endif

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@

libdir = $(ADDON_DIR)/scopes

libsynaescope_la_LDFLAGS = $(synaescopeldflags)
LIBS =

libsynaescope_la_SOURCES= $(synaescopesources)
