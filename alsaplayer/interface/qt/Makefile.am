if HAVE_QT

qtltlibs = libqt.la
qtldflags = -export-dynamic -avoid-version
qtsources = MainWindowBase.cpp      \
						ScopesDialogBase.cpp    \
						PlayListDialogBase.cpp  \
						MainWindow.cpp          \
						ScopesDialog.cpp        \
						PlayListDialog.cpp      \
						PlayListItem.cpp        \
						plugin.cpp

DESIGNERSOURCES = MainWindowBase.h      \
                  MainWindowBase.cpp    \
                  ScopesDialogBase.h    \
                  ScopesDialogBase.cpp  \
                  PlayListDialogBase.h  \
                  PlayListDialogBase.h

METASOURCES = MainWindowBase.moc			\
							ScopesDialogBase.moc		\
							PlayListDialogBase.moc	\
							MainWindow.moc					\
							ScopesDialog.moc				\
							PlayListDialog.moc
else

qtltlibs =
qtldflags =
qtsources =

endif

lib_LTLIBRARIES = $(qtltlibs)

AM_CXXFLAGS = @QT_CXXFLAGS@ @DFLAGS@ $(COMMON_FLAGS) \
	$(EXTRA_CFLAGS) \
	-D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
	-DADDON_DIR=\"$(ADDON_DIR)\"

LINK = $(CXXLINK)

libdir = $(ADDON_DIR)/interface

libqt_la_LDFLAGS = $(qtldflags)
libqt_la_LIBADD = @QT_LIBS@
libqt_la_SOURCES= $(qtsources)

# UIC headers

MainWindowBase.h : $(srcdir)/MainWindowBase.ui
	$(UIC) -o MainWindowBase.h $(srcdir)/MainWindowBase.ui

ScopesDialogBase.h : $(srcdir)/ScopesDialogBase.ui
	$(UIC) -o ScopesDialogBase.h $(srcdir)/ScopesDialogBase.ui

PlayListDialogBase.h : $(srcdir)/PlayListDialogBase.ui
	$(UIC) -o PlayListDialogBase.h $(srcdir)/PlayListDialogBase.ui

# UIC implementations

MainWindowBase.cpp : MainWindowBase.h MainWindowBase.moc
	$(UIC) -impl MainWindowBase.h $(srcdir)/MainWindowBase.ui > MainWindowBase.cpp
	echo '#include "MainWindowBase.moc"' >> MainWindowBase.cpp

ScopesDialogBase.cpp : ScopesDialogBase.h ScopesDialogBase.moc
	$(UIC) -impl ScopesDialogBase.h $(srcdir)/ScopesDialogBase.ui > ScopesDialogBase.cpp
	echo '#include "ScopesDialogBase.moc"' >> ScopesDialogBase.cpp

PlayListDialogBase.cpp : PlayListDialogBase.h PlayListDialogBase.moc
	$(UIC) -impl PlayListDialogBase.h $(srcdir)/PlayListDialogBase.ui > PlayListDialogBase.cpp
	echo '#include "PlayListDialogBase.moc"' >> PlayListDialogBase.cpp

MainWindowBase.cpp			: MainWindowBase.moc
ScopesDialogBase.cpp		: ScopesDialogBase.moc
PlayListDialogBase.cpp	: PlayListDialogBase.moc

$(srcdir)/MainWindow.cpp			: MainWindow.moc
$(srcdir)/ScopesDialog.cpp		: ScopesDialog.moc
$(srcdir)/PlayListDialog.cpp	: PlayListDialog.moc

MainWindowBase.moc			: MainWindowBase.h
	$(MOC) MainWindowBase.h -o MainWindowBase.moc

ScopesDialogBase.moc		: ScopesDialogBase.h
	$(MOC) ScopesDialogBase.h -o ScopesDialogBase.moc

PlayListDialogBase.moc	: PlayListDialogBase.h
	$(MOC) PlayListDialogBase.h -o PlayListDialogBase.moc

MainWindow.moc			: $(srcdir)/MainWindow.h
	$(MOC) $(srcdir)/MainWindow.h -o MainWindow.moc

ScopesDialog.moc		: $(srcdir)/ScopesDialog.h
	$(MOC) $(srcdir)/ScopesDialog.h -o ScopesDialog.moc

PlayListDialog.moc	: $(srcdir)/PlayListDialog.h
	$(MOC) $(srcdir)/PlayListDialog.h -o PlayListDialog.moc

DISTCLEANFILES = $(METASOURCES) $(DESIGNERSOURCES)

