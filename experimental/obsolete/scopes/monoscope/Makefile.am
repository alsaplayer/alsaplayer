if HAVE_GTK

monoscopeltlibs = libmonoscope.la
monoscopeldflags = -export-dynamic -avoid-version @GTK_LIBS@
monoscopesources = monoscope.c

lib_LTLIBRARIES = $(monoscopeltlibs)

else

monoscopeltlibs =
monoscopeldflags =
monoscopesources =

endif

libdir = $(ADDON_DIR)/scopes

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@


libmonoscope_la_LDFLAGS = $(monoscopeldflags)
LIBS =

libmonoscope_la_SOURCES= $(monoscopesources)
