dnl Process this file with autoconf to produce a configure script.

AC_INIT([alsaplayer],[0.99.82],[alsaplayer-devel@lists.tartarus.org],[alsaplayer],[http://alsaplayer.sourceforge.net/])

AC_CONFIG_SRCDIR([app/Main.cpp])
AC_CANONICAL_TARGET([])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])

dnl Add parameters for aclocal
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

dnl Check for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_USE_SYSTEM_EXTENSIONS

IT_PROG_INTLTOOL(0.50.0)

AC_PROG_LN_S
dnl static libs make no sense for alsaplayer
AC_DISABLE_STATIC([])
LT_INIT
AM_PROG_CC_C_O
AC_PROG_MAKE_SET

dnl ======================
dnl Check for header files
dnl ======================

m4_warn([obsolete],
[The preprocessor macro 'STDC_HEADERS' is obsolete.
  Except in unusual embedded environments, you can safely include all
  C89 headers unconditionally.])dnl
# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP

AC_HEADER_DIRENT
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(malloc.h)
AC_CHECK_HEADERS(sys/ioctl.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(linux/cdrom.h)
AC_CHECK_HEADERS(sys/soundcard.h)
AC_CHECK_HEADERS(sys/audioio.h)
AC_CHECK_HEADERS(audio/audiolib.h)
AC_CHECK_HEADERS(libintl.h)
AC_CHECK_HEADERS(mad.h)
AC_CHECK_HEADERS(id3tag.h)
AC_CHECK_HEADERS(FLAC/stream_decoder.h)

dnl ===============
dnl POSIX.4 threads
dnl ===============

AC_CHECK_LIB(pthread,pthread_create, ,
AC_MSG_ERROR([** POSIX.4 threads not installed or broken **]))

dnl ============================================================
dnl Check for typedefs, structures, and compiler characteristics
dnl ============================================================

AC_C_CONST
AC_TYPE_SIZE_T

dnl ===========================
dnl Check for library functions
dnl ===========================

AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_CHECK_FUNCS(madvise)

dnl =======
dnl DOXYGEN
dnl =======
AC_CHECK_PROG(HAVE_DOXYGEN, doxygen, true, false)
if test $HAVE_DOXYGEN = "false"; then
	AC_MSG_WARN([*** doxygen not found, docs will not be built])
fi

dnl =======================================================
dnl alsaplayer control variables and their default values
dnl this config procedure shamelessly stolen from libmikmod
dnl =======================================================

have_gtk2=no
have_glib=no
have_opengl=no
have_oggvorbis=no
have_flac=no
have_libFLAC_113=no
have_alsa=no
have_oss=no
have_esd=no
have_sparc=no
have_sgi=no
have_nas=no
have_mikmod=no
have_jack=no
have_xosd=no
dynload=no
use_3dnow_asm=no
have_libmad=no
use_libid3tag=no
can_systray=no
have_systray=no

if test "x$x_includes" = "xNONE" ; then
    x_includes=/usr/X11R6/include
fi
if test "x$x_libraries" = "xNONE" ; then
   x_libraries=/usr/X11R6/lib
fi

AM_PATH_LIBMIKMOD(3.1.7, have_mikmod=yes, have_mikmod=no)
AM_PATH_OGG(have_ogg=yes, have_ogg=no)
AM_PATH_VORBIS(have_vorbis=yes, have_vorbis=no)

dnl ======================
dnl PKGCONFIG preparations
dnl ======================

if test -z $PKG_CONFIG_PATH; then
	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
fi

export PKG_CONFIG_PATH


dnl =========
dnl NLS stuff
dnl =========

AC_ARG_ENABLE(nls,
	AS_HELP_STRING([--enable-nls],[Enabled NLS support (default=yes)]),,
	enable_nls=no)

if test "$enable_nls" = yes; then
	echo NLS support use gettext
fi


dnl ======
dnl OpenGL
dnl ======

AC_ARG_ENABLE(opengl,
	AS_HELP_STRING([--enable-opengl],[Enable OpenGL plugins (default=yes)]),,
	enable_opengl=yes)

if test "x$enable_opengl" = xyes; then
	LIBS_save=$LIBS
	OPENGL_LIBS=error
	AC_CHECK_LIB(GL, glBegin, OPENGL_LIBS="-lGL -lpthread")

	if test "x$OPENGL_LIBS" = xerror; then
		AC_CHECK_LIB(MesaGL, glBegin, OPENGL_LIBS="-lMesaGL -lpthread")
	fi

	have_opengl_headers=yes
	AC_CHECK_HEADERS(GL/gl.h GL/glx.h,,have_opengl_headers=no)

	if test "x$OPENGL_LIBS" = xerror || test "x$have_opengl_headers" = xno; then
		have_opengl=no
	else
		have_opengl=yes
	fi

	LIBS=$LIBS_save
else
	AC_MSG_RESULT([*** Disabling OpenGL plugins per user request ***])
	have_opengl=no
fi

AC_SUBST(OPENGL_LIBS)
AM_CONDITIONAL(HAVE_OPENGL, test "x$have_opengl" = xyes)

dnl ================
dnl Some input stuff
dnl ================
if test "$have_glib" = "no" ; then
	have_gtk2=no
fi

if test "$have_ogg" = "yes" ; then
	if test "$have_vorbis" = "yes" ; then
		have_oggvorbis=yes
	else
		have_oggvorbis=no
	fi
else
	have_oggvorbis=no
fi

AC_ARG_ENABLE(mikmod,
	AS_HELP_STRING([--enable-mikmod],[Enable Mikmod plugin (default=yes)]),
	[if test "$enableval" = "no" ; then
		if test "$have_mikmod" = "yes" ; then have_mikmod=no ; fi
	fi])


AC_ARG_ENABLE(oggvorbis,
	AS_HELP_STRING([--enable-oggvorbis],[Enable Ogg Vorbis plugin (default=yes)]),
	[if test "$enableval" = "no" ; then
		if test "$have_oggvorbis" = "yes" ; then have_oggvorbis=no ; fi
	fi])

AM_CONDITIONAL(HAVE_OGGVORBIS,test "$have_oggvorbis" = "yes")

dnl ============
dnl flac library
dnl ============

AC_CHECK_LIB(FLAC, FLAC__stream_decoder_new,
     FLC_LDADD="-lFLAC -lm"; have_flac=yes;
     AC_DEFINE(HAVE_LIBFLC, 1, [Define if you have FLAC library (-lFLAC)]), have_flac=no; FLC_LDADD=, -lm)


AC_ARG_ENABLE(flac,
	AS_HELP_STRING([--enable-flac],[Enable FLAC plugin (default=yes)]),
	[if test "$enableval" = "no" ; then
		if test "$have_flac" = "yes" ; then have_flac=no ; fi
	fi])

dnl ========================
dnl check for libflac>=1.1.3
dnl ========================

AC_CHECK_LIB(FLAC, [FLAC__stream_decoder_init_ogg_stream],
     have_libFLAC_113=yes, have_libFLAC_113=no)

dnl ===========
dnl MAD library
dnl ===========

AC_CHECK_LIB(mad, mad_stream_init,
     MAD_LDADD=-lmad; have_libmad=yes;
     AC_DEFINE(HAVE_LIBMAD, 1, [Define if you have MAD library (-lmad)]), have_libmad=no; MAD_LDADD=, -lmad)

AC_ARG_ENABLE(mad,
	AS_HELP_STRING([--enable-mad],[Enable MAD plugin (default=yes)]),
	[if test "$enableval" = "no" ; then
		if test "$have_libmad" = "yes" ; then have_libmad=no ; fi
	fi])

dnl ==============
dnl id3tab library
dnl ==============

oldcflags=$CFLAGS
CFLAGS="$CFLAGS -lz"

AC_CHECK_LIB(id3tag, id3_file_open,
	FLC_LDADD="$FLC_LDADD -lid3tag -lz"; use_libid3tag=yes;
	AC_DEFINE(HAVE_LIBID3TAG, 1, [Define if you have libid3tag library (-lid3tag)]), use_libid3tag=no)

AC_ARG_ENABLE(id3tag,
	AS_HELP_STRING([--enable-id3tag],[Enable libid3tag support (default=yes)]),
	[if test "$enableval" = "no" ; then
		if test "$use_libid3tag" = "yes" ; then use_libid3tag=no ; fi
	fi])


CFLAGS=$oldcflags

AC_SUBST(FLC_LDADD)
AC_SUBST(MAD_LDADD)

dnl ==========
dnl JACK stuff
dnl ==========
PKG_CHECK_MODULES(JACK, jack >= 0.118.0, have_jack=yes, have_jack=no)
AC_SUBST(JACK_LIBS)
AC_SUBST(JACK_CFLAGS)

dnl ==========
dnl XOSD stuff
dnl nb.: by checking for "xosd_create" instead of "xosd_init", we
dnl      implicitely require xosd 2.x instead of 1.x
dnl      (which is exactly what we want)
dnl ==========
AC_CHECK_LIB(xosd, xosd_create,
	XOSD_LDADD=-lxosd; have_xosd=yes;
	AC_DEFINE(HAVE_LIBXOSD, 1, [Define if you have XOSD library (-lxosd)]), have_xosd=no; XOSD_LDADD=, -lxosd)
AM_CONDITIONAL(HAVE_XOSD, test "x$have_xosd" = xyes)

dnl ==========
dnl glib stuff
dnl ==========

PKG_CHECK_MODULES(GLIB2, glib-2.0, have_glib2=yes, have_glib2=no)
AC_SUBST(GLIB2_LIBS)
AC_SUBST(GLIB2_CFLAGS)

dnl ==========
dnl GTK2 stuff
dnl ==========

PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= 2.8.0 gthread-2.0 >= 2.0.3 pango, have_gtk2=yes, have_gtk2=no)
AC_SUBST(GTK2_LIBS)
AC_SUBST(GTK2_CFLAGS)

dnl ================
dnl GTK2 for systray
dnl ================

PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= 2.10.0 gthread-2.0 >= 2.0.3 pango, can_systray=yes, can_systray=no)

AC_ARG_ENABLE(systray,
	AS_HELP_STRING([--enable-systray],[Enable SysTray icon (default=no)]),,
	enable_systray=no)

if test "x$enable_systray" = xyes ; then
	if test "$can_systray" = "yes" ; then
		have_systray=yes
	else
		have_systray=no
	fi
else
	have_systray=no
fi

AM_CONDITIONAL(HAVE_SYSTRAY,test "x$have_systray" = "xyes")

if test "$enable_systray" = yes; then
    if test "$have_systray" = no; then
    echo Systray support need gtk+>=2.10.n
    else
    echo Experimental systray support will be compiled
    fi
fi

AP_DEFINE_CONDITIONAL(HAVE_SYSTRAY, test "$have_systray" = "yes", [Define if we have the systray icon])

dnl ==================
dnl libsndfile library
dnl ==================

have_sndfile=no

PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.4, have_sndfile=yes, have_sndfile=no)
AC_SUBST(SNDFILE_LIBS)
AC_SUBST(SNDFILE_CFLAGS)
AM_CONDITIONAL(HAVE_SNDFILE, test "x$have_sndfile" = xyes)

dnl ==========
dnl ALSA stuff
dnl ==========

PKG_CHECK_MODULES(ALSA, alsa >= 1.0.4, have_alsa=yes, have_alsa=no)
AC_SUBST(ALSA_LIBS)

# Clean up ALSA_CFLAGS.
# For some insane reason, the ALSA people think its a good idea to provide a
# trailing '/alsa' to their include path, but most people know the only
# sensible way to access the package specific headers is via the namespaced
# path, eg #include <alsa/header.h> for the alsa headers.
#
# Therefore we outright remove '-I/usr/include/alsa' because its completely
# un-necessary  and then with whats left, remove the '/alsa' components.
ALSA_CFLAGS=$(echo $ALSA_CFLAGS | sed 's|-I/usr/include/alsa||g' | sed 's|/alsa||g')
AC_SUBST(ALSA_CFLAGS)
AM_CONDITIONAL(HAVE_ALSA, test "x$have_alsa" = xyes)

dnl =========
dnl ESD stuff
dnl =========

AM_PATH_ESD(0.2.4, have_esd=yes, have_esd=no)

dnl ===========
dnl SPARC stuff
dnl ===========

if test "$ac_cv_header_sys_audioio_h" = "yes" ; then
	have_sparc=yes
else
	have_sparc=no
fi


dnl =========
dnl OSS stuff
dnl =========

if test "$ac_cv_header_sys_soundcard_h" = "yes" ; then
	have_oss=yes
else
	have_oss=no
fi

dnl =========
dnl SGI stuff
dnl =========

AC_CHECK_LIB(audio, alOpenPort,
[have_sgi=yes
SGI_LIBS=-laudio],
have_sgi=no)

AC_SUBST(SGI_LIBS)

dnl =========
dnl NAS stuff
dnl =========

oldcflags=$CFLAGS
CFLAGS="$CFLAGS -I$x_includes -L$x_libraries -lXt"
AC_CHECK_LIB(audio,AuStartFlow, [have_nas=yes NAS_LIB="-L$x_libraries -laudio -lXt"], have_nas=no)
CFLAGS=$oldcflags
AC_SUBST(NAS_LIB)


dnl =====================================================
dnl Sound output stuff. This will go away once everything
dnl is really modular
dnl =====================================================

AC_ARG_ENABLE(jack,
	AS_HELP_STRING([--enable-jack],[Enable jack support (default=yes)]),
	[if test "$enableval" = "no" ; then have_jack=no ; fi])

AC_ARG_ENABLE(alsa,
	AS_HELP_STRING([--enable-alsa],[Enable ALSA (default=yes)]),
	[if test "$enableval" = "no" ; then have_alsa=no ; fi])

AC_ARG_ENABLE(esd,
	AS_HELP_STRING([--enable-esd],[Enable esd driver (default=yes)]),
	[if test "$enableval" = "no" ; then have_esd=no ; fi])

AC_ARG_ENABLE(sparc,
	AS_HELP_STRING([--enable-sparc],[Enable SPARC driver (default=yes)]),
	[if test "$enableval" = "no" ; then have_sparc=no ; fi])

AC_ARG_ENABLE(oss,
	AS_HELP_STRING([--enable-oss],[Enable OSS driver (default=yes)]),
	[if test "$enableval" = "no" ; then have_oss=no ; fi])

AC_ARG_ENABLE(sgi,
	AS_HELP_STRING([--enable-sgi],[Enable SGI driver (default=yes)]),
	[if test "$enableval" = "no" ; then have_sgi=no ; fi])

AC_ARG_ENABLE(gtk2,
	AS_HELP_STRING([--enable-gtk2],[Enable GTK2 graphical interface (default=yes)]),
	[if test "$enableval" = "no" ; then have_gtk2=no ; fi])

AC_ARG_ENABLE(nas,
	AS_HELP_STRING([--enable-nas],[Enable NAS driver (default=yes)]),
	[if test "$enableval" = "no" ; then have_nas=no ; fi])

if test "x$ac_cv_header_mad_h" = "xno" ; then
	have_libmad=no
fi

if test "x$ac_cv_header_id3tag_h" = "xno" ; then
	use_libid3tag=no
fi


AM_CONDITIONAL(HAVE_DOXYGEN, $HAVE_DOXYGEN)
AM_CONDITIONAL(HAVE_LIBMAD,test "x$have_libmad" = xyes)
AM_CONDITIONAL(USE_LIBID3TAG, test "x$use_libid3tag" = xyes)
AM_CONDITIONAL(HAVE_FLC, test "x$have_flac" = xyes)
AM_CONDITIONAL(HAVE_JACK,	test "x$have_jack" = xyes)
AM_CONDITIONAL(HAVE_ALSA, test "x$have_alsa" = xyes)
AM_CONDITIONAL(HAVE_OSS, test "x$have_oss" = xyes)
AM_CONDITIONAL(HAVE_ESD, test "x$have_esd" = xyes)
AM_CONDITIONAL(HAVE_SPARC, test "x$have_sparc" = xyes)
AM_CONDITIONAL(HAVE_SGI, test "x$have_sgi" = xyes)
AM_CONDITIONAL(HAVE_NAS, test "x$have_nas" = xyes)
AM_CONDITIONAL(HAVE_LINUX_CDROM,test "x$ac_cv_header_linux_cdrom_h" = xyes)
AM_CONDITIONAL(HAVE_GLIB2, test "x$have_glib2" = xyes)
AM_CONDITIONAL(HAVE_GTK2, test "x$have_gtk2" = xyes)
AM_CONDITIONAL(HAVE_MIKMOD,test "x$have_mikmod" = xyes)
AM_CONDITIONAL(HAVE_SYSTRAY,test "x$have_systray" = xyes)


dnl ============
dnl Machine type
dnl ============

AC_MSG_CHECKING(machine architecture)
AC_SUBST(ARCH_TYPE)
ARCH_TYPE=`uname -m`
AC_MSG_RESULT($ARCH_TYPE)

AM_CONDITIONAL(ARCH_X86,test "$ARCH_TYPE" = "i486" || test "$ARCH_TYPE" = "i586" || test "$ARCH_TYPE" = "i686" )
AM_CONDITIONAL(ARCH_486,test "$ARCH_TYPE" = "i486")
AM_CONDITIONAL(ARCH_586,test "$ARCH_TYPE" = "i586")
AM_CONDITIONAL(ARCH_686,test "$ARCH_TYPE" = "i686")
AM_CONDITIONAL(ARCH_PPC,test "$ARCH_TYPE" = "ppc")
AM_CONDITIONAL(ARCH_ULTRA,test "$ARCH_TYPE" = "sun4u")
dnl cruft to satisfy gtk2s Makefile. Leave it in for now
AM_CONDITIONAL(BUILD_EXTERNAL, test "a" = "b")

test_cflags="-O2 -funroll-loops -finline-functions -ffast-math -Wall -Wextra -Wshadow"
for f in $test_cflags ; do
	AP_ADD_CFLAGS($f)
	done
AP_ADD_CFLAGS([-Wunused-variable])
AP_ADD_CFLAGS([-Wmissing-prototypes])
AP_ADD_CFLAGS([-Wstrict-prototypes])

AC_LANG_PUSH([C++])
for f in $test_cflags ; do
	AP_ADD_CXXFLAGS($f)
	done
AC_LANG_POP

AP_DEFINE_CONDITIONAL(HAVE_GLIB2, test "$have_glib2" = "yes", [Define if we have glib2])
AP_DEFINE_CONDITIONAL(HAVE_GTK2, test "$have_gtk2" = "yes", [Define if we have gtk2])

dnl ===============
dnl Debugging stuff
dnl ===============

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug],[Enable debugging (default=no)]),,
	enable_debug=no)

if test "$enable_debug" = yes; then
	DFLAGS="$DFLAGS -g"
	AP_ADD_CFLAGS(-fstack-protector)
	AC_LANG_PUSH([C++])
	AP_ADD_CXXFLAGS(-fstack-protector)
	AC_LANG_POP
fi


AC_ARG_ENABLE(profiling,
	AS_HELP_STRING([--enable-profiling],[Enable profiling (-pg)]),,
	enable_profiling=no)

if test "$enable_profiling" = yes; then
	DFLAGS="$DFLAGS -pg"
fi

AC_ARG_ENABLE(werror,
	AS_HELP_STRING([--enable-werror],[enable -Werror in all Makefiles]))

if test x$enable_werror = "xyes" ; then
	AP_ADD_CFLAGS([-Werror])
	AC_LANG_PUSH([C++])
	AP_ADD_CXXFLAGS([-Werror])
	AC_LANG_POP
	fi

dnl ================================================================
dnl Special assembly options. Note: Need to fix the above arch code!
dnl ================================================================

AC_SUBST(DFLAGS)

ADDON_DIR=${libdir}/$PACKAGE


dnl ===============
dnl GetText Package
dnl ===============

GETTEXT_PACKAGE=alsaplayer
AC_SUBST(GETTEXT_PACKAGE)

dnl ================
dnl Create Makefiles
dnl ================

AC_SUBST(ALSA_LIB)
AC_SUBST(ADDON_DIR)
AC_SUBST(AS)
AC_SUBST(ASFLAGS)

AC_CONFIG_FILES([ po/Makefile.in
Makefile
alsaplayer.1
alsaplayer.pc
alsaplayer.spec
app/Makefile
interface/Makefile
interface/gtk2/Makefile
interface/gtk2/pixmaps/Makefile
interface/text/Makefile
interface/daemon/Makefile
interface/xosd/Makefile
scopes2/Makefile
scopes2/monoscope/Makefile
scopes2/levelmeter/Makefile
scopes2/spacescope/Makefile
scopes2/logbarfft/Makefile
scopes2/synaescope/Makefile
scopes2/blurscope/Makefile
scopes2/opengl_spectrum/Makefile
output/Makefile
output/alsa/Makefile
output/jack/Makefile
output/oss/Makefile
output/esound/Makefile
output/sgi/Makefile
output/sparc/Makefile
output/null/Makefile
output/nas/Makefile
input/Makefile
input/mad/Makefile
input/cdda/Makefile
input/mikmod/Makefile
input/vorbis/Makefile
input/flac/Makefile
input/sndfile/Makefile
reader/Makefile
reader/file/Makefile
reader/http/Makefile
alsaplayer/Makefile
libalsaplayer/Makefile
extra/Makefile
examples/Makefile
m4/Makefile
docs/Makefile
docs/reference.doxygen
])
AC_OUTPUT

echo "
Configuration:

  Install path:                       ${prefix}
  Use libmad shared library:          ${have_libmad}
  Enable libid3tag support:           ${use_libid3tag}
  Build GTK2 interface:               ${have_gtk2}
  Build XOSD interface:               ${have_xosd}
  Build OpenGL based visual plugins:  ${have_opengl}
  Build alsa output plugin:           ${have_alsa}
  Build jack output plugin:           ${have_jack}
  Build oss output plugin:            ${have_oss}
  Build esound output plugin:         ${have_esd}
  Build sparc output plugin:          ${have_sparc}
  Build sgi output plugin:            ${have_sgi}
  Build nas output plugin:            ${have_nas}
  Build vorbis input plugin:          ${have_oggvorbis}
  Build cdda input plugin:            ${ac_cv_header_linux_cdrom_h}
  Build mikmod input plugin:          ${have_mikmod}
  Build FLAC input plugin:            ${have_flac}
  Build sndfile input plugin:         ${have_sndfile}
  Build systray icon:                 ${have_systray}"
echo

if test $have_jack = "no" -a $have_sparc = "no" -a $have_alsa = "no" -a $have_oss = "no" -a $have_esd = "no" -a $have_sgi = "no" -a $have_nas = "no" ; then
echo "
**WARNING**: NO OUTPUT MODULES selected. AlsaPlayer won't be
**WARNING**: able to produce sound output unless you manually
**WARNING**: add output modules to:
**WARNING**: ${prefix}/alsaplayer/output/"
echo
fi

echo Now type \'make\' to build AlsaPlayer.
