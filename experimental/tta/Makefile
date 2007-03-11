#
# $Id: Makefile,v 1.1.1.1 2005/03/21 17:17:16 peter_lemenkov Exp $
#

CC	= gcc

PREFIX = /usr
DEBUG =		-Wall -O2
CFLAGS =	
CPPFLAGS =	-I$(PREFIX)/include/alsaplayer 

LINKER_FLAGS   = 
LINKER_OBJECTS = tta_engine.o \
		 id3.o \
		 ttadec.o

all: $(LINKER_OBJECTS)
	$(CC) -fPIC -shared $(LINKER_OBJECTS) $(LINKER_FLAGS) -o tta_engine.so

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<
clean:
	rm -f *.o *.lo *~ *.so
