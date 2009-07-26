EXTRA_DIST = xosd.cpp

if HAVE_XOSD

xosd_interfaceltlibs = libxosd_interface.la
xosd_interfaceldflags = -export-dynamic -avoid-version -lxosd
xosd_interfacesources = xosd.cpp

else

xosd_interfaceltlibs =
xosd_interfaceldflags =
xosd_interfacesources =

endif

lib_LTLIBRARIES = $(xosd_interfaceltlibs)

COMMON_FLAGS = @ARCH_CFLAGS@

AM_CXXFLAGS = @DFLAGS@ $(COMMON_FLAGS) \
		-D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		-DADDON_DIR=\"$(ADDON_DIR)\"

libdir = $(ADDON_DIR)/interface

libxosd_interface_la_LDFLAGS = $(xosd_interfaceldflags)
libxosd_interface_la_LIBADD =
libxosd_interface_la_SOURCES= $(xosd_interfacesources)
