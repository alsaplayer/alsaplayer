EXTRA_DIST = README desert.theme alsaplayer.desktop alsaplayer.png

DESKTOP_DIR=$(datadir)/applications
ICON_DIR=$(datadir)/pixmaps

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(DESKTOP_DIR)
	(installfiles=`echo *.desktop`; \
	if test "$$installfiles" = '*.desktop'; \
	then echo '--Nothing to install' ; \
	else \
	  for i in $$installfiles ; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(DESKTOP_DIR); \
	  done; \
	fi)
	$(mkinstalldirs) $(DESTDIR)$(ICON_DIR)
	(installfiles=`echo *.png`; \
	if test "$$installfiles" = '*.png'; \
	then echo '--Nothing to install' ; \
	else \
	  for i in $$installfiles ; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(ICON_DIR); \
	  done; \
	fi)

uninstall-local:
	(installfiles=`echo *.desktop`; \
	if test "$$installfiles" = '*.desktop'; \
	then echo '--Nothing to uninstall' ; \
	else \
	  for i in $$installfiles ; do \
	    echo '-- Uninstalling '$$i ; \
	    rm $(DESKTOP_DIR)/$$i; \
	  done; \
	fi)
	(installfiles=`echo *.png`; \
	if test "$$installfiles" = '*.png'; \
	then echo '--Nothing to uninstall' ; \
	else \
	  for i in $$installfiles ; do \
	    echo '-- Uninstalling '$$i ; \
	    rm $(ICON_DIR)/$$i; \
	  done; \
	fi)
