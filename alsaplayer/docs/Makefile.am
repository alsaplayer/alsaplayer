EXTRA_DIST = wishlist.txt donations.txt

clean-local:
	rm -f *~ *.bak $(DOC_STAMPS) || true

if HAVE_DOXYGEN

DOX=reference.doxygen

DOC_STAMPS=html-build.stamp

DOC_DIR=$(datadir)/doc/alsaplayer

all-local: doxygen-build.stamp

doxygen-build.stamp: $(DOX)
	@echo '*** Running doxygen ***'
	doxygen $(DOX)
	touch doxygen-build.stamp
			
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
uninstall-local: doxygen-build.stamp
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
	rm -f *.stamp || true
	if test -d reference; then rm -rf reference; fi

dist-hook: dist-hook-local
	mkdir $(distdir)/reference
	mkdir $(distdir)/reference/html
	-cp reference/html/*.html reference/html/*.css \
	    reference/html/*.png  $(distdir)/reference/html

.PHONY : dist-hook-local
