if HAVE_GTK2

levelmeterltlibs = liblevelmeter.la
levelmeterldflags = -export-dynamic -avoid-version @GTK2_LIBS@
levelmetersources = levelmeter.c

lib_LTLIBRARIES = $(levelmeterltlibs)

else

levelmeterltlibs =
levelmeterldflags =
levelmetersources =

endif

libdir = $(ADDON_DIR)/scopes2

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK2_CFLAGS@ @DFLAGS@

liblevelmeter_la_LDFLAGS = $(levelmeterldflags)
LIBS =

liblevelmeter_la_SOURCES= $(levelmetersources)
