SOURCE_FILES = jack.cpp

if HAVE_JACK

jack_outltlibs = libjack_out.la
jack_outldflags = -export-dynamic -avoid-version
jack_outsources = $(SOURCE_FILES)

else

jack_outlitlibs =
jack_outldflags =
jack_outsources =

endif

EXTRA_DIST = $(SOURCE_FILES)

lib_LTLIBRARIES = $(jack_outltlibs)

COMMON_FLAGS = -D_REENTRANT -I$(top_builddir) \
	-I$(top_srcdir)/alsaplayer \
	@ARCH_CFLAGS@ 

AM_CXXFLAGS = $(COMMON_FLAGS) $(JACK_CFLAGS)

AM_CFLAGS = $(AM_CXXFLAGS)

libdir = $(ADDON_DIR)/output

libjack_out_la_LDFLAGS = $(jack_outldflags)
libjack_out_la_LIBADD = @JACK_LIBS@
libjack_out_la_SOURCES= $(jack_outsources)
