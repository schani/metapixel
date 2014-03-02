PREFIX = /usr/local
INSTALL = install
MANPAGE_XSL = /usr/share/xml/docbook/stylesheet/nwalsh/current/manpages/docbook.xsl

BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man

VERSION = 1.1.0

DEBUG = -g
#OPTIMIZE = -O2
#PROFILE = -pg

#MACOS_LDOPTS = -L/sw/lib
#MACOS_CFLAGS = -I/usr/X11/include/libpng15

CC = gcc
CFLAGS = $(MACOS_CFLAGS) $(OPTIMIZE) $(DEBUG) $(PROFILE)
FORMATDEFS = -DRWIMG_JPEG -DRWIMG_PNG -DRWIMG_GIF

export CFLAGS CC FORMATDEFS

LDOPTS = $(MACOS_LDOPTS) -L/usr/X11R6/lib $(PROFILE) $(DEBUG) `pkg-config --libs glib-2.0`
CCOPTS = $(CFLAGS) -I/usr/X11R6/include -I/usr/X11R6/include/X11 -I. -Wall \
	 -DMETAPIXEL_VERSION=\"$(VERSION)\" $(FORMATDEFS) -DCONSOLE_OUTPUT `pkg-config --cflags glib-2.0`
CC = gcc
#LIBFFM = -lffm

OBJS = main.o bitmap.o color.o metric.o matcher.o tiling.o metapixel.o library.o classic.o collage.o search.o \
	utils.o error.o zoom.o \
	getopt.o getopt1.o
IMAGESIZE_OBJS = imagesize.o
BOREDOM_OBJS = boredom.o

all : metapixel metapixel.1 imagesize boredom

librwimg :
	$(MAKE) -C rwimg

liblispreader :
	$(MAKE) -C lispreader

metapixel : $(OBJS) librwimg liblispreader
	$(CC) -o metapixel $(OBJS) rwimg/librwimg.a lispreader/liblispreader.a -lpng -ljpeg -lgif $(LIBFFM) -lm -lz $(LDOPTS)

metapixel.1 : metapixel.xml
	xsltproc --nonet $(MANPAGE_XSL) metapixel.xml

imagesize : $(IMAGESIZE_OBJS)
	$(CC) -o imagesize $(IMAGESIZE_OBJS) rwimg/librwimg.a -lpng -ljpeg -lgif -lm -lz  $(LDOPTS)

boredom : $(BOREDOM_OBJS)
	$(CC) -o boredom $(BOREDOM_OBJS) rwimg/librwimg.a -lpng -ljpeg -lgif -lm -lz $(LDOPTS)

zoom : zoom.c rwjpeg.c rwpng.c readimage.c writeimage.c
	$(CC) -o zoom $(OPTIMIZE) $(PROFILE) $(MACOS_CCOPTS) -DTEST_ZOOM -DRWIMG_JPEG -DRWIMG_PNG \
		zoom.c rwjpeg.c rwpng.c readimage.c writeimage.c $(MACOS_LDOPTS) -lpng -ljpeg -lm -lz

%.o : %.c
	$(CC) $(CCOPTS) -c $<

TAGS :
	etags `find . -name '*.c' -o -name '*.h'`

install : metapixel metapixel.1
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) metapixel $(BINDIR)
	$(INSTALL) metapixel-prepare $(BINDIR)
	$(INSTALL) metapixel.1 $(MANDIR)/man1
#	$(INSTALL) imagesize $(BINDIR)
#	$(INSTALL) sizesort $(BINDIR)

clean :
	rm -f *.o metapixel imagesize boredom *~
	$(MAKE) -C rwimg clean
	$(MAKE) -C lispreader clean

realclean : clean
	rm -f metapixel.1

dist : metapixel.1
	rm -rf metapixel-$(VERSION)
	mkdir metapixel-$(VERSION)
	cp Makefile README NEWS COPYING *.[ch] metapixel-prepare sizesort metapixel.xml metapixel.1 metapixelrc \
		metapixel-$(VERSION)/
	tar -zcvf metapixel-$(VERSION).tar.gz metapixel-$(VERSION)
	rm -rf metapixel-$(VERSION)
