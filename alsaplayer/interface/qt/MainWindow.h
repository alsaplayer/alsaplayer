/*
 *   qt_interface.h (C) 2001 by Rik Hemsley (rikkus) <rik@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef QT_MAIN_WINDOW_H
#define QT_MAIN_WINDOW_H

#include "MainWindowBase.h"

class CorePlayer;
class Playlist;

class QPopupMenu;

class PlayListDialog;

class MainWindow : public MainWindowBase
{
  Q_OBJECT

  public:

    MainWindow(CorePlayer *, Playlist *);
    ~MainWindow();

  protected slots:

    void slotPrevious();
    void slotPlay();
    void slotNext();
    void slotStop();
    void slotPlayList();
    void slotPause();
    void slotBack();
    void slotForward();

    void slotSpeedSliderMoved(int);
    void slotPositionSliderPressed();
    void slotPositionSliderReleased();

    void slotBalanceSliderMoved(int);
    void slotVolumeSliderMoved(int);

    void slotScopes();
    void slotFX();
    void slotAbout();
    void slotCDDA();

  protected:

    void timerEvent(QTimerEvent *);
    void closeEvent(QCloseEvent *);

  private:

    CorePlayer      * player_;
    Playlist        * playList_;
    bool              updatePositionSlider_;
    QPopupMenu      * popup_;
    PlayListDialog  * playListDialog_;
};

#endif // QT_MAIN_WINDOW_H
// vim:ts=2:sw=2:tw=78:et
