xosdltlibs = libxosd.la
xosdldflags = -export-dynamic -avoid-version
xosdsources = xosd.cpp

lib_LTLIBRARIES = $(xosdltlibs)

AM_CXXFLAGS = @DFLAGS@ $(COMMON_FLAGS) $(EXTRA_CFLAGS) \
		-D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		-DADDON_DIR=\"$(ADDON_DIR)\"

AM_CFLAGS = $(CXXFLAGS)

libdir = $(ADDON_DIR)/interface

libxosd_la_LDFLAGS = $(xosdldflags)
libxosd_la_LIBADD =
libxosd_la_SOURCES= $(xosdsources)
