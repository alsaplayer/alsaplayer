PREFIX = /usr/local

# choose suitable compiler
CC = gcc

CFLAGS =	-fPIC -DBUILD_CROSS_PLATFORM -I$(PREFIX)

LINKER_FLAGS   = -L${PREFIX}/lib -lmac
LINKER_OBJECTS = mac_engine.o

# bumps the empty 'make' command

all: $(LINKER_OBJECTS)
	$(CC) -shared  mac_engine.o $(LINKER_FLAGS) -o mac_engine.so

.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

install:
	cp mac_engine.so $(PREFIX)/lib/alsaplayer/input/
clean:
	rm -f 	./*.o \
		./*~ \
		./*.so
