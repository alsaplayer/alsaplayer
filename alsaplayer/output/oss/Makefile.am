if HAVE_OSS

oss_outltlibs = liboss_out.la
oss_outldflags = -export-dynamic -avoid-version
oss_outsources = oss.c

else

oss_outlitlibs =
oss_outldflags =
oss_outsources =

endif

lib_LTLIBRARIES = $(oss_outltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer @ARCH_CFLAGS@ 

libdir = $(ADDON_DIR)/output

liboss_out_la_LDFLAGS = $(oss_outldflags)
LIBS =

liboss_out_la_SOURCES= $(oss_outsources)
