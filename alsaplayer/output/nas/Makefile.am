if HAVE_NAS

nas_outltlibs = libnas_out.la
nas_outldflags = -export-dynamic -avoid-version
nas_outsources = nas.c

else

nas_outltlibs =
nas_outldflags =
nas_outsources =

endif

EXTRA_DIST = nas.c

lib_LTLIBRARIES = $(nas_outltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@	

libdir = $(ADDON_DIR)/output

libnas_out_la_LDFLAGS = $(nas_outldflags)
LIBS = @NAS_LIB@

libnas_out_la_SOURCES= $(nas_outsources)
