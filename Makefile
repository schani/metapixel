LDOPTS = -L/usr/X11R6/lib -pg
CCOPTS = -I/usr/X11R6/include -I/usr/X11R6/include/X11 -Wall -O9 -pg
CC = gcc
#LIBFFM = -lffm

metapixel : metapixel.o vector.o rwpng.o getopt.o getopt1.o
	$(CC) $(LDOPTS) -o metapixel metapixel.o vector.o rwpng.o getopt.o getopt1.o -lpng $(LIBFFM) -lm -lMagick -lX11 -lz -lbz2

%.o : %.c
	$(CC) $(CCOPTS) -c $<

clean :
	rm -f *.o metapixel *~
