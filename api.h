/*
 * api.h
 *
 * metapixel
 *
 * Copyright (C) 2004 Mark Probst
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __METAPIXEL_API_H__
#define __METAPIXEL_API_H__

#include "zoom.h"

typedef struct _library_t library_t;
typedef struct _metapixel_t metapixel_t;
typedef struct _bitmap_t bitmap_t;
typedef struct _metric_t metric_t;

#include "internals.h"

#define COLOR_RGB_8             1

struct _bitmap_t
{
    int color;

    unsigned int width;		/* in pixels */
    unsigned int height;	/* in pixels */

    unsigned int pixel_stride;	/* in bytes */
    unsigned int row_stride;	/* in bytes */

    unsigned char *data;

    int refcount;
    bitmap_t *super;
};

/* bitmap_new takes possession of data.  It is freed when the bitmap
   is freed. */
bitmap_t* bitmap_new (int color, unsigned int width, unsigned int height,
		      unsigned int pixel_stride, unsigned int row_stride, unsigned char *data);
/* This is the same as bitmap_new, only that pixel_stride and
   row_stride are assumed to take on their minimal values. */
bitmap_t* bitmap_new_packed (int color, unsigned int width, unsigned int height,
			     unsigned char *data);
/* This allocated the bitmap data itself, which will be
   uninitialized. */
bitmap_t* bitmap_new_empty (int color, unsigned int width, unsigned int height);


/* Only supports whatever read_image supports (i.e., JPEG and PNG). */
bitmap_t* bitmap_read (const char *filename);

bitmap_t* bitmap_copy (bitmap_t *bitmap);
bitmap_t* bitmap_sub (bitmap_t *super, unsigned int x, unsigned int y,
		      unsigned int width, unsigned int height);
bitmap_t* bitmap_scale (bitmap_t *orig, unsigned int scaled_width, unsigned int scaled_height,
			int filter);

void bitmap_free (bitmap_t *bitmap);

int bitmap_write (bitmap_t *bitmap, const char *filename);

void bitmap_paste (bitmap_t *dst, bitmap_t *src, unsigned int x, unsigned int y);
/* Opacity is 0 for full transparency and 0x10000 (65536) for full opacity. */
void bitmap_alpha_compose (bitmap_t *dst, bitmap_t *src, unsigned int opacity);

#ifndef CLIENT_METAPIXEL_DATA_T
#define CLIENT_METAPIXEL_DATA_T void
#endif

struct _metapixel_t
{
    library_t *library;

    char *name;
    char *filename;		/* only used internally */

    unsigned int width;
    unsigned int height;

    float aspect_ratio;		/* aspect ratio of the original
				   (unscaled) image, given by
				   width/height */

    int enabled;		/* always true in this release */

    /* these three are very internal */
    /*
    wavelet_coefficients_t coeffs;
    float means[NUM_CHANNELS];
    */
    unsigned char subpixels[NUM_SUBPIXELS * NUM_CHANNELS];

    bitmap_t *bitmap;		/* this is != 0 iff library == 0 */

    metapixel_t *next;		/* next in library */

    /* the following is only used by the command line metapixel and
       will be removed when that has been rewritten. */
    CLIENT_METAPIXEL_DATA_T *client_data;
};

struct _library_t
{
    char *path;

    metapixel_t *metapixels;
    unsigned int num_metapixels;
};

typedef struct
{
} tiling_t;

struct _metric_t
{
    int kind;
    float weights[NUM_CHANNELS];
};

typedef struct
{
} matcher_t;

typedef struct
{
} classic_mosaic_t;

/* library_new and library_open return 0 on failure. */
/* library_new will not create the directory! */
/* library_open_without_reading opens a library but doesn't read
   the tables file. */
library_t* library_new (const char *path);
library_t* library_open (const char *path);
library_t* library_open_without_reading (const char *path);

void library_close (library_t *library);

/* Copies the metapixel data structure and adds the copy to the
   library.  Returns 0 on failure. */
int library_add_metapixel (library_t *library, metapixel_t *metapixel);

metapixel_t* metapixel_new_from_bitmap (bitmap_t *bitmap, const char *name,
					unsigned int scaled_width, unsigned int scaled_height);
void metapixel_free (metapixel_t *metapixel);

/* The returned bitmap must be freed with bitmap_free.  Returns 0 on
   failure. */
bitmap_t* metapixel_get_bitmap (metapixel_t *metapixel);

/* Not functional yet. */
void metapixel_set_enabled (metapixel_t *metapixel, int enabled);

tiling_t* tiling_init_rectangular (tiling_t *tiling, unsigned int small_width, unsigned int small_height);

/* These do not allocate memory for the metric. */
metric_t* metric_init_subpixel (metric_t *metric, float weights[]);
metric_t* metric_init_wavelet (metric_t *metric, float weights[]);

/* These do not allocate memory for the matcher. */
matcher_t* matcher_init_local (matcher_t *matcher, metric_t *metric, unsigned int min_distance);
matcher_t* matcher_init_global (matcher_t *matcher, metric_t *metric);

classic_mosaic_t* generate_classic_mosaic (int num_libraries, library_t **libraries,
					   bitmap_t *in_image, float in_image_scale,
					   tiling_t *tiling, matcher_t *matcher);

/* Cheat must be in the range from 0 (full transparency, i.e., no
   cheating) to 0x10000 (full opacity). */
bitmap_t* make_collage_mosaic (int num_libraries, library_t **libraries, bitmap_t *in_image, float in_image_scale,
			       unsigned int small_width, unsigned int small_height,
			       int min_distance, metric_t *metric, unsigned int cheat);

#endif
