if HAVE_GTK2

blurscopeltlibs = libblurscope.la
blurscopeldflags = -export-dynamic -avoid-version @GTK2_LIBS@
blurscopesources = blurscope.c

lib_LTLIBRARIES = $(blurscopeltlibs)

else

blurscopeltlibs =
blurscopeldflags =
blurscopesources =

endif

libdir = $(ADDON_DIR)/scopes2

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK2_CFLAGS@ @DFLAGS@

libblurscope_la_LDFLAGS = $(blurscopeldflags)
LIBS =

libblurscope_la_SOURCES= $(blurscopesources)
