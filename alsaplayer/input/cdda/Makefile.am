if HAVE_LINUX_CDROM

cddaltlibs = libcdda.la
cddaldflags = -export-dynamic -avoid-version
cddasources = cdda_engine.c

else

cddaltlibs =
cddaldflags =
cddasources =

endif

lib_LTLIBRARIES = $(cddaltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			@ARCH_CFLAGS@

libdir = $(ADDON_DIR)/input

libcdda_la_LDFLAGS = $(cddaldflags)
LIBS =

libcdda_la_SOURCES= $(cddasources)
