/****************************************************************************
** Form interface generated from reading ui file 'PlayListDialogBase.ui'
**
** Created: Wed Aug 29 17:21:24 2001
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#ifndef PLAYLISTDIALOGBASE_H
#define PLAYLISTDIALOGBASE_H

#include <qvariant.h>
#include <qdialog.h>
class QVBoxLayout; 
class QHBoxLayout; 
class QGridLayout; 
class QListView;
class QListViewItem;
class QPushButton;

class PlayListDialogBase : public QDialog
{ 
    Q_OBJECT

public:
    PlayListDialogBase( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~PlayListDialogBase();

    QListView* list;
    QPushButton* addButton;
    QPushButton* loadButton;
    QPushButton* saveButton;
    QPushButton* removeButton;
    QPushButton* clearButton;
    QPushButton* closeButton;

protected slots:
    virtual void slotAdd();
    virtual void slotClear();
    virtual void slotLoad();
    virtual void slotRemove();
    virtual void slotSave();

protected:
    QHBoxLayout* PlayListDialogBaseLayout;
    QVBoxLayout* Layout2;
};

#endif // PLAYLISTDIALOGBASE_H
