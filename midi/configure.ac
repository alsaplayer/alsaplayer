dnl Process this file with autoconf to produce a configure script.

AC_INIT(midi_engine.c)

MIDI_MAJOR_VERSION=0
MIDI_MINOR_VERSION=1
MIDI_MICRO_VERSION=0

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
AC_HEADER_STDC

dnl pkg-config stuff

if test -z $PKG_CONFIG_PATH; then
	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
fi

AC_ARG_WITH(alsaplayer,
	[  --with-alsaplayer     Location of alsaplayer build dir (will prepend to PKG_CONFIG_PATH)],
	[PKG_CONFIG_PATH="${withval}:${PKG_CONFIG_PATH}"],)	

export PKG_CONFIG_PATH

PKG_CHECK_MODULES(ALSAPLAYER, alsaplayer)

PLUGIN_DIR=`pkg-config alsaplayer --variable=plugindir`
INTERFACEPLUGIN_DIR=`pkg-config alsaplayer --variable=interfaceplugindir`
INPUTPLUGIN_DIR=`pkg-config alsaplayer --variable=inputplugindir`
OUTPUTPLUGIN_DIR=`pkg-config alsaplayer --variable=outputplugindir`
SCOPEPLUGIN_DIR=`pkg-config alsaplayer --variable=scopeplugindir`

TIMIDITY_DIR="/usr/local/share/timidy"

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
