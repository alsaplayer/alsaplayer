EXTRA_DIST = esound.c

if HAVE_ESD

esoundltlibs = libesound.la
esoundldflags = -export-dynamic -avoid-version
esoundsources = esound.c

else

esoundltlibs =
esoundldflags =
esoundsources =

endif


lib_LTLIBRARIES = $(esoundltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			@ARCH_CFLAGS@ @ESD_CFLAGS@

libdir = $(ADDON_DIR)/output

libesound_la_LDFLAGS = $(esoundldflags)
LIBS = @ESD_LIBS@

libesound_la_SOURCES= $(esoundsources)
