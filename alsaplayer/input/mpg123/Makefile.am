EXTRA_DIST = README decode_i386.c decode_i586.s dct64_i386.c decode.c \
			dct64.c audio.h
mpg123ltlibs = libmpg123.la

libmpg123_la_LDFLAGS = -module -avoid-version
lib_LTLIBRARIES = $(mpg123ltlibs)

libdir = $(ADDON_DIR)/input

ASFLAGS =

COMMON_CFLAGS = -I$(top_builddir) -I$(top_srcdir)/alsaplayer \
	@ARCH_CFLAGS@ -D_REENTRANT @DFLAGS@

COMMON_SRC = common.c decode_2to1.c decode_4to1.c decode_ntom.c \
	layer1.c layer2.c layer3.c tabinit.c equalizer.c huffman.h \
	mpg123.h mpg123_engine.c readers.c l2tables.h audio.h \
	mpg123_engine.h httpget.c common.h genre.h getbits.h getbits.c


EXTRA_X86_SRC = decode_i386.c decode_i586.s dct64_i386.c

if ARCH_486

EXTRA_CFLAGS = -DI386_ASSEM -DPENTIUM_OPT -DLINUX -DREAL_IS_FLOAT
EXTRA_SRC = $(EXTRA_X86_SRC)

else

if ARCH_586
	
EXTRA_CFLAGS = -DI386_ASSEM -DPENTIUM_OPT -DLINUX -DREAL_IS_FLOAT
EXTRA_SRC = $(EXTRA_X86_SRC)

else

if ARCH_686

EXTRA_CFLAGS = -DI386_ASSEM -DPENTIUM_OPT -DLINUX -DREAL_IS_FLOAT
EXTRA_SRC =  $(EXTRA_X86_SRC)

else

if ARCH_PPC

EXTRA_CFLAGS = -mcpu=ppc
EXTRA_SRC = decode.c dct64.c

else

EXTRA_SRC = decode.c dct64.c
EXTRA_CFLAGS =

endif
endif
endif
endif
	
CFLAGS = $(COMMON_CFLAGS) $(EXTRA_CFLAGS)

INCLUDES = -I$(ROOT_DIR)/app $(GLIB_CFLAGS)

libmpg123_la_SOURCES = $(COMMON_SRC) $(EXTRA_SRC)
