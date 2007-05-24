if HAVE_GTK2

monoscopeltlibs = libmonoscope.la
monoscopeldflags = -export-dynamic -avoid-version @GTK2_LIBS@
monoscopesources = monoscope.c

lib_LTLIBRARIES = $(monoscopeltlibs)

else

monoscopeltlibs =
monoscopeldflags =
monoscopesources =

endif

libdir = $(ADDON_DIR)/scopes2

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK2_CFLAGS@ @DFLAGS@


libmonoscope_la_LDFLAGS = $(monoscopeldflags)
LIBS =

libmonoscope_la_SOURCES= $(monoscopesources)
