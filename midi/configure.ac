dnl Process this file with autoconf to produce a configure script.

AC_INIT(midi_engine.c)

MIDI_MAJOR_VERSION=0
MIDI_MINOR_VERSION=1
MIDI_MICRO_VERSION=1

BETA=

MIDI_VERSION=$MIDI_MAJOR_VERSION.$MIDI_MINOR_VERSION.${MIDI_MICRO_VERSION}${BETA}

AM_INIT_AUTOMAKE(midi_in, ${MIDI_VERSION})
AM_MAINTAINER_MODE

if test "x$x_includes" = "xNONE"
then
    x_includes=/usr/X11R6/include
fi
if test "x$x_libraries" = "xNONE"
then
   x_libraries=/usr/X11R6/lib
fi

dnl Check for programs.

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AM_DISABLE_STATIC
AM_PROG_LIBTOOL
AC_PROG_MAKE_SET

dnl ======================
dnl Check for header files
dnl ======================

AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(malloc.h)
AC_CHECK_HEADERS(sys/ioctl.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(unistd.h)


dnl ===========================
dnl Check for library functions
dnl ===========================

AC_PROG_GCC_TRADITIONAL
AC_FUNC_MMAP
AC_FUNC_MALLOC
AC_CHECK_FUNCS(strdup)



dnl ===============
dnl Debugging stuff
dnl ===============

AC_ARG_ENABLE(debug, 
[  --enable-debug          Enable debugging (default=no)],, enable_debug=no)

if test "$enable_debug" = yes; then
DFLAGS="$DFLAGS -g"
fi


AC_ARG_ENABLE(profiling,
[   --enable-profiling     Enable profiling (-pg)],, enable_profiling=no)

if test "$enable_profiling" = yes; then
DFLAGS="$DFLAGS -pg"
fi


AC_SUBST(DFLAGS)
AM_CONDITIONAL(OPTIMIZE,test "$enable_debug" = "no")

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

AC_MSG_CHECKING(for gcc optimization flags)
old_cflags=$CFLAGS
CFLAGS="-O2 -fexpensive-optimizations -funroll-loops -finline-functions -fomit-frame-pointer -ffast-math"
AC_TRY_COMPILE([#include <stdio.h>],
[ printf ("hello, world"); ],
[ EXTRA_OPT_CFLAGS="-O2 -fexpensive-optimizations -funroll-loops -finline-functions -fomit-frame-pointer -ffast-math"],[ EXTRA_OPT_CFLAGS=""])
CFLAGS=$old_cflags
AC_MSG_RESULT([$EXTRA_OPT_CFLAGS])

ARCH_CFLAGS="$EXTRA_OPT_CFLAGS"

if test "$ARCH_TYPE" = "i486" || test "$ARCH_TYPE" = "i586" || test "$ARCH_TYPE" = "i686"
then
	AC_DEFINE(ARCH_X86, 1, [Define if on x86 platform])
fi

if test "$ARCH_TYPE" = "i486"
then
        AC_DEFINE(ARCH_486, 1, [Define if on a 486 platform])
fi

if test "$ARCH_TYPE" = "i586"
then
        AC_DEFINE(ARCH_586, 1, [Define if on a 586 platform])
fi

if test "$ARCH_TYPE" = "i686"
then
        AC_DEFINE(ARCH_686, 1, [Define if on a 686 platform])
#		EXTRA_CPU_CFLAGS=$(if $CC -march=i686 -S -o /dev/null -xc /dev/null > /dev/null; then echo "-march=i686"; fi)
fi

if test "$ARCH_TYPE" = "ppc"
then
		AC_DEFINE(ARCH_PPC, 1, [Define if on a PPC platform])
fi

if test "$ARCH_TYPE" = "sun4u"
then
		AC_DEFINE(ARCH_ULTRA, 1, [Define if on a SUN ULTRA platform])
fi


dnl ================================================================
dnl Special assembly options. Note: Need to fix the above arch code!
dnl ================================================================

case "$host" in
	i?86-* | k?-* | athlon-*) dnl catch i386,i486,i586,i686,k6,k7
		AC_DEFINE(FPM_INTEL, 1, [Define if we are on Intel])
	;;	
	*-*)
		echo "host_alias = $host_alias"
		AC_DEFINE(FPM_64BIT, 1, [Use this otherwise])
	;;	
esac

ARCH_CFLAGS="$ARCH_CFLAGS $EXTRA_CPU_CFLAGS"

AC_SUBST(ARCH_CFLAGS)




dnl pkg-config stuff

if test -z $PKG_CONFIG_PATH; then
	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
fi

AC_ARG_WITH(alsaplayer,
	[  --with-alsaplayer     Location of alsaplayer build dir (will prepend to PKG_CONFIG_PATH)],
	[PKG_CONFIG_PATH="${withval}:${PKG_CONFIG_PATH}"],)	

export PKG_CONFIG_PATH

TIMIDITY_DIR=${datadir}/timidity

AC_ARG_WITH(timidity,
	[  --with-timidity       Location of timidity patch dir (DATADIR/timidity)],
	[TIMIDITY_DIR="$withval"],)	

export TIMIDITY_DIR

PKG_CHECK_MODULES(ALSAPLAYER, alsaplayer)

PLUGIN_DIR=`pkg-config alsaplayer --variable=plugindir`
INTERFACEPLUGIN_DIR=`pkg-config alsaplayer --variable=interfaceplugindir`
INPUTPLUGIN_DIR=`pkg-config alsaplayer --variable=inputplugindir`
OUTPUTPLUGIN_DIR=`pkg-config alsaplayer --variable=outputplugindir`
SCOPEPLUGIN_DIR=`pkg-config alsaplayer --variable=scopeplugindir`


AC_SUBST(TIMIDITY_DIR)
AC_SUBST(ALSAPLAYER_LIBS)
AC_SUBST(ALSAPLAYER_CFLAGS)
AC_SUBST(PLUGIN_DIR)
AC_SUBST(INTERFACEPLUGIN_DIR)
AC_SUBST(INPUTPLUGIN_DIR)
AC_SUBST(OUTPUTPLUGIN_DIR)
AC_SUBST(SCOPEPLUGIN_DIR)

AM_CONFIG_HEADER(config.h)

AC_SUBST(MIDI_MAJOR_VERSION)
AC_SUBST(MIDI_MINOR_VERSION)
AC_SUBST(MIDI_MICRO_VERSION)
AC_SUBST(MIDI_VERSION)

AC_OUTPUT([
Makefile
])
echo
echo Now type \'make\' to build the MIDI input plugin.
echo
