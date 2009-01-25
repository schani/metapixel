/*
 * api.h
 *
 * metapixel
 *
 * Copyright (C) 2004-2009 Mark Probst
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

#include <stdio.h>

#include "error.h"
#include "zoom.h"

typedef struct _library_t library_t;
typedef struct _metapixel_t metapixel_t;
typedef struct _bitmap_t bitmap_t;
typedef struct _metric_t metric_t;
typedef struct _matcher_t matcher_t;
typedef struct _tiling_t tiling_t;

#define FLIP_HOR               1
#define FLIP_VER               2

typedef struct
{
    metapixel_t *pixel;
    unsigned int orientation;	/* FLIP_* flags */
    unsigned int pixel_index;	/* used internally */
    float score;
} metapixel_match_t;

struct _tiling_t
{
    int kind;
    unsigned int metawidth, metaheight;
};

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

    int refcount;		/* if negative, the data doesn't
				   belong to the bitmap and must not
				   be freed */
    bitmap_t *super;
};

/* bitmap_new takes possession of data.  It is freed when the bitmap
   is freed. */
bitmap_t* bitmap_new (int color, unsigned int width, unsigned int height,
		      unsigned int pixel_stride, unsigned int row_stride, unsigned char *data);
/* bitmap_new_copying doesn't take possession of data. */
bitmap_t* bitmap_new_copying (int color, unsigned int width, unsigned int height,
			      unsigned int pixel_stride, unsigned int row_stride, unsigned char *data);
/* bitmap_new_dont_possess doesn't take possession of data, but
   doesn't copy it either, hence the bitmap must be freed before the
   data is. */
bitmap_t* bitmap_new_dont_possess (int color, unsigned int width, unsigned int height,
				   unsigned int pixel_stride, unsigned int row_stride, unsigned char *data);
/* This is the same as bitmap_new, only that pixel_stride and
   row_stride are assumed to take on their minimal values. */
bitmap_t* bitmap_new_packed (int color, unsigned int width, unsigned int height,
			     unsigned char *data);
/* This allocates the bitmap data itself, which will be
   uninitialized. */
bitmap_t* bitmap_new_empty (int color, unsigned int width, unsigned int height);


/* Only supports whatever read_image supports (i.e., JPEG and PNG). */
bitmap_t* bitmap_read (const char *filename);

bitmap_t* bitmap_copy (bitmap_t *bitmap);
bitmap_t* bitmap_sub (bitmap_t *super, unsigned int x, unsigned int y,
		      unsigned int width, unsigned int height);
bitmap_t* bitmap_scale (bitmap_t *orig, unsigned int scaled_width, unsigned int scaled_height,
			int filter);
bitmap_t* bitmap_flip (bitmap_t *orig, unsigned int flip);

void bitmap_free (bitmap_t *bitmap);

int bitmap_write (bitmap_t *bitmap, const char *filename);

void bitmap_paste (bitmap_t *dst, bitmap_t *src, unsigned int x, unsigned int y);
/* Opacity is 0 for full transparency and 0x10000 (65536) for full opacity. */
void bitmap_alpha_compose (bitmap_t *dst, bitmap_t *src, unsigned int opacity);

struct _metapixel_t
{
    library_t *library;

    char *name;

    /* Only used internally.  Can be zero (for mem libraries). */
    char *filename;

    unsigned int width;
    unsigned int height;

    /* How are we allowed to flip this metapixel? */
    unsigned int flip;

    /* Aspect ratio of the original (unscaled) image, given by
       width/height */
    float aspect_ratio;		

    /* These are the positions (in metapixel-coordinates) of the
       metapixel in the original image if the metapixel is from an
       antimosaic.  Otherwise, both are negative. */
    int anti_x, anti_y;		

    int enabled;		/* Always true in this release */

    /* these three are very internal */
    /*
    wavelet_coefficients_t coeffs;
    float means[NUM_CHANNELS];
    */
    unsigned char subpixels_rgb[NUM_SUBPIXELS * NUM_CHANNELS];
    unsigned char subpixels_hsv[NUM_SUBPIXELS * NUM_CHANNELS];
    unsigned char subpixels_yiq[NUM_SUBPIXELS * NUM_CHANNELS];

    /* This is != 0 iff library == 0 || filename == 0, i.e., for
       metapixels which are not in a library or only in a mem
       library. */
    bitmap_t *bitmap;

    metapixel_t *next;		/* next in library */
};

struct _library_t
{
    char *path;

    metapixel_t *metapixels;
    unsigned int num_metapixels;
};

typedef struct
{
    tiling_t tiling;
    metapixel_match_t *matches;
} classic_mosaic_t;

typedef struct
{
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
    metapixel_match_t match;
} collage_match_t;

typedef struct
{
    unsigned int in_image_width;
    unsigned int in_image_height;

    unsigned int num_matches;
    collage_match_t *matches;
} collage_mosaic_t;

/* value will be in the range 0.0 to 1.0 */
typedef void (*progress_report_func_t) (float value);

/* library_new and library_open return 0 on failure. */
/* library_new will not create the directory! */
library_t* library_new (const char *path);
library_t* library_open (const char *path);
/* library_open_without_reading opens a library but doesn't read
   the tables file.  Returns 0 on failure. */
library_t* library_open_without_reading (const char *path);

/* Creates a library which is not (yet) saved to disk.  Of course, the
   small images take up memory. */
library_t* library_new_mem (void);

/* Saves a mem library or copies an external library.  Modifies the
   library data structure to accomodate for the change.  Returns 0 on
   failure. */
int library_save (library_t *library, const char *path);

void library_close (library_t *library);

/* Copies the metapixel data structure and adds the copy to the
   library.  Returns the copied metapixel or 0 on failure. */
metapixel_t* library_add_metapixel (library_t *library, metapixel_t *metapixel);

metapixel_t* metapixel_new_from_bitmap (bitmap_t *bitmap, const char *name,
					unsigned int scaled_width, unsigned int scaled_height);
void metapixel_free (metapixel_t *metapixel);

/* The returned bitmap must be freed with bitmap_free.  Returns 0 on
   failure. */
bitmap_t* metapixel_get_bitmap (metapixel_t *metapixel);

/* Not functional yet. */
void metapixel_set_enabled (metapixel_t *metapixel, int enabled);

/* metawidth and metaheight are the number of metapixels across the
   width and height of the image, respectively. */
tiling_t* tiling_init_rectangular (tiling_t *tiling, unsigned int metawidth, unsigned int metaheight);

void tiling_get_metapixel_coords (tiling_t *tiling, unsigned int image_width, unsigned int image_height,
				  unsigned int metapixel_x, unsigned int metapixel_y,
				  unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height);

#define METRIC_WAVELET   1
#define METRIC_SUBPIXEL  2

#define COLOR_SPACE_RGB        1
#define COLOR_SPACE_HSV        2
#define COLOR_SPACE_YIQ        3

/* These do not allocate memory for the metric. */
metric_t* metric_init (metric_t *metric, int kind, int color_space, float weights[]);

/* These do not allocate memory for the matcher. */
matcher_t* matcher_init_local (matcher_t *matcher, metric_t *metric, unsigned int min_distance);
matcher_t* matcher_init_global (matcher_t *matcher, metric_t *metric);

classic_reader_t* classic_reader_new_from_file (const char *image_filename, tiling_t *tiling);
classic_reader_t* classic_reader_new_from_bitmap (bitmap_t *bitmap, tiling_t *tiling);
void classic_reader_free (classic_reader_t *reader);

classic_writer_t* classic_writer_new_for_file (const char *filename, unsigned int width, unsigned int height);
classic_writer_t* classic_writer_new_for_bitmap (bitmap_t *bitmap);
void classic_writer_free (classic_writer_t *writer);

/* forbid_reconstruction_radius is only relevant for antimosaics.  If
   the mosaic to be generated is not from an antimosaic, it must be
   0. */
classic_mosaic_t* classic_generate (int num_libraries, library_t **libraries,
				    classic_reader_t *reader, matcher_t *matcher,
				    unsigned int forbid_reconstruction_radius,
				    unsigned int allowed_flips,
				    progress_report_func_t report_func);
classic_mosaic_t* classic_generate_from_bitmap (int num_libraries, library_t **libraries,
						bitmap_t *in_image, tiling_t *tiling, matcher_t *matcher,
						unsigned int forbid_reconstruction_radius,
						unsigned int allowed_flips,
						progress_report_func_t report_func);
collage_mosaic_t* collage_generate_from_bitmap (int num_libraries, library_t **libraries, bitmap_t *in_bitmap,
						unsigned int min_small_width, unsigned int min_small_height,
						unsigned int max_small_width, unsigned int max_small_height,
						unsigned int min_distance, metric_t *metric,
						unsigned int allowed_flips,
						progress_report_func_t report_func);

/* If some metapixel in the mosaic isn't in one of the supplied
   libraries, classic_read tries to open the library.  If
   *num_new_libraries is >0 after classic_read returns, then each
   library in *new_libraries and the *new_libraries array itself
   belongs to the caller (libraries must be freed with library_free,
   the array with free. */
classic_mosaic_t* classic_read (int num_libraries, library_t **libraries, const char *filename,
				int *num_new_libraries, library_t ***new_libraries);
collage_mosaic_t* collage_read (int num_libraries, library_t **libraries, const char *filename,
				int *num_new_libraries, library_t ***new_libraries);

/* Each metapixel in the mosaic must be in a (saved) library.  Returns
   0 on failure. */
int classic_write (classic_mosaic_t *mosaic, FILE *out);
int collage_write (collage_mosaic_t *mosaic, FILE *out);

void classic_free (classic_mosaic_t *mosaic);
void collage_free (collage_mosaic_t *mosaic);

/* cheat must be in the range from 0 (full transparency, i.e., no
   cheating) to 0x10000 (full opacity).  If cheat == 0, then
   reader/in_image can be 0. */
int classic_paste (classic_mosaic_t *mosaic, classic_reader_t *reader, unsigned int cheat,
		   classic_writer_t *writer, progress_report_func_t report_func);
/* width and height are the width and height of the resulting bitmap. */
bitmap_t* classic_paste_to_bitmap (classic_mosaic_t *mosaic, unsigned int width, unsigned int height,
				   bitmap_t *in_image, unsigned int cheat, progress_report_func_t report_func);
bitmap_t* collage_paste_to_bitmap (collage_mosaic_t *mosaic, unsigned int width, unsigned int height,
				   bitmap_t *in_image, unsigned int cheat, progress_report_func_t report_func);

#endif
