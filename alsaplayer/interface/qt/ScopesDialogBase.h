/****************************************************************************
** Form interface generated from reading ui file 'ScopesDialogBase.ui'
**
** Created: Wed Aug 29 05:38:40 2001
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#ifndef SCOPESDIALOGBASE_H
#define SCOPESDIALOGBASE_H

#include <qvariant.h>
#include <qdialog.h>
class QVBoxLayout; 
class QHBoxLayout; 
class QGridLayout; 
class QListView;
class QListViewItem;
class QPushButton;

class ScopesDialogBase : public QDialog
{ 
    Q_OBJECT

public:
    ScopesDialogBase( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~ScopesDialogBase();

    QListView* listView;
    QPushButton* closeButton;

protected:
    QVBoxLayout* ScopesDialogBaseLayout;
    QHBoxLayout* Layout1;
};

#endif // SCOPESDIALOGBASE_H
