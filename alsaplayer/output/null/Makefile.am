nullltlibs = libnull.la
nullldflags = -export-dynamic -avoid-version
nullsources = null.c

lib_LTLIBRARIES = $(nullltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include @ARCH_CFLAGS@

libdir = $(ADDON_DIR)/output

libnull_la_LDFLAGS = $(nullldflags)
LIBS =

libnull_la_SOURCES= $(nullsources)
