daemonltlibs = libdaemon.la
daemonldflags = -export-dynamic -avoid-version
daemonsources = daemon.cpp

lib_LTLIBRARIES = $(daemonltlibs)

CXXFLAGS = @DFLAGS@ $(COMMON_FLAGS) $(EXTRA_CFLAGS) \
		-D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		-DADDON_DIR=\"$(ADDON_DIR)\"

CFLAGS = $(CXXFLAGS)

libdir = $(ADDON_DIR)/interface

libdaemon_la_LDFLAGS = $(daemonldflags)
libdaemon_la_LIBADD =
libdaemon_la_SOURCES= $(daemonsources)
