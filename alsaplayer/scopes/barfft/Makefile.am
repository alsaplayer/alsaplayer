if HAVE_GTK

gtk_barfft = gtk_barfft.c
barfftltlibs = libbarfft.la
barfftldflags = -export-dynamic -avoid-version @GTK_LIBS@
barfftsources = $(gtk_barfft)

lib_LTLIBRARIES = $(barfftltlibs)

else

gtk_barfft =
barfftltlibs =
barfftldflags =
barfftsources =

endif

EXTRA_DIST = gtk_barfft.c

CFLAGS += -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			-I$(top_srcdir)/app/scopes \
			@GTK_CFLAGS@ @DFLAGS@

libdir = $(ADDON_DIR)/scopes

libbarfft_la_LDFLAGS = $(barfftldflags)
LIBS =

libbarfft_la_SOURCES= $(barfftsources)
