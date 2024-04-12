dnl qt.m4
dnl Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

dnl AM_PATH_QT([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN([AM_PATH_QT],
[
AS_MESSAGE([checking for Qt 3.0, built with -thread ......])

AC_LANG_SAVE
AC_LANG([C++])

saved_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
saved_LIBRARY_PATH="$LIBRARY_PATH"
saved_CXXFLAGS="$CXXFLAGS"
saved_LDFLAGS="$LDFLAGS"
saved_LIBS="$LIBS"

AC_ARG_WITH(
  qt-libdir,
  [  --with-qt-libdir=PFX   Prefix where Qt library is installed (optional)],
  qt_libdir="$withval",
  qt_libdir=""
)

AC_ARG_WITH(
  qt-incdir,
  [  --with-qt-incdir=PFX   Prefix where Qt includes are installed (optional)],
  qt_incdir="$withval",
  qt_incdir=""
)

AC_ARG_WITH(
  qt-bindir,
  [  --with-qt-bindir=PFX   Prefix where moc and uic are installed (optional)],
  qt_bindir="$withval",
  qt_bindir=""
)

AC_MSG_CHECKING([include path])

dnl If we weren't given qt_incdir, have a guess.

if test "x$qt_incdir" != "x"
then
  AC_MSG_RESULT([specified as $qt_incdir])
else

  GUESS_QT_INC_DIRS="$QTDIR/include /usr/include /usr/include/qt /usr/include/qt2 /usr/local/include /usr/local/include/qt /usr/local/include/qt2 /usr/X11R6/include/ /usr/X11R6/include/qt /usr/X11R6/include/X11/qt /usr/X11R6/include/qt2 /usr/X11R6/include/X11/qt2"

  for dir in $GUESS_QT_INC_DIRS
  do
    if test -e $dir/qt.h
    then
      qt_incdir=$dir
      AC_MSG_RESULT([assuming $dir])
      break
    fi
  done

fi

dnl If we weren't given qt_libdir, have a guess.

AC_MSG_CHECKING([library path])

if test "x$qt_libdir" != "x"
then
  AC_MSG_RESULT([specified as $qt_libdir])
else

  GUESS_QT_LIB_DIRS="$QTDIR/lib /usr/lib /usr/local/lib /usr/X11R6/lib /usr/local/qt/lib"

  for dir in $GUESS_QT_LIB_DIRS
  do
    if test -e $dir/libqt-mt.so
    then
      qt_libdir=$dir
      AC_MSG_RESULT([assuming $dir/libqt-mt.so])
      break
    fi
  done
fi

dnl If we weren't given qt_bindir, have a guess.

AC_MSG_CHECKING([binary directory])

if test "x$qt_bindir" != "x"
then
  AC_MSG_RESULT([specified as $qt_bindir])
else

  GUESS_QT_BIN_DIRS="$QTDIR/bin /usr/bin /usr/local/bin /usr/local/bin/qt2 /usr/X11R6/bin"

  for dir in $GUESS_QT_BIN_DIRS
  do
    if test -x $dir/moc -a -x $dir/uic
    then
      qt_bindir=$dir
      AC_MSG_RESULT([assuming $dir])
      break
    fi
  done
fi

dnl ifelse is talked about in m4 docs

if test "x$qt_incdir" = "x"
then
  AC_MSG_ERROR([Can't find includes])
  ifelse([$2], , :, [$2])
elif test "x$qt_libdir" = "x"
then
  AC_MSG_ERROR([Can't find library])
  ifelse([$2], , :, [$2])
elif test "x$qt_bindir" = "x"
then
  AC_MSG_ERROR([Can't find moc and/or uic])
  ifelse([$2], , :, [$2])
else
  AC_MSG_CHECKING([if a Qt program links])

  found_qt="no"

  LDFLAGS="$LDFLAGS -L$qt_libdir"
  LIBS="$LIBS -lqt-mt"
  CXXFLAGS="$CXXFLAGS -I$qt_incdir"
  LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$qt_libdir"

  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <qstring.h>
  ]], [[
#if QT_VERSION < 300
#error Qt version too old
#endif
  QString s("Hello, world!");
  qDebug(s.latin1());
  ]])],[found_qt="yes"
  AC_MSG_RESULT(ok)],[AC_MSG_ERROR(failed - check config.log for details)
  ])

  if test "x$found_qt" = "xyes"
  then
    QT_CXXFLAGS="-I$qt_incdir"
    QT_LIBS="-L$qt_libdir -lqt-mt"
    MOC="$qt_bindir/moc"
    UIC="$qt_bindir/uic"
    ifelse([$1], , :, [$1])
  else
    ifelse([$2], , :, [$2])
  fi

fi

AC_SUBST(QT_CXXFLAGS)
AC_SUBST(QT_LIBS)
AC_SUBST(MOC)
AC_SUBST(UIC)

AC_LANG_RESTORE()

LD_LIBRARY_PATH="$saved_LD_LIBRARY_PATH"
LIBRARY_PATH="$saved_LIBRARY_PATH"
CXXFLAGS="$saved_CXXFLAGS"
LDFLAGS="$saved_LDFLAGS"
LIBS="$saved_LIBS"
])
