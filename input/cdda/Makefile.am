if HAVE_LINUX_CDROM

cddaltlibs = libcdda.la
cddaldflags = -export-dynamic -avoid-version
cddasources = cdda_engine.c cdda.h

else

cddaltlibs =
cddaldflags =
cddasources =

endif

lib_LTLIBRARIES = $(cddaltlibs)

AM_CFLAGS = @DFLAGS@ -D_REENTRANT

INCLUDES = -I$(top_builddir) -I$(top_srcdir)/libalsaplayer \
				-I$(top_srcdir)/alsaplayer

libdir = $(ADDON_DIR)/input

libcdda_la_SOURCES= $(cddasources)
libcdda_la_LDFLAGS = $(cddaldflags)
