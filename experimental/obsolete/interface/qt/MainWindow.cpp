/*
 *   qt_interface.cpp (C) 2001 by Rik Hemsley (rikkus) <rik@kde.org>
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

#include "CorePlayer.h"
#include "Playlist.h"

#include <qapplication.h>
#include <qpopupmenu.h>
#include <qlabel.h>
#include <qslider.h>
#include <qtoolbutton.h>
#include <qmessagebox.h>
#include <qtimer.h>

#include "MainWindow.h"
#include "ScopesDialog.h"
#include "PlayListDialog.h"
#include "license.cpp"

MainWindow::MainWindow(CorePlayer * player, Playlist * playList)
  : MainWindowBase        (0, "QtAlsaPlayer"),
    player_               (player),
    playList_             (playList),
    popup_                (0L),
    playListDialog_       (0L),
    updateTitleTimer_     (0L),
    updatePositionSlider_ (true),
    updateTitle_          (true)
{
  popup_ = new QPopupMenu(this);

  popup_->insertItem(tr("Scopes..."),         this, SLOT(slotScopes()));
  popup_->insertItem(tr("Effects..."),        this, SLOT(slotFX()));
  popup_->insertItem(tr("About..."),          this, SLOT(slotAbout()));
  popup_->insertSeparator();
  popup_->insertItem(tr("CD Player (CDDA)"),  this, SLOT(slotCDDA()));
  popup_->insertSeparator();
  popup_->insertItem(tr("Exit"),              qApp, SLOT(quit()));

  menuButton->setPopup(popup_);

  resize(sizeHint().width(), minimumSizeHint().height());

  // poll me bitch.
  startTimer(200);

  updateTitleTimer_ = new QTimer(this);

  connect(updateTitleTimer_, SIGNAL(timeout()), SLOT(slotUpdateTitle()));

  playListDialog_ = new PlayListDialog(this, playList_);
  playListDialog_->hide();
}

MainWindow::~MainWindow()
{
}

void MainWindow::timerEvent(QTimerEvent *)
{
  ulong sampleRate = player_->GetSampleRate();

  bool active = player_->IsActive();

  if (active)
  {
    ulong currentTime = player_->GetCurrentTime();

    ulong currentMin = currentTime / 6000;
    ulong currentSec = (currentTime % 6000) / 100;

    ulong totalTime = player_->GetCurrentTime(player_->GetFrames());

    ulong totalMin = totalTime / 6000;
    ulong totalSec = (totalTime % 6000) / 100;

    timeLabel->setText
      (
       QString().sprintf
       (
        "%02d:%02d/%02d:%02d",
        currentMin,
        currentSec,
        totalMin,
        totalSec
       )
      );

    stream_info info;

    memset(&info, 0, sizeof(stream_info));

    player_->GetStreamInfo(&info);

    if (updateTitle_)
      titleLabel->setText(QString::fromUtf8(info.title));

    infoLabel   ->setText(QString::fromUtf8(info.stream_type));
  }
  else
  {
    titleLabel  ->setText(tr("No stream"));
    timeLabel   ->setText(tr("No time data"));
    infoLabel   ->setText("");
  }

  speedSlider ->setValue(int(player_->GetSpeed() * 100));
  speedLabel  ->setText(QString("%1%").arg(int(player_->GetSpeed() * 100)));

  if (updatePositionSlider_)
  {
    positionSlider->setRange(0, player_->GetFrames());

    // Let's try to hack around Qt 3's brokenness.

    int pixelWidth = positionSlider->width();
    int maxValue = positionSlider->maxValue();

    int stepSize = maxValue / pixelWidth;

    int oldPosition(positionSlider->value());
    int newPosition(player_->GetPosition());

    int diff = newPosition - oldPosition;

    if (diff > stepSize)
      positionSlider->setValue(player_->GetPosition());
  }

  volumeSlider  ->setValue(player_->GetVolume());
  balanceSlider ->setValue(player_->GetPan());

  volumeLabel->setText(QString("%1%").arg(player_->GetVolume()));
}

void MainWindow::closeEvent(QCloseEvent *)
{
  qApp->quit();
}

void MainWindow::slotSpeedSliderMoved(int value)
{
  player_->SetSpeed(float(value) / 100.0f);
}

void MainWindow::slotPositionSliderPressed()
{
  updatePositionSlider_ = false;
}

void MainWindow::slotPositionSliderReleased()
{
  player_->Seek(positionSlider->value());
  updatePositionSlider_ = true;
}

void MainWindow::slotVolumeSliderMoved(int value)
{
  player_->SetVolume(value);
}

void MainWindow::slotBalanceSliderMoved(int value)
{
  static const int balanceCentre        = 0;
  static const int lowBalanceThreshold  = balanceCentre - 10;
  static const int highBalanceThreshold = balanceCentre + 10;

  if (value > lowBalanceThreshold && value < highBalanceThreshold)
  {
    value = balanceCentre;
    balanceSlider ->setValue(value);
  }

  player_->SetPan(value);

  QString labelText;

  if (value < lowBalanceThreshold)
    labelText = tr("Pan: left %1%").arg(-value);

  else if (value > highBalanceThreshold)
    labelText = tr("Pan: right %1%").arg(value);

  else
    labelText = tr("Pan: center");

  setTemporaryTitle(labelText);
}

void MainWindow::slotPrevious()
{
  playList_->Prev();
}

void MainWindow::slotPlay()
{
  // player_->Start();
  // Apparently we should use the playlist...
  playList_->UnPause();
}

void MainWindow::slotNext()
{
  playList_->Next();
}

void MainWindow::slotStop()
{
  player_->Stop();
}

void MainWindow::slotPlayList()
{
  playListDialog_->show();
}

void MainWindow::slotPause()
{
  player_->SetSpeed(0.0f);
}

void MainWindow::slotBack()
{
  player_->SetSpeed(-1.0f);
}

void MainWindow::slotForward()
{
  player_->SetSpeed(1.0f);
}

void MainWindow::slotScopes()
{
  ScopesDialog scopesDialog(this);
  scopesDialog.exec();
}

void MainWindow::slotFX()
{
}

void MainWindow::slotAbout()
{
  QMessageBox::about
    (
     this,
     tr("About AlsaPlayer"),
     tr(licenseText) 
    );
}

void MainWindow::slotCDDA()
{
  player_->Stop();
  player_->SetFile("CD.cdda");
  player_->Start();
}

void MainWindow::setTemporaryTitle(const QString & s)
{
  updateTitle_ = false;
  titleLabel->setText(s);
  updateTitleTimer_->start(4000, true);
}

void MainWindow::slotUpdateTitle()
{
  updateTitle_ = true;
}

#include "MainWindow.moc"
// vim:ts=2:sw=2:tw=78:et
