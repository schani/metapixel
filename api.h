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

#define COLOR_RGB_8             1

typedef struct
{
    int color;

    unsigned int width;		/* in pixels */
    unsigned int height;	/* in pixels */

    unsigned int pixel_stride;	/* in bytes */
    unsigned int row_stride;	/* in bytes */

    unsigned char *data;
} bitmap_t;

/* bitmap_init does not allocate memory.  It simply initializes the
   data structure *bitmap with the values given. */
bitmap_t* bitmap_init (bitmap_t *bitmap, int color, unsigned int width, unsigned int height,
		       unsigned int pixel_stride, unsigned int row_stride, unsigned char *data);

/* For bitmaps which were allocated by the API and must be freed by
   the user. */
void bitmap_free (bitmap_t *bitmap);

typedef struct _library_t library_t;
typedef struct _metapixel_t metapixel_t;

struct _metapixel_t
{
    library_t *library;

    char *name;

    unsigned int width;
    unsigned int height;

    int enabled;		/* always true in this release */

    metapixel_t *next;		/* next in library */
}

struct _library_t
{
    char *path;

    metpixel_t *pixels;
}

typedef struct
{
} tiling_t;

typedef struct
{
} metric_t;

typedef struct
{
} matcher_t;

typedef struct
{
} classic_mosaic_t;

/* library_new and library_open return 0 on failure. */
/* library_new will not create the directory! */
library_t* library_new (const char *path);
library_t* library_open (const char *path);

void library_close (library_t *library);

/* Returns 0 on failure. */
int library_add_metapixel (library_t *library, metapixel_t *metapixel);

metapixel_t* metapixel_new (bitmap_t *bitmap, const char *name,
			    unsigned int scaled_width, unsigned int scaled_height);

/* The returned bitmap must be freed with bitmap_free.  Returns 0 on
   failure. */
bitmap_t* metapixel_get_bitmap (metapixel_t *metapixel);

/* Not functional yet. */
void metapixel_set_enabled (metapixel_t *metapixel, int enabled);

tiling_t* tiling_init_rectangular (tiling_t *tiling, unsigned int small_width, unsigned int small_height);

/* These do not allocate memory for the metric. */
metric_t* metric_init_subpixel (metric_t *metric, float yiq_weights[]);
metric_t* metric_init_wavelet (metric_t *metric, float yiq_weights[]);

/* These do not allocate memory for the matcher. */
matcher_t* matcher_init_local (matcher_t *matcher, metric_t *metric, unsigned int min_distance);
matcher_t* matcher_init_global (matcher_t *matcher, metric_t *metric);

classic_mosaic_t* generate_classic_mosaic (int num_libraries, library_t **libraries,
					   bitmap_t *in_image, float in_image_scale,
					   tiling_t *tiling, matcher_t *matcher);

#endif
