AUTOMAKE_OPTIONS = foreign
alsaplayerincludedir = $(includedir)/alsaplayer

lib_LTLIBRARIES = libalsaplayer.la

INCLUDES = -I$(top_srcdir)/alsaplayer

libalsaplayer_la_LDFLAGS = -export-dynamic -version-info 0:1:0

libalsaplayer_la_SOURCES = external.cpp

EXTRA_DIST = packet.h

alsaplayerinclude_HEADERS = control.h
