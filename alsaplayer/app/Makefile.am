## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
## If you would like to negotiate alternate licensing terms, you may do
## so by contacting the author: Andy Lo A Foe <andy@alsaplayer.org>
##
## $Id$
##

if ARCH_ULTRA

SUN_LIBS = -lresolv

endif

if HAVE_JACK

JACK_FLAGS = -DUSE_JACK

endif

LIBS = -lpthread -ldl $(SUN_LIBS) @JACK_LIB@

COMMON_FLAGS = @DFLAGS@ -D_REENTRANT \
		-I$(top_srcdir)/include \
		-DADDON_DIR=\"$(ADDON_DIR)\" @ARCH_CFLAGS@ $(JACK_FLAGS)

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

jackplayer:
		ln -s alsaplayer jackplayer
