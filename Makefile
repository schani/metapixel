PREFIX = /usr/local
INSTALL = install

BINDIR = $(PREFIX)/bin

VERSION = 0.10

#DEBUG = -g
OPTIMIZE = -O2
#PROFILE = -pg

MACOS_LDOPTS = -L/sw/lib
MACOS_CCOPTS = -I/sw/include

LDOPTS = $(MACOS_LDOPTS) -L/usr/X11R6/lib $(PROFILE) $(DEBUG)
CCOPTS = $(MACOS_CCOPTS) -I/usr/X11R6/include -I/usr/X11R6/include/X11 -I. -Wall $(OPTIMIZE) $(DEBUG) $(PROFILE) -DMETAPIXEL_VERSION=\"$(VERSION)\"
CC = gcc
#LIBFFM = -lffm

OBJS = metapixel.o vector.o zoom.o rwpng.o rwjpeg.o readimage.o writeimage.o lispreader.o getopt.o getopt1.o
CONVERT_OBJS = convert.o lispreader.o getopt.o getopt1.o
IMAGESIZE_OBJS = imagesize.o rwpng.o rwjpeg.o readimage.o

all : metapixel convert imagesize

metapixel : $(OBJS)
	$(CC) $(LDOPTS) -o metapixel $(OBJS) -lpng -ljpeg $(LIBFFM) -lm -lz

convert : $(CONVERT_OBJS)
	$(CC) $(LDOPTS) -o convert $(CONVERT_OBJS)

imagesize : $(IMAGESIZE_OBJS)
	$(CC) $(LDOPTS) -o imagesize $(IMAGESIZE_OBJS) -lpng -ljpeg -lm -lz

zoom : zoom.c rwjpeg.c rwpng.c readimage.c writeimage.c
	$(CC) -o zoom $(OPTIMIZE) $(PROFILE) $(MACOS_CCOPTS) -DTEST_ZOOM zoom.c rwjpeg.c rwpng.c readimage.c writeimage.c $(MACOS_LDOPTS) -lpng -ljpeg -lm -lz

%.o : %.c
	$(CC) $(CCOPTS) -c $<

install : metapixel
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) metapixel $(BINDIR)
	$(INSTALL) metapixel-prepare $(BINDIR)
#	$(INSTALL) imagesize $(BINDIR)
#	$(INSTALL) sizesort $(BINDIR)

clean :
	rm -f *.o metapixel convert imagesize *~

dist :
	rm -rf metapixel-$(VERSION)
	mkdir metapixel-$(VERSION)
	cp Makefile README NEWS COPYING *.[ch] metapixel-prepare sizesort metapixel-$(VERSION)/
	tar -zcvf metapixel-$(VERSION).tar.gz metapixel-$(VERSION)
	rm -rf metapixel-$(VERSION)
