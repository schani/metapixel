PREFIX = /usr/local
INSTALL = install

BINDIR = $(PREFIX)/bin

#PROFILE = -pg

LDOPTS = -L/usr/X11R6/lib $(PROFILE)
CCOPTS = -I/usr/X11R6/include -I/usr/X11R6/include/X11 -Wall -O9 $(PROFILE)
CC = gcc
#LIBFFM = -lffm

all : metapixel

metapixel : metapixel.o vector.o rwpng.o getopt.o getopt1.o
	$(CC) $(LDOPTS) -o metapixel metapixel.o vector.o rwpng.o getopt.o getopt1.o -lpng $(LIBFFM) -lm -lMagick -lX11 -lz -lbz2

%.o : %.c
	$(CC) $(CCOPTS) -c $<

install : metapixel
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) metapixel $(BINDIR)
	$(INSTALL) metapixel-prepare $(BINDIR)

clean :
	rm -f *.o metapixel *~
