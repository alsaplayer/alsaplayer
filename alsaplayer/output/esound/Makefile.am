EXTRA_DIST = esound.c

if HAVE_ESD

esound_outltlibs = libesound_out.la
esound_outldflags = -export-dynamic -avoid-version
esound_outsources = esound.c

else

esound_outltlibs =
esound_outldflags =
esound_outsources =

endif


lib_LTLIBRARIES = $(esound_outltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @ESD_CFLAGS@

libdir = $(ADDON_DIR)/output

libesound_out_la_LDFLAGS = $(esound_outldflags)
LIBS = @ESD_LIBS@

libesound_out_la_SOURCES= $(esound_outsources)
