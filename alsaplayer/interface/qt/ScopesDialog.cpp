/*
 *   scopes_dialog.cpp (C) 2001 by Rik Hemsley (rikkus) <rik@kde.org>
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
#include <qheader.h>

#include "ScopesDialog.h"

ScopesDialog::ScopesDialog(QWidget * parent)
  : ScopesDialogBase(parent, "ScopesDialog", true)
{
  listView->header()->hide();
  new QCheckListItem(listView, "Some scope", QCheckListItem::CheckBox);
  new QCheckListItem(listView, "Some other scope", QCheckListItem::CheckBox);
}

#include "ScopesDialog.moc"
// vim:ts=2:sw=2:tw=78:et
