if HAVE_NAS

nasltlibs = libnas.la
nasldflags = -export-dynamic -avoid-version
nassources = nas.c

else

nasltlibs =
nasldflags =
nassources =

endif

EXTRA_DIST = nas.c

lib_LTLIBRARIES = $(nasltlibs)

CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@	

libdir = $(ADDON_DIR)/output

libnas_la_LDFLAGS = $(nasldflags)
LIBS = @NAS_LIB@

libnas_la_SOURCES= $(nassources)
