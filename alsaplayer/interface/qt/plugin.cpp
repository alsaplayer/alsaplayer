/*
 *  qt.cpp (C) 2001 by Rik Hemsley (rikkus) <rik@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/ 

#include <qapplication.h>
#include <qplatinumstyle.h>

#include "CorePlayer.h"
#include "Playlist.h"
#include "MainWindow.h"
#include "interface_plugin.h"

int interface_qt_init()
{
	return 1;
}


int interface_qt_running()
{
	return 1;
}


int interface_qt_stop()
{
	return 1;
}

void interface_qt_close()
{
  qApp->quit();
}

int interface_qt_start
(
 CorePlayer * coreplayer,
 Playlist   * playlist,
 int          argc,
 char      ** argv
)
{
  bool customStyle = false;

  for (int i = 0; i < argc; i++)
  {
    if (QString(argv[i]) == QString("-style"))
    {
      customStyle = true;
      break;
    }
  }

  QApplication app(argc, argv);

  if (!customStyle)
    app.setStyle(new QPlatinumStyle);

  MainWindow w(coreplayer, playlist);

  app.setMainWidget(&w);

  w.show();

  return app.exec();
}


interface_plugin default_plugin =
{
	INTERFACE_PLUGIN_VERSION,
	{ "Qt interface v1.0" },
	{ "Rik Hemsley" },
	NULL,
	interface_qt_init,
	interface_qt_start,
	interface_qt_running,
	interface_qt_stop,
	interface_qt_close
};

extern "C"
{
  interface_plugin * interface_plugin_info()
  {
    return &default_plugin;
  }
}
