# $Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/Makefile,v 1.2 2000/01/07 00:11:37 schani Exp $

CC = gcc
L = $(DEST)/lib
COPTS = -O9 $(FLOATOPT)
IPATH = -I.
CFLAGS = $(IPATH) $(COPTS)

SRC = window.c pic.c \
	raw.c \
	zoom.c scanline.c filt.c

OBJ = window.o pic.o \
	raw.o \
	zoom.o scanline.o filt.o

.SUFFIXES: .o .s .i .c

.c.i:
	$(CC) $(CFLAGS) -P $*.c
	cat -s <$*.i >temp
	mv temp $*.i

all: libzoom.a

libzoom.a: $(OBJ)
	ar rcu libzoom.a $(OBJ)
	ranlib libzoom.a

clean:
	-rm -f libzoom.a *.o *~

pic.o: pic.h pixel.h
window.o: window.h

raw.o: raw.h pixel.h

zoom.o: zoom.h scanline.h filt.h
scanline.o: scanline.h
filt.o: filt.h
