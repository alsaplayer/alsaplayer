cliltlibs = libcli.la
clildflags = -export-dynamic -avoid-version
clisources = cli.cpp

lib_LTLIBRARIES = $(cliltlibs)

CXXFLAGS += @DFLAGS@ $(COMMON_FLAGS) $(EXTRA_CFLAGS) \
		-D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
		-DADDON_DIR=\"$(ADDON_DIR)\"

CFLAGS += $(CXXFLAGS)

libdir = $(ADDON_DIR)/interface

libcli_la_LDFLAGS = $(clildflags)
libcli_la_LIBADD =
libcli_la_SOURCES= $(clisources)
