/* pixel.h: pixel data types */

#ifndef PIXEL_HDR
#define PIXEL_HDR

/* $Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/pixel.h,v 1.1 2000/01/04 03:35:21 schani Exp $ */

/*
 * we have pixel data types for various channel types and numbers of channels:
 *	channel types: 1 byte int., 2 byte int., 4 byte int., 4 byte float
 *	number of channels: 1 (monochrome), 3 (rgb), 4 (rgba)
 */

typedef unsigned char			Pixel1;
typedef struct {Pixel1 r, g, b;}	Pixel1_rgb;
typedef struct {Pixel1 r, g, b, a;}	Pixel1_rgba;
#define PIXEL1_MIN 0
#define PIXEL1_MAX 255

typedef short				Pixel2;
typedef struct {Pixel2 r, g, b;}	Pixel2_rgb;
typedef struct {Pixel2 r, g, b, a;}	Pixel2_rgba;
#define PIXEL2_MIN -32768
#define PIXEL2_MAX 32767

typedef long				Pixel4;
typedef struct {Pixel4 r, g, b;}	Pixel4_rgb;
typedef struct {Pixel4 r, g, b, a;}	Pixel4_rgba;

typedef float				Pixelf;
typedef struct {Pixelf r, g, b;}	Pixelf_rgb;
typedef struct {Pixelf r, g, b, a;}	Pixelf_rgba;

#define PIXEL_UNDEFINED -239	/* to flag undefined vbls in various places */

#endif
