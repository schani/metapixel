PREFIX = /usr/local
INSTALL = install

BINDIR = $(PREFIX)/bin

VERSION = 0.4

#PROFILE = -pg

LDOPTS = -L/usr/X11R6/lib $(PROFILE)
CCOPTS = -I/usr/X11R6/include -I/usr/X11R6/include/X11 -Wall -O9 $(PROFILE) -DMETAPIXEL_VERSION=\"$(VERSION)\"
CC = gcc
#LIBFFM = -lffm

OBJS = metapixel.o vector.o rwpng.o rwjpeg.o readimage.o getopt.o getopt1.o

all : metapixel

metapixel : $(OBJS) libzoom/libzoom.a
	$(CC) $(LDOPTS) -o metapixel $(OBJS) libzoom/libzoom.a -lpng -ljpeg $(LIBFFM) -lm -lz

%.o : %.c
	$(CC) $(CCOPTS) -c $<

libzoom/libzoom.a :
	$(MAKE) -C libzoom libzoom.a

install : metapixel
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) metapixel $(BINDIR)
	$(INSTALL) metapixel-prepare $(BINDIR)

clean :
	$(MAKE) -C libzoom clean
	rm -f *.o metapixel *~

dist :
	rm -rf metapixel-$(VERSION)
	mkdir metapixel-$(VERSION)
	cp Makefile README NEWS COPYING *.[ch] metapixel-prepare metapixel-$(VERSION)/
	mkdir metapixel-$(VERSION)/libzoom
	cp libzoom/Makefile libzoom/*.[ch] metapixel-$(VERSION)/libzoom/
	tar -zcvf metapixel-$(VERSION).tar.gz metapixel-$(VERSION)
	rm -rf metapixel-$(VERSION)
