if HAVE_LINUX_CDROM

cddaltlibs = libcdda.la
cddaldflags = -export-dynamic -avoid-version
cddasources = cdda_engine.cpp

else

cddaltlibs =
cddaldflags =
cddasources =

endif

lib_LTLIBRARIES = $(cddaltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			@ARCH_CFLAGS@

CXXFLAGS = $(CFLAGS)

# autoconf/automake BUG!!
# LINK = $(CXXLINK)

libdir = $(ADDON_DIR)/input

libcdda_la_LDFLAGS = $(cddaldflags)
libcdda_la_SOURCES= $(cddasources)
