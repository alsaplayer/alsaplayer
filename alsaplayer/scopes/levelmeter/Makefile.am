if HAVE_GTK

levelmeterltlibs = liblevelmeter.la
levelmeterldflags = -export-dynamic -avoid-version @GTK_LIBS@
levelmetersources = levelmeter.c

lib_LTLIBRARIES = $(levelmeterltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ @DFLAGS@

else

levelmeterltlibs =
levelmeterldflags =
levelmetersources =

endif

libdir = $(ADDON_DIR)/scopes

liblevelmeter_la_LDFLAGS = $(levelmeterldflags)
LIBS =

liblevelmeter_la_SOURCES= $(levelmetersources)
