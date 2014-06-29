if HAVE_ALSA

alsa_outltlibs = libalsa_out.la
alsa_outldflags = -export-dynamic -avoid-version
alsa_outsources = alsa.c

else

alsa_outltlibs =
alsa_outldflags =
alsa_outsources =

endif

EXTRA_DIST= alsa.c

lib_LTLIBRARIES = $(alsa_outltlibs)

AM_CFLAGS = @ALSA_CFLAGS@ @DFLAGS@ -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer

libdir = $(ADDON_DIR)/output

libalsa_out_la_LDFLAGS = $(alsa_outldflags)
LIBS = @ALSA_LIBS@

libalsa_out_la_SOURCES= $(alsa_outsources)
