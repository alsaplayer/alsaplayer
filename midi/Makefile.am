
SUBDIRS = timidity

midi_inltlibs = libmidi_in.la
midi_inldflags = -export-dynamic -avoid-version
midi_insources = midi_engine.c common.c playmidi.c readmidi.c readinfo.c \
	resample.c mix.c instrum.c tables.c controls.c output.c filter.c \
	fffload.c sndfont.c readsbk.c effects.c reverb_e.c chorus_e.c \
	phaser_e.c celeste_e.c resample_l.c resample_f.c cfg.c dumb_c.c \
	dumb_a.c b_out.c common.h controls.h effects.h filter.h gtim.h \
	instrum.h mix.h output.h playmidi.h readmidi.h resample.h sbk.h \
	sflayer.h tables.h version.h sf2cfg.c autocfg.c md.h

noinst_HEADERS = gtim.h common.h readmidi.h playmidi.h resample.h mix.h instrum.h \
	tables.h controls.h output.h filter.h md.h cfg.l

EXTRA_DIST = bootstrap

TIMID_DIR = @TIMIDITY_DIR@
AM_CFLAGS = -DDEFAULT_PATH=\"$(TIMID_DIR)\"

lib_LTLIBRARIES = $(midi_inltlibs)

#CFLAGS = -D_REENTRANT -I./include
CFLAGS = -I$(top_builddir) @ALSAPLAYER_CFLAGS@

libdir = $(INPUTPLUGIN_DIR)

libmidi_in_la_LDFLAGS = $(midi_inldflags)
LIBS =

libmidi_in_la_SOURCES= $(midi_insources)

cfg.c: cfg.l
	rm -f cfg.c
	flex -t -B $(srcdir)/cfg.l >cfg.c
