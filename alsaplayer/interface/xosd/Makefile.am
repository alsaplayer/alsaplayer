EXTRA_DIST = xosd.cpp

if HAVE_XOSD

xosdltlibs = libxosd.la
xosdldflags = -export-dynamic -avoid-version -lxosd
xosdsources = xosd.cpp

else

xosdltlibs =
xosdldflags =
xosdsources =

endif

lib_LTLIBRARIES = $(xosdltlibs)

COMMON_FLAGS = @ARCH_CFLAGS@

AM_CXXFLAGS = @DFLAGS@ $(COMMON_FLAGS) \
		-D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		-DADDON_DIR=\"$(ADDON_DIR)\"

AM_CFLAGS = $(CXXFLAGS)

libdir = $(ADDON_DIR)/interface

libxosd_la_LDFLAGS = $(xosdldflags)
libxosd_la_LIBADD =
libxosd_la_SOURCES= $(xosdsources)
