textltlibs = libtext.la
textldflags = -export-dynamic -avoid-version
textsources = text.cpp

lib_LTLIBRARIES = $(textltlibs)

CXXFLAGS = @DFLAGS@ $(COMMON_FLAGS) $(EXTRA_CFLAGS) \
		-D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		-DADDON_DIR=\"$(ADDON_DIR)\"

AM_CFLAGS = $(CXXFLAGS)

libdir = $(ADDON_DIR)/interface

libtext_la_LDFLAGS = $(textldflags)
libtext_la_LIBADD =
libtext_la_SOURCES= $(textsources)
