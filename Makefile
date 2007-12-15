PREFIX = /usr/local
INSTALL = install
MANPAGE_XSL = /usr/share/xml/docbook/stylesheet/nwalsh/manpages/docbook.xsl

BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man

VERSION = 1.1.0

DEBUG = -g
#OPTIMIZE = -O2
#PROFILE = -pg

MACOS_LDOPTS = -L/sw/lib
MACOS_CFLAGS = -I/sw/include

CC = gcc
CFLAGS = $(MACOS_CFLAGS) $(OPTIMIZE) $(DEBUG) $(PROFILE)
FORMATDEFS = -DRWIMG_JPEG -DRWIMG_PNG -DRWIMG_GIF

export CFLAGS CC FORMATDEFS

LDOPTS = $(MACOS_LDOPTS) -L/usr/X11R6/lib $(PROFILE) $(DEBUG)
CCOPTS = $(CFLAGS) -I/usr/X11R6/include -I/usr/X11R6/include/X11 -I. -Wall \
	 -DMETAPIXEL_VERSION=\"$(VERSION)\" $(FORMATDEFS) -DCONSOLE_OUTPUT
CC = gcc
#LIBFFM = -lffm

LISPREADER_OBJS = lispreader.o pools.o allocator.o
OBJS = main.o bitmap.o color.o metric.o matcher.o tiling.o metapixel.o library.o classic.o collage.o search.o \
	utils.o error.o avl.o vector.o zoom.o \
	$(LISPREADER_OBJS) getopt.o getopt1.o
CONVERT_OBJS = convert.o $(LISPREADER_OBJS) getopt.o getopt1.o
IMAGESIZE_OBJS = imagesize.o

all : metapixel metapixel.1 imagesize
# convert 

librwimg :
	$(MAKE) -C rwimg

metapixel : $(OBJS) librwimg
	$(CC) $(LDOPTS) -o metapixel $(OBJS) rwimg/librwimg.a -lpng -ljpeg -lgif $(LIBFFM) -lm -lz

metapixel.1 : metapixel.xml
	xsltproc --nonet $(MANPAGE_XSL) metapixel.xml

convert : $(CONVERT_OBJS)
	$(CC) $(LDOPTS) -o convert $(CONVERT_OBJS)

imagesize : $(IMAGESIZE_OBJS)
	$(CC) $(LDOPTS) -o imagesize $(IMAGESIZE_OBJS) rwimg/librwimg.a -lpng -ljpeg -lgif -lm -lz

zoom : zoom.c rwjpeg.c rwpng.c readimage.c writeimage.c
	$(CC) -o zoom $(OPTIMIZE) $(PROFILE) $(MACOS_CCOPTS) -DTEST_ZOOM -DRWIMG_JPEG -DRWIMG_PNG \
		zoom.c rwjpeg.c rwpng.c readimage.c writeimage.c $(MACOS_LDOPTS) -lpng -ljpeg -lm -lz

%.o : %.c
	$(CC) $(CCOPTS) -c $<

install : metapixel metapixel.1
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) metapixel $(BINDIR)
	$(INSTALL) metapixel-prepare $(BINDIR)
	$(INSTALL) metapixel.1 $(MANDIR)/man1
#	$(INSTALL) imagesize $(BINDIR)
#	$(INSTALL) sizesort $(BINDIR)

clean :
	rm -f *.o metapixel convert imagesize *~
	$(MAKE) -C rwimg clean

realclean : clean
	rm -f metapixel.1

dist : metapixel.1
	rm -rf metapixel-$(VERSION)
	mkdir metapixel-$(VERSION)
	cp Makefile README NEWS COPYING *.[ch] metapixel-prepare sizesort metapixel.xml metapixel.1 metapixelrc \
		metapixel-$(VERSION)/
	tar -zcvf metapixel-$(VERSION).tar.gz metapixel-$(VERSION)
	rm -rf metapixel-$(VERSION)
