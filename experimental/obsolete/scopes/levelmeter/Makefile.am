if HAVE_GTK

levelmeterltlibs = liblevelmeter.la
levelmeterldflags = -export-dynamic -avoid-version @GTK_LIBS@
levelmetersources = levelmeter.c

lib_LTLIBRARIES = $(levelmeterltlibs)

else

levelmeterltlibs =
levelmeterldflags =
levelmetersources =

endif

libdir = $(ADDON_DIR)/scopes

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@

liblevelmeter_la_LDFLAGS = $(levelmeterldflags)
LIBS =

liblevelmeter_la_SOURCES= $(levelmetersources)
