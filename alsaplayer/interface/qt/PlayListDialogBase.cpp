/****************************************************************************
** Form implementation generated from reading ui file 'PlayListDialogBase.ui'
**
** Created: Wed Aug 29 17:21:24 2001
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#include "PlayListDialogBase.h"

#include <qheader.h>
#include <qlistview.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qvariant.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/* 
 *  Constructs a PlayListDialogBase which is a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f' 
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
PlayListDialogBase::PlayListDialogBase( QWidget* parent,  const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "PlayListDialogBase" );
    resize( 457, 269 ); 
    setCaption( tr( "Playlist - AlsaPlayer" ) );
    setAcceptDrops( TRUE );
    PlayListDialogBaseLayout = new QHBoxLayout( this ); 
    PlayListDialogBaseLayout->setSpacing( 6 );
    PlayListDialogBaseLayout->setMargin( 11 );

    list = new QListView( this, "list" );
    list->addColumn( tr( "Filename" ) );
    list->setSelectionMode( QListView::Multi );
    list->setAllColumnsShowFocus( TRUE );
    PlayListDialogBaseLayout->addWidget( list );

    Layout2 = new QVBoxLayout; 
    Layout2->setSpacing( 6 );
    Layout2->setMargin( 0 );

    addButton = new QPushButton( this, "addButton" );
    addButton->setText( tr( "&Add..." ) );
    Layout2->addWidget( addButton );

    loadButton = new QPushButton( this, "loadButton" );
    loadButton->setText( tr( "&Load..." ) );
    Layout2->addWidget( loadButton );

    saveButton = new QPushButton( this, "saveButton" );
    saveButton->setText( tr( "&Save..." ) );
    Layout2->addWidget( saveButton );

    removeButton = new QPushButton( this, "removeButton" );
    removeButton->setText( tr( "&Remove" ) );
    Layout2->addWidget( removeButton );

    clearButton = new QPushButton( this, "clearButton" );
    clearButton->setText( tr( "C&lear" ) );
    Layout2->addWidget( clearButton );

    closeButton = new QPushButton( this, "closeButton" );
    closeButton->setText( tr( "&Close" ) );
    closeButton->setAutoDefault( TRUE );
    closeButton->setDefault( TRUE );
    Layout2->addWidget( closeButton );
    QSpacerItem* spacer = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
    Layout2->addItem( spacer );
    PlayListDialogBaseLayout->addLayout( Layout2 );

    // signals and slots connections
    connect( closeButton, SIGNAL( clicked() ), this, SLOT( accept() ) );
    connect( saveButton, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( loadButton, SIGNAL( clicked() ), this, SLOT( slotLoad() ) );
    connect( addButton, SIGNAL( clicked() ), this, SLOT( slotAdd() ) );
    connect( removeButton, SIGNAL( clicked() ), this, SLOT( slotRemove() ) );
    connect( clearButton, SIGNAL( clicked() ), this, SLOT( slotClear() ) );
}

/*  
 *  Destroys the object and frees any allocated resources
 */
PlayListDialogBase::~PlayListDialogBase()
{
    // no need to delete child widgets, Qt does it all for us
}

void PlayListDialogBase::slotAdd()
{
    qWarning( "PlayListDialogBase::slotAdd(): Not implemented yet!" );
}

void PlayListDialogBase::slotClear()
{
    qWarning( "PlayListDialogBase::slotClear(): Not implemented yet!" );
}

void PlayListDialogBase::slotLoad()
{
    qWarning( "PlayListDialogBase::slotLoad(): Not implemented yet!" );
}

void PlayListDialogBase::slotRemove()
{
    qWarning( "PlayListDialogBase::slotRemove(): Not implemented yet!" );
}

void PlayListDialogBase::slotSave()
{
    qWarning( "PlayListDialogBase::slotSave(): Not implemented yet!" );
}

