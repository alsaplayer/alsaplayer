/****************************************************************************
** Form interface generated from reading ui file 'MainWindowBase.ui'
**
** Created: Wed Aug 29 16:54:39 2001
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#ifndef MAINWINDOWBASE_H
#define MAINWINDOWBASE_H

#include <qvariant.h>
#include <qwidget.h>
class QVBoxLayout; 
class QHBoxLayout; 
class QGridLayout; 
class QFrame;
class QLabel;
class QSlider;
class QToolButton;

class MainWindowBase : public QWidget
{ 
    Q_OBJECT

public:
    MainWindowBase( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~MainWindowBase();

    QFrame* Frame3;
    QLabel* speedLabelLeft;
    QLabel* timeLabel;
    QLabel* speedLabel;
    QLabel* titleLabel;
    QLabel* volumeLabel;
    QLabel* volumeLabelLeft;
    QLabel* infoLabel;
    QSlider* positionSlider;
    QToolButton* menuButton;
    QToolButton* previousButton;
    QToolButton* playButton;
    QToolButton* nextButton;
    QToolButton* stopButton;
    QToolButton* playlistButton;
    QToolButton* pauseButton;
    QToolButton* backButton;
    QToolButton* forwardButton;
    QSlider* speedSlider;
    QLabel* PixmapLabel2;
    QSlider* balanceSlider;
    QLabel* PixmapLabel1;
    QSlider* volumeSlider;

protected slots:
    virtual void slotCDDA();
    virtual void slotAbout();
    virtual void slotBack();
    virtual void slotBalanceSliderMoved(int);
    virtual void slotFX();
    virtual void slotForward();
    virtual void slotNext();
    virtual void slotPause();
    virtual void slotPlay();
    virtual void slotPlayList();
    virtual void slotPositionSliderPressed();
    virtual void slotPositionSliderReleased();
    virtual void slotPrevious();
    virtual void slotScopes();
    virtual void slotSpeedSliderMoved(int);
    virtual void slotStop();
    virtual void slotVolumeSliderMoved(int);

protected:
    QVBoxLayout* MainWindowBaseLayout;
    QGridLayout* Frame3Layout;
    QHBoxLayout* Layout10;
    QVBoxLayout* Layout9;
    QHBoxLayout* Layout8;
    QVBoxLayout* Layout5;
    QHBoxLayout* Layout3;
    QHBoxLayout* Layout4;
};

#endif // MAINWINDOWBASE_H
