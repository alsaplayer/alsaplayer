EXTRA_DIST = alsa.c

if HAVE_ALSA_05X

alsaltlibs = libalsa.la
alsaldflags = -export-dynamic -avoid-version
alsasources = alsa.c

else

alsaltlibs =
alsaldflags =
alsasources =

endif

EXTRA_DIST= alsa.c

lib_LTLIBRARIES = $(alsaltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/include \
			@ARCH_CFLAGS@	

libdir = $(ADDON_DIR)/output

libalsa_la_LDFLAGS = $(alsaldflags)
LIBS = @ALSA_LIB@

libalsa_la_SOURCES= $(alsasources)
