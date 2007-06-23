meshltlibs = libmesh.la
meshldflags = -export-dynamic -avoid-version @GTK_LIBS@
meshsources = mesh.c trackball.c trackball.h

lib_LTLIBRARIES = $(meshltlibs)

AM_CFLAGS = -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
			@ARCH_CFLAGS@ @GTK_CFLAGS@ -lgtkgl -lGL -lGLU @DFLAGS@

libdir = $(ADDON_DIR)/scopes

libmesh_la_LDFLAGS = $(meshldflags)
LIBS =

libmesh_la_SOURCES= $(meshsources)
