if HAVE_GTK2

spacescopeltlibs = libspacescope.la
spacescopeldflags = -export-dynamic -avoid-version @GTK2_LIBS@
spacescopesources = spacescope.c

lib_LTLIBRARIES = $(spacescopeltlibs)

else

spacescopeltlibs =
spacescopeldflags =
spacescopesources =

endif

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK2_CFLAGS@ @DFLAGS@

libdir = $(ADDON_DIR)/scopes2

libspacescope_la_LDFLAGS = $(spacescopeldflags)
LIBS =

libspacescope_la_SOURCES= $(spacescopesources)
