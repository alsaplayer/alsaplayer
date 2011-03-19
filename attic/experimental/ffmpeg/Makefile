PREFIX? = /usr

# choose suitable compiler
COMPILATOR = g++

DEBUG =		-Wall -O2
CFLAGS =	-fPIC
CPPFLAGS =	-I$(PREFIX)/include `pkg-config libavcodec libavformat --cflags` 

LINKER_FLAGS   = -L${PREFIX}/lib `pkg-config libavcodec libavformat --libs` 
LINKER_OBJECTS = ffmpeg_engine.o

# bumps the empty 'make' command

all: $(LINKER_OBJECTS)
	$(CC) -shared -fPIC $(LINKER_OBJECTS) $(LINKER_FLAGS) -o ffmpeg_engine.so

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

install:
	cp ffmpeg_engine.so $(PREFIX)/lib/alsaplayer/input/

clean:
	rm -f *.so *.o *~
