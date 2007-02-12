PREFIX = /usr

# choose suitable compiler
COMPILATOR = gcc

DEBUG =		-Wall -O2
CFLAGS =	-fPIC
CPPFLAGS =	-I$(PREFIX)/include

LINKER_FLAGS   = -L${PREFIX}/lib -lwavpack
LINKER_OBJECTS = wv_engine.o

# bumps the empty 'make' command

all: $(LINKER_OBJECTS)
	$(CC) -shared -fPIC $(LINKER_OBJECTS) $(LINKER_FLAGS) -o wv_engine.so

.cpp.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -f *.so *.o *~
