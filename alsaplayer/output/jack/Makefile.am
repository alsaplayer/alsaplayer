if HAVE_JACK

jack_outltlibs = libjack_out.la
jack_outldflags = -export-dynamic -avoid-version
jack_outsources = jack.cpp

JACK_FLAGS = @GLIB_CFLAGS@
JACK_LIBS = @JACK_LIB@ @GLIB_LIBS@

else

jack_outlitlibs =
jack_outldflags =
jack_outsources =

endif

EXTRA_DIST = jack.cpp

lib_LTLIBRARIES = $(jack_outltlibs)

COMMON_CFLAGS = -D_REENTRANT -I$(top_builddir) \
	-I$(top_srcdir)/alsaplayer \
	$(JACK_FLAGS)
	@ARCH_CFLAGS@ 

CXXFLAGS = $(COMMON_CFLAGS) $(EXTRA_CFLAGS)

AM_CFLAGS = $(CXXFLAGS)

libdir = $(ADDON_DIR)/output

libjack_out_la_LDFLAGS = $(jack_outldflags)

LIBS = $(JACK_LIBS)

libjack_out_la_SOURCES= $(jack_outsources)
