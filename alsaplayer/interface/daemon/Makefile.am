daemon_interfaceltlibs = libdaemon_interface.la
daemon_interfaceldflags = -export-dynamic -avoid-version
daemon_interfacesources = daemon.cpp

lib_LTLIBRARIES = $(daemon_interfaceltlibs)

COMMON_FLAGS = @ARCH_CFLAGS@

AM_CXXFLAGS = @DFLAGS@ $(COMMON_FLAGS) \
		-D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		-DADDON_DIR=\"$(ADDON_DIR)\"

libdir = $(ADDON_DIR)/interface

libdaemon_interface_la_LDFLAGS = $(daemon_interfaceldflags)
libdaemon_interface_la_LIBADD =
libdaemon_interface_la_SOURCES= $(daemon_interfacesources)
