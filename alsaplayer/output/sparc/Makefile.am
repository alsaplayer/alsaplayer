if HAVE_SPARC

sparc_outltlibs = libsparc_out.la
sparc_outldflags = -export-dynamic -avoid-version
sparc_outsources = sparc_out.c

else

sparc_outlitlibs =
sparc_outldflags =
sparc_outsources =

endif

EXTRA_DIST = sparc_out.c

lib_LTLIBRARIES = $(sparc_outltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer @ARCH_CFLAGS@ 

libdir = $(ADDON_DIR)/output

libsparc_out_la_LDFLAGS = $(sparc_outldflags)
LIBS =

libsparc_out_la_SOURCES= $(sparc_outsources)
