/*
 *   playlist_dialog.h (C) 2001 by Rik Hemsley (rikkus) <rik@kde.org>
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

#ifndef QT_PLAY_LIST_DIALOG_H
#define QT_PLAY_LIST_DIALOG_H

#include "PlayListDialogBase.h"

#include "Playlist.h"

class PlayListDialog
  :         public PlayListDialogBase,
    virtual public PlaylistInterface
{
  Q_OBJECT

  public:

    PlayListDialog(QWidget * parent, Playlist *);
    ~PlayListDialog();

    void CbSetCurrent(unsigned);
    void CbInsert(std::vector<PlayItem> &, unsigned);
    void CbRemove(unsigned, unsigned);
    void CbLock();
    void CbUnlock();
    void CbClear();

  protected slots:

    void slotAdd();
    void slotLoad();
    void slotRemove();
    void slotSave();
    void slotClear();
    void slotShuffle();

    void slotPlay(QListViewItem *);

  private:

    Playlist * playList_;
};

#endif // QT_PLAY_LIST_DIALOG_H
// vim:ts=2:sw=2:tw=78:et
