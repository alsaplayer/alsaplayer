EXTRA_DIST = wishlist.txt donations.txt

clean-local:
	rm -f *~ *.bak reference.doxygen.log || true

if HAVE_DOXYGEN

DOC_DIR=$(datadir)/doc/alsaplayer

all-local: reference.doxygen.log

reference.doxygen.log: reference.doxygen $(top_srcdir)/configure
	$(AM_V_GEN)doxygen reference.doxygen
	@UNDOCUMENTED=$$(cat $@); \
	if test -n "$$UNDOCUMENTED" ; then \
		echo "---------- UNDOCUMENTED CODE ----------"; \
		echo "$$UNDOCUMENTED"; \
		echo "---------------------------------------"; \
		fi

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(DOC_DIR)/reference/html
	(installfiles=`echo reference/html/*.html`; \
	if test "$$installfiles" = 'reference/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles reference/html/doxygen.png reference/html/doxygen.css; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(DOC_DIR)/reference/html; \
	  done; \
	fi)

# we need doxygen stuff built so we can know what to uninstall
uninstall-local: reference.doxygen.log
	(installfiles=`echo reference/html/*.html`; \
	if test "$$installfiles" = 'reference/html/*.html'; \
	then echo '-- Nothing to uninstall' ; \
	else \
	  for i in $$installfiles reference/html/doxygen.png reference/html/doxygen.css; do \
	    echo '-- Unstalling '$$i ; \
	    rm $(DOC_DIR)/$$i; \
	  done; \
	fi)

dist-check-doxygen:

endif

distclean-local: clean
	rm -f reference.doxygen.log || true
	if test -d reference; then rm -rf reference; fi

dist-hook: dist-hook-local
	mkdir $(distdir)/reference
	mkdir $(distdir)/reference/html
	-cp reference/html/*.html reference/html/*.css \
	    reference/html/*.png  $(distdir)/reference/html

.PHONY : dist-hook-local
