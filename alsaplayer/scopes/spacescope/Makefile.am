if HAVE_GTK

spacescopeltlibs = libspacescope.la
spacescopeldflags = -export-dynamic -avoid-version @GTK_LIBS@
spacescopesources = spacescope.c

lib_LTLIBRARIES = $(spacescopeltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@

else

spacescopeltlibs =
spacescopeldflags =
spacescopesources =

endif

libdir = $(ADDON_DIR)/scopes

libspacescope_la_LDFLAGS = $(spacescopeldflags)
LIBS =

libspacescope_la_SOURCES= $(spacescopesources)
