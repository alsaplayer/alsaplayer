if HAVE_SPARC

sparcltlibs = libsparc.la
sparcldflags = -export-dynamic -avoid-version
sparcsources = sparc.c

else

sparclitlibs =
sparcldflags =
sparcsources =

endif

EXTRA_DIST = sparc.c

lib_LTLIBRARIES = $(sparcltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include @ARCH_CFLAGS@ 

libdir = $(ADDON_DIR)/output

libsparc_la_LDFLAGS = $(sparcldflags)
LIBS =

libsparc_la_SOURCES= $(sparcsources)
