if ARCH_ULTRA

SUN_LIBS = -lresolv

endif

LIBS = -lpthread -ldl $(SUN_LIBS)

COMMON_FLAGS = @DFLAGS@ -D_REENTRANT \
		-I$(top_srcdir)/include \
		-DADDON_DIR=\"$(ADDON_DIR)\" @ARCH_CFLAGS@

CXXFLAGS = $(COMMON_FLAGS) $(EXTRA_CFLAGS)

CFLAGS = $(CXXFLAGS)

bin_PROGRAMS = alsaplayer 

alsaplayer_LDFLAGS = -export-dynamic

alsaplayer_SOURCES = AlsaSubscriber.cpp \
		AlsaNode.cpp Effects.cpp \
		Main.cpp CorePlayer.cpp Playlist.cpp  \
		SampleBuffer.cpp \
		reverbst.cpp \
		fft.c convolve.c utilities.cpp 
