if HAVE_CURL

curlltlibs = libcurl.la
curlldflags = -export-dynamic -avoid-version
curlsources = curl.c

else

curlltlibs =
curlldflags =
curlsources =

endif

lib_LTLIBRARIES = $(curlltlibs)

CFLAGS = @DFLAGS@ @CURL_CFLAGS@ -Wall -D_REENTRANT -I$(top_builddir) -I$(top_srcdir)/alsaplayer

libdir = $(ADDON_DIR)/reader

libcurl_la_LDFLAGS = $(curlldflags)
LIBS = @CURL_LIBS@

libcurl_la_SOURCES= $(curlsources)
