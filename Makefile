PREFIX = /usr/local
INSTALL = install

BINDIR = $(PREFIX)/bin

VERSION = 0.8

#DEBUG = -g
OPTIMIZE = -O3
#PROFILE = -pg

LDOPTS = -L/sw/lib -L/usr/X11R6/lib $(PROFILE) $(DEBUG)
CCOPTS = -I/sw/include -I/usr/X11R6/include -I/usr/X11R6/include/X11 -I. -Wall $(OPTIMIZE) $(DEBUG) $(PROFILE) -DMETAPIXEL_VERSION=\"$(VERSION)\"
CC = gcc
#LIBFFM = -lffm

OBJS = metapixel.o vector.o rwpng.o rwjpeg.o readimage.o writeimage.o lispreader.o getopt.o getopt1.o
CONVERT_OBJS = convert.o lispreader.o getopt.o getopt1.o
IMAGESIZE_OBJS = imagesize.o rwpng.o rwjpeg.o readimage.o

all : metapixel convert imagesize

metapixel : $(OBJS) libzoom/libzoom.a
	$(CC) $(LDOPTS) -o metapixel $(OBJS) libzoom/libzoom.a -lpng -ljpeg $(LIBFFM) -lm -lz

convert : $(CONVERT_OBJS)
	$(CC) $(LDOPTS) -o convert $(CONVERT_OBJS)

imagesize : $(IMAGESIZE_OBJS)
	$(CC) $(LDOPTS) -o imagesize $(IMAGESIZE_OBJS) libzoom/libzoom.a -lpng -ljpeg -lm -lz

%.o : %.c
	$(CC) $(CCOPTS) -c $<

libzoom/libzoom.a :
	( cd libzoom ; $(MAKE) libzoom.a )

install : metapixel
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) metapixel $(BINDIR)
	$(INSTALL) metapixel-prepare $(BINDIR)

clean :
	( cd libzoom ; $(MAKE) clean )
	rm -f *.o metapixel convert *~

dist :
	rm -rf metapixel-$(VERSION)
	mkdir metapixel-$(VERSION)
	cp Makefile README NEWS COPYING *.[ch] metapixel-prepare sizesort metapixel-$(VERSION)/
	mkdir metapixel-$(VERSION)/libzoom
	cp libzoom/Makefile libzoom/*.[ch] metapixel-$(VERSION)/libzoom/
	tar -zcvf metapixel-$(VERSION).tar.gz metapixel-$(VERSION)
	rm -rf metapixel-$(VERSION)
