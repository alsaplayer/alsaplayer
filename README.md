# AlsaPlayer - PCM audio player for Linux and compatible OSes

Original version written by Andy Lo A Foe (andy@loafoe.nl)
GZK2 interface written by Madej from Poland.
Contributions from many other peoples. See the AUTHORS file for details.

## DESCRIPTION
AlsaPlayer is a PCM player developed on the Linux Operating System. Since
the first public beta release it support has been added for various other
Operating Systems (mostly Unix variants). AlsaPlayer was written in the first
place to exercise the new ALSA (Advanced Linux Sound Architecture) driver and
library system.


## INTERFACE PLUGINS
This cool feature allows you to completely customize your user interface.
I.e. the core of AlsaPlayer is decoupled from the user interface. The default
GTK2 interface is the best supported one. Among others, there is also a text
only interface.


## INPUT PLUGINS
The program is very much plugin based. New file formats can be added simply
by writing a new input plugin. The only requirement is that the data can be
presented in PCM audio format.The following plugins are in various states
of usability:

- OGG vorbis plugin, works flawlessly, uses the ogg/vorbis libs
  http://www.vorbis.com/
- MPEG audio plugin, based on mpg123 0.59r, works quite well, being phased out
  however for:
- MAD based audio plugin, based on the new MAD MPEG decoder library, uses a
  tiny bit more CPU, but has much better output quality then mpg123.
  http://www.mars.org/home/rob/proj/mpeg/
- CDDA plugin, play back audio CD's by ripping the data digitally off the disk
- MikMod plugin, play back all MikMod supported module formats, no random
  seeking in modules yet.
  http://www.mikmod.org/
- Audiofile library plugin, this one needs a bit of work still
  http://oss.sgi.com/projects/audiofile/
- FLAC (including OggFLAC) plugin with support for FLAC >= 1.3.

## OUTPUT PLUGINS
AlsaPlayer also uses a plugin system for outputting audio data. The output
mechanism was designed with ALSA in mind of course. Many other Unix audio
systems map quite well on to it however. Supported output plugins include:

- ALSA, default plugin, best supported.
- OSS and OSS/Lite
- Esound
- Sparc (tested on UltraSparc)
- SGI
- JACK, http://jackit.sf.net, this plugin is actually built-in since its
  differs radically from all the others. It is callback based. This is my
  preferred output method these days. The underlying audio driver is ALSA.


## SCOPE PLUGINS
Just as input and output support gets loaded in dynamically, scope (or
visualization) plugins are loaded in dynamically also. This enables anyone to
develop a visualization plugin without changing a single line of code in the
main program. A few scope plugins are provided in the main alsaplayer
distribution:

- Monoscope
- Spacescope
- Levelmeter
- Blurscope
- OpenGL spectrum
- logFFtscope

Nothing stops you from writing a kick ass *FULLSCREEN* (DGA) or even a
*Hardwarde Accelerated OpenGL* visualization plugin for AlsaPlayer!


## EFFECTS PLUGINS
Work in progress...


## SOCKET CONTROL
AlsaPlayer can be controlled from an external program. You only need to
link your application against the supplied libalsaplayer.so and in order
to control AlsaPlayer from your own applications. Seeking, speed control and
playlist advancement are only a few of the commands available to you.
See the "examples" directory for sample implementations.


## INSTALLATION
Installing AlsaPlayer from source should be as easy as executing the configure
script and then make. The configure script will try to detect all input and
output plugins for your system.

*IMPORTANT*: Make sure you run 'make install' after the compilation is
finished. The various plugins need to be in a specific place on your system.
If you don't want to install it on your system right away you can always use a
different --prefix when running configure.


## EMBEDDED MODE
For target systems with low CPU and RAM resources you can run configure with
"CFLAGS=-DEMBEDDED". This reduces CPU and RAM usage but imposes the following
limitations:

- no mixing of streams; single-stream only
- no software volume/pan (hardware volume/pan unaffected)
- no effect plugins


## RUNNING
Just fire up the executable. You can pass files to play on the command line
too. These will be added to the queue while the first entry will start playing
automatically. The CD like button hides the menu. All other controls should be
straight forward. Improvements to the interface will follow soon.

## OTHER INFO
- WWW Page : http://alsaplayer.sourceforge.net/
- Original author, Email : <andy@loafoe.nl>
- Administrator, Email: <dominique_libre@users.sourceforge.net>
