/****************************************************************************
** Form implementation generated from reading ui file 'ScopesDialogBase.ui'
**
** Created: Wed Aug 29 05:38:40 2001
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#include "ScopesDialogBase.h"

#include <qheader.h>
#include <qlistview.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qvariant.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/* 
 *  Constructs a ScopesDialogBase which is a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f' 
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
ScopesDialogBase::ScopesDialogBase( QWidget* parent,  const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "ScopesDialogBase" );
    resize( 259, 254 ); 
    setCaption( tr( "Scopes - AlsaPlayer" ) );
    ScopesDialogBaseLayout = new QVBoxLayout( this ); 
    ScopesDialogBaseLayout->setSpacing( 6 );
    ScopesDialogBaseLayout->setMargin( 11 );

    listView = new QListView( this, "listView" );
    listView->addColumn( tr( "Scopes" ) );
    ScopesDialogBaseLayout->addWidget( listView );

    Layout1 = new QHBoxLayout; 
    Layout1->setSpacing( 6 );
    Layout1->setMargin( 0 );
    QSpacerItem* spacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    Layout1->addItem( spacer );

    closeButton = new QPushButton( this, "closeButton" );
    closeButton->setText( tr( "&Close" ) );
    closeButton->setAutoDefault( TRUE );
    closeButton->setDefault( TRUE );
    Layout1->addWidget( closeButton );
    ScopesDialogBaseLayout->addLayout( Layout1 );

    // signals and slots connections
    connect( closeButton, SIGNAL( clicked() ), this, SLOT( accept() ) );
}

/*  
 *  Destroys the object and frees any allocated resources
 */
ScopesDialogBase::~ScopesDialogBase()
{
    // no need to delete child widgets, Qt does it all for us
}

