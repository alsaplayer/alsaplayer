if HAVE_OSS

ossltlibs = liboss.la
ossldflags = -export-dynamic -avoid-version
osssources = oss.c

else

osslitlibs =
ossldflags =
osssources =

endif

lib_LTLIBRARIES = $(ossltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include @ARCH_CFLAGS@ 

libdir = $(ADDON_DIR)/output

liboss_la_LDFLAGS = $(ossldflags)
LIBS =

liboss_la_SOURCES= $(osssources)
