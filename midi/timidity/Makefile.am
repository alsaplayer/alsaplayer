#	$Id$

CONFIG_FILES = timidity.cfg piano+guitar drums README

TIMID_DIR = @TIMIDITY_DIR@

EXTRA_DIST = $(CONFIG_FILES)

SUBDIRS = patch

install-data-local:
	$(mkinstalldirs) $(TIMID_DIR)
	@for i in $(CONFIG_FILES); do \
	  if [ ! -f "$(TIMID_DIR)/$$i" ] ; then  \
	    echo "$(INSTALL_DATA) $(srcdir)/$$i $(DESTDIR)$(TIMID_DIR)"; \
	    $(INSTALL_DATA) $(srcdir)/$$i $(DESTDIR)$(TIMID_DIR) ; \
          fi ; \
        done

uninstall-local:
	for i in $(CONFIG_FILES); do \
	  rm -f $(TIMID_DIR)/$$i ; \
	done

