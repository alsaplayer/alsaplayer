EXTRA_DIST= sgi.c

if HAVE_SGI

sgi_outltlibs = libsgi_out.la
sgi_outldflags = -export-dynamic -avoid-version
sgi_outsources = sgi.c

else

sgi_outltlibs =
sgi_outldflags =
sgi_outsources =

endif


lib_LTLIBRARIES = $(sgi_outltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include @ARCH_CFLAGS@ 

libdir = $(ADDON_DIR)/output

libsgi_out_la_LDFLAGS = $(sgi_outldflags)
LIBS = @SGI_LIBS@

libsgi_out_la_SOURCES= $(sgi_outsources)
