#	$Id$

CONFIG_FILES = *.pat

TIMID_DIR = @TIMIDITY_DIR@
PATCH_DIR = $(TIMID_DIR)/patch

EXTRA_DIST = $(CONFIG_FILES)

install-data-local:
	$(mkinstalldirs) $(PATCH_DIR)
	@for i in $(CONFIG_FILES); do \
	  if [ ! -f "$(PATCH_DIR)/$$i" ] ; then  \
	    echo "$(INSTALL_DATA) $(srcdir)/$$i $(DESTDIR)$(PATCH_DIR)"; \
	    $(INSTALL_DATA) $(srcdir)/$$i $(DESTDIR)$(PATCH_DIR) ; \
          fi ; \
        done

uninstall-local:
	for i in $(CONFIG_FILES); do \
	  rm -f $(PATCH_DIR)/$$i ; \
	done

