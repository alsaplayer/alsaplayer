if HAVE_MIKMOD

modltlibs = libmod.la
modldflags = -export-dynamic -avoid-version
modsources = mikmod_engine.c drv_alsaplayer.c
modcflags = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
		@ARCH_CFLAGS@ -D_REENTRANT @LIBMIKMOD_CFLAGS@ @DFLAGS@
modldadd = @LIBMIKMOD_LDADD@

lib_LTLIBRARIES = $(modltlibs)

else

modltlibs = 
modldflags = 
modsources = 
modcflags =
modldadd = 

endif

libdir = $(ADDON_DIR)/input

libmod_la_LDFLAGS = $(modldflags)
LIBS = @LIBMIKMOD_LIBS@
AM_CXXFLAGS = $(modcflags)
AM_CFLAGS = $(modcflags)
libmod_la_LIBADD = $(modldadd)

libmod_la_SOURCES = 	$(modsources)
