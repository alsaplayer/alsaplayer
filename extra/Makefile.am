EXTRA_DIST = README desert.theme alsaplayer.desktop

DESKTOP_DIR=$(datadir)/applications

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

uninstall-local:
	(installfiles=`echo *.desktop`; \
	if test "$$installfiles" = '*.desktop'; \
	then echo '--Nothing to uninstall' ; \
	else \
	  for i in $$installfiles ; do \
	    echo '-- Unstalling '$$i ; \
	    rm $(DESKTOP_DIR)/$$i; \
	  done; \
	fi)
