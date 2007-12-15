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
MACOS_CCOPTS = -I/sw/include

LDOPTS = $(MACOS_LDOPTS) -L/usr/X11R6/lib $(PROFILE) $(DEBUG)
CCOPTS = $(MACOS_CCOPTS) -I/usr/X11R6/include -I/usr/X11R6/include/X11 -I. -Wall $(OPTIMIZE) $(DEBUG) $(PROFILE) \
	 -DMETAPIXEL_VERSION=\"$(VERSION)\" -DRWIMG_JPEG -DRWIMG_PNG -DCONSOLE_OUTPUT
CC = gcc
#LIBFFM = -lffm

LISPREADER_OBJS = lispreader.o pools.o allocator.o
OBJS = main.o bitmap.o color.o metric.o matcher.o tiling.o metapixel.o library.o classic.o collage.o search.o \
	utils.o error.o avl.o vector.o zoom.o rwpng.o rwjpeg.o readimage.o writeimage.o \
	$(LISPREADER_OBJS) getopt.o getopt1.o
CONVERT_OBJS = convert.o $(LISPREADER_OBJS) getopt.o getopt1.o
IMAGESIZE_OBJS = imagesize.o rwpng.o rwjpeg.o readimage.o

all : metapixel metapixel.1 imagesize
# convert 

metapixel : $(OBJS)
	$(CC) $(LDOPTS) -o metapixel $(OBJS) -lpng -ljpeg $(LIBFFM) -lm -lz

metapixel.1 : metapixel.xml
	xsltproc --nonet $(MANPAGE_XSL) metapixel.xml

convert : $(CONVERT_OBJS)
	$(CC) $(LDOPTS) -o convert $(CONVERT_OBJS)

imagesize : $(IMAGESIZE_OBJS)
	$(CC) $(LDOPTS) -o imagesize $(IMAGESIZE_OBJS) -lpng -ljpeg -lm -lz

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

realclean : clean
	rm -f metapixel.1

dist : metapixel.1
	rm -rf metapixel-$(VERSION)
	mkdir metapixel-$(VERSION)
	cp Makefile README NEWS COPYING *.[ch] metapixel-prepare sizesort metapixel.xml metapixel.1 metapixelrc \
		metapixel-$(VERSION)/
	tar -zcvf metapixel-$(VERSION).tar.gz metapixel-$(VERSION)
	rm -rf metapixel-$(VERSION)
