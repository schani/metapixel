/*
 * internals.h
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

#ifndef __METAPIXEL_INTERNALS_H__
#define __METAPIXEL_INTERNALS_H__

#include "api.h"

#ifndef MIN
#define MIN(a,b)           ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)           ((a)>(b)?(a):(b))
#endif

#define SQRT_OF_TWO         1.414213562373095048801688724209698078570

#define TABLES_FILENAME "tables.mxt"

#define NUM_CHANNELS        3

/* Wavelet parameters */
#define WAVELET_IMAGE_SIZE           64
#define SQRT_OF_WAVELET_IMAGE_SIZE   8.0
#define WAVELET_IMAGE_PIXELS         (WAVELET_IMAGE_SIZE * WAVELET_IMAGE_SIZE)
#define WAVELET_ROW_LENGTH           (WAVELET_IMAGE_SIZE * NUM_CHANNELS)
#define SIGNIFICANT_WAVELET_COEFFS   40
#define NUM_WAVELET_COEFFS           (NUM_CHANNELS * SIGNIFICANT_WAVELET_COEFFS)
#define NUM_WAVELET_INDEXES          (WAVELET_IMAGE_SIZE * WAVELET_IMAGE_SIZE * NUM_CHANNELS * 2)

/* Subpixel parameters */
#define NUM_SUBPIXEL_ROWS_COLS       5
#define NUM_SUBPIXELS                (NUM_SUBPIXEL_ROWS_COLS * NUM_SUBPIXEL_ROWS_COLS)

typedef struct
{
    int index;
    float coeff;
} coefficient_with_index_t;

typedef unsigned short index_t;

typedef struct
{
    index_t coeffs[NUM_WAVELET_COEFFS];
} wavelet_coefficients_t;

/* Does not initialize coefficients! */
metapixel_t* metapixel_new (const char *name, unsigned int scaled_width, unsigned int scaled_height,
			    float aspect_ratio);

void wavelet_decompose_image (float *image);
void wavelet_find_highest_coeffs (float *image, coefficient_with_index_t highest_coeffs[NUM_WAVELET_COEFFS]);
void wavelet_generate_coeffs (wavelet_coefficients_t *search_coeffs, float sums[NUM_WAVELET_COEFFS],
			      coefficient_with_index_t raw_coeffs[NUM_WAVELET_COEFFS]);

/* Must be called before using any wavelet functions. */
void init_wavelet (void);

/* FIXME: this must go! */
void transform_rgb_to_yiq (float *image, int num_pixels);

#endif
