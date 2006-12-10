PREFIX = /usr/local
INSTALL = install
MANPAGE_XSL = /sw/share/xml/xsl/docbook-xsl/manpages/docbook.xsl

BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man

VERSION = 1.0.2

#DEBUG = -g
OPTIMIZE = -O2
#PROFILE = -pg

MACOS_LDOPTS = -L/sw/lib
MACOS_CCOPTS = -I/sw/include

FORMATDEFS = -DRWIMG_JPEG -DRWIMG_PNG -DRWIMG_GIF

LDOPTS = $(MACOS_LDOPTS) -L/usr/X11R6/lib $(PROFILE) $(DEBUG)
CCOPTS = $(MACOS_CCOPTS) -I/usr/X11R6/include -I/usr/X11R6/include/X11 -I. -Irwimg -Wall $(OPTIMIZE) $(DEBUG) $(PROFILE) -DMETAPIXEL_VERSION=\"$(VERSION)\"
CC = gcc
#LIBFFM = -lffm

export CCOPTS CC FORMATDEFS

LISPREADER_OBJS = lispreader.o pools.o allocator.o
OBJS = metapixel.o vector.o zoom.o $(LISPREADER_OBJS) getopt.o getopt1.o
CONVERT_OBJS = convert.o $(LISPREADER_OBJS) getopt.o getopt1.o
IMAGESIZE_OBJS = imagesize.o

all : metapixel metapixel.1 convert metapixel-imagesize

metapixel : $(OBJS) librwimg
	$(CC) $(LDOPTS) -o metapixel $(OBJS) rwimg/librwimg.a -lpng -ljpeg -lgif $(LIBFFM) -lm -lz

metapixel.1 : metapixel.xml
	xsltproc --nonet $(MANPAGE_XSL) metapixel.xml

convert : $(CONVERT_OBJS)
	$(CC) $(LDOPTS) -o convert $(CONVERT_OBJS)

metapixel-imagesize : $(IMAGESIZE_OBJS) librwimg
	$(CC) $(LDOPTS) -o metapixel-imagesize $(IMAGESIZE_OBJS) rwimg/librwimg.a -lpng -ljpeg -lgif -lm -lz

zoom : zoom.c librwimg
	$(CC) -o zoom $(OPTIMIZE) $(PROFILE) $(MACOS_CCOPTS) -DTEST_ZOOM zoom.c $(MACOS_LDOPTS) rwimg/librwimg.a -lpng -ljpeg -lgif -lm -lz

%.o : %.c
	$(CC) $(CCOPTS) $(FORMATDEFS) -c $<

librwimg :
	$(MAKE) -C rwimg

install : metapixel metapixel.1
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) metapixel $(BINDIR)
	$(INSTALL) metapixel-prepare $(BINDIR)
	$(INSTALL) metapixel.1 $(MANDIR)/man1
	$(INSTALL) metapixel-imagesize $(BINDIR)
	$(INSTALL) metapixel-sizesort $(BINDIR)

clean :
	rm -f *.o metapixel convert metapixel-imagesize *~
	$(MAKE) -C rwimg clean

realclean : clean
	rm -f metapixel.1

dist : metapixel.1
	rm -rf metapixel-$(VERSION)
	mkdir metapixel-$(VERSION)
	mkdir metapixel-$(VERSION)/rwimg
	cp Makefile README NEWS COPYING *.[ch] metapixel-prepare metapixel-sizesort \
		metapixel.xml metapixel.1 metapixelrc metapixel.spec \
			metapixel-$(VERSION)/
	cp rwimg/Makefile rwimg/*.[ch] metapixel-$(VERSION)/rwimg/
	tar -zcvf metapixel-$(VERSION).tar.gz metapixel-$(VERSION)
	rm -rf metapixel-$(VERSION)
