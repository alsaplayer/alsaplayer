if HAVE_GTK

spacescopeltlibs = libspacescope.la
spacescopeldflags = -export-dynamic -avoid-version @GTK_LIBS@
spacescopesources = spacescope.c

lib_LTLIBRARIES = $(spacescopeltlibs)

else

spacescopeltlibs =
spacescopeldflags =
spacescopesources =

endif

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@

libdir = $(ADDON_DIR)/scopes

libspacescope_la_LDFLAGS = $(spacescopeldflags)
LIBS =

libspacescope_la_SOURCES= $(spacescopesources)
