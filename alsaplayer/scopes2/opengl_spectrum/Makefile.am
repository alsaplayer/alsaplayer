if HAVE_OPENGL

oglspectrumltlibs = liboglspectrum.la
oglspectrumsources = opengl_spectrum.c
oglespectrumldflags = -export-dynamic -avoid-version

else

oglspectrumltlibs = 
oglspectrumsources = 
oglespectrumldflags = 

endif

EXTRA_DIST = opengl_spectrum.c

lib_LTLIBRARIES = $(oglspectrumltlibs)

libdir = $(ADDON_DIR)/scopes2

liboglspectrum_la_LIBADD = @OPENGL_LIBS@

liboglspectrum_la_LDFLAGS = $(oglespectrumldflags)

liboglspectrum_la_SOURCES = $(oglspectrumsources)

AM_CFLAGS = -D_REENTRANT @ARCH_CFLAGS@ @DFLAGS@

INCLUDES = -I$(top_srcdir)/alsaplayer -I$(top_builddir)
