/*
 *   playlist_dialog.cpp (C) 2001 by Rik Hemsley (rikkus) <rik@kde.org>
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

#include <qlistview.h>
#include <qfiledialog.h>
#include <qmessagebox.h>
#include <qheader.h>

#include <vector>
#include <algorithm>

#include "PlayListItem.h"
#include "PlayListDialog.h"

PlayListDialog::PlayListDialog(QWidget * parent, Playlist * playList)
  : PlayListDialogBase(parent, "PlayListDialog"),
    PlaylistInterface(),
    playList_(playList)
{
  list->setSorting(0);

  list->header()->hide();

  playList_->Register(this);
}

PlayListDialog::~PlayListDialog()
{
  playList_->UnRegister(this);
}

void PlayListDialog::slotAdd()
{
  PlayListDialogBase::slotAdd();

  QStringList l =
    QFileDialog::getOpenFileNames(QString::null, QString::null, this,
      "FindSomeMusic", tr("Add music - AlsaPlayer"));

  if (!l.isEmpty())
  {
    vector<string> stlVec;

    for (QStringList::ConstIterator it(l.begin()); it != l.end(); ++it)
      stlVec.push_back(string((*it).local8Bit().data()));

    playList_->Insert(stlVec, playList_->Length());
  }
}

void PlayListDialog::slotLoad()
{
  QString filename =
    QFileDialog::getOpenFileName(QString::null, QString::null, this,
      "LoadAPlayList", tr("Load playlist - AlsaPlayer"));

  if (!!filename)
  {
    uint oldLength = playList_->Length();

    plist_result ret =
      playList_->Load(string(filename.local8Bit().data()), 0, false);

    if (E_PL_SUCCESS != ret)
    {
      QMessageBox::warning(this, tr("Warning - AlsaPlayer"),
        tr("Couldn't load playlist !"));
    }
    else
    {
      // Remove old entries. XXX Do we really want to ?
      playList_->Remove(oldLength, playList_->Length() - oldLength);
    }
  }
}

void PlayListDialog::slotRemove()
{
  ::std::vector<int> l;

  for (QListViewItemIterator it(list); it.current(); ++it)
  {
    PlayListItem * item = static_cast<PlayListItem *>(it.current());

    if (item->isSelected())
    {
      qDebug("playList_->Remove(%d)", item->height());
      playList_->Remove(item->height(), item->height());
    }
  }
  
#if 0
  ::std::sort(l.begin(), l.end());
  ::std::reverse(l.begin(), l.end());

  int begin(l.front());
  int end(begin);

  for (vector<int>::const_iterator it(l.begin()); it != l.end(); ++it)
  {
    qDebug("Removing item %d", *it);
    if (*it != (end + 1))
    {
      playList_->Remove(begin, end);

      begin = *it;
      end   = begin;
    }
    else
    {
      end = *it;
    }
  }
#endif
}

void PlayListDialog::slotSave()
{
  QString filename =
    QFileDialog::getSaveFileName(QString::null, QString::null, this,
      "LoadAPlayList", tr("Save playlist - AlsaPlayer"));

  if (!!filename)
  {
    plist_result ret =
      playList_->Save(string(filename.local8Bit().data()), PL_FORMAT_M3U);

    if (ret != E_PL_SUCCESS)
    {
      QMessageBox::warning(this, tr("Warning - AlsaPlayer"),
        tr("Couldn't save playlist !"));
    }
  }
}

void PlayListDialog::slotClear()
{
  playList_->Clear();
}


void PlayListDialog::CbSetCurrent(unsigned cur)
{
  for (QListViewItemIterator it(list); it.current(); ++it)
  {
    PlayListItem * i = static_cast<PlayListItem *>(it.current());

    if (i->height() == cur)
    {
      list->setCurrentItem(i);
      break;
    }
  }
}

void PlayListDialog::CbInsert(std::vector<PlayItem> & l, unsigned pos)
{
  std::vector<PlayItem>::const_iterator it;

  for (it = l.begin(); it != l.end(); ++it)
    new PlayListItem(QString::fromLocal8Bit((*it).filename.c_str()), list);
}

void PlayListDialog::CbRemove(unsigned begin, unsigned end)
{
  qDebug("CbRemove(%d, %d)", begin, end);

  for (int i = end; i >= begin; --i)
  {
    for (QListViewItemIterator it(list); it.current(); --it)
    {
      PlayListItem * item = static_cast<PlayListItem *>(it.current());

      if (item->height() == i)
      {
        qDebug("Removing item from display %d", i);
        delete item;
      }
    }
  }
}


void PlayListDialog::CbLock()
{
  return;
}


void PlayListDialog::CbUnlock()
{
  return;
}


void PlayListDialog::CbClear()
{
  list->clear();
}

#include "PlayListDialog.moc"
// vim:ts=2:sw=2:tw=78:et
