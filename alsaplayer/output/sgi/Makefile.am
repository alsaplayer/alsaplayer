EXTRA_DIST= sgi.c

if HAVE_SGI

sgiltlibs = libsgi.la
sgildflags = -export-dynamic -avoid-version
sgisources = sgi.c

else

sgiltlibs =
sgildflags =
sgisources =

endif


lib_LTLIBRARIES = $(sgiltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include @ARCH_CFLAGS@ 

libdir = $(ADDON_DIR)/output

libsgi_la_LDFLAGS = $(sgildflags)
LIBS = @SGI_LIBS@

libsgi_la_SOURCES= $(sgisources)
