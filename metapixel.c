/*
 * metapixel.c
 *
 * metapixel
 *
 * Copyright (C) 1997-2004 Mark Probst
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "api.h"

void
metapixel_complete_subpixel (metapixel_t *pixel)
{
    color_convert_rgb_pixels(pixel->subpixels_hsv, pixel->subpixels_rgb, NUM_SUBPIXELS, COLOR_SPACE_HSV);
    color_convert_rgb_pixels(pixel->subpixels_yiq, pixel->subpixels_rgb, NUM_SUBPIXELS, COLOR_SPACE_YIQ);
}

static void
generate_coefficients (metapixel_t *pixel)
{
    /*
    static float sums[NUM_COEFFS];
    static coefficient_with_index_t raw_coeffs[NUM_COEFFS];
    */

    bitmap_t *scaled_bitmap;

    /* generate wavelet coefficients */
    /*
    if (pixel->width != WAVELET_IMAGE_SIZE || pixel->height != WAVELET_IMAGE_SIZE)
    {
	scaled_bitmap = bitmap_scale(pixel->bitmap, WAVELET_IMAGE_SIZE, WAVELET_IMAGE_SIZE, FILTER_MITCHELL);
	assert(scaled_bitmap != 0);
    }
    else
	scaled_bitmap = pixel->bitmap;

    for (i = 0; i < WAVELET_IMAGE_PIXELS * NUM_CHANNELS; ++i)
	float_image[i] = scaled_bitmap->data[i];

    if (scaled_bitmap != pixel->bitmap)
	bitmap_free(scaled_bitmap);

    transform_rgb_to_yiq(float_image, WAVELET_IMAGE_PIXELS);

    wavelet_decompose_image(float_image);
    wavelet_find_highest_coeffs(float_image, raw_coeffs);
    wavelet_generate_coeffs(&pixel->coeffs, sums, raw_coeffs);

    for (i = 0; i < NUM_CHANNELS; ++i)
	pixel->means[i] = float_image[i];
    */

    /* generate subpixel coefficients */
    if (pixel->width != NUM_SUBPIXEL_ROWS_COLS || pixel->height != NUM_SUBPIXEL_ROWS_COLS)
    {
	scaled_bitmap = bitmap_scale(pixel->bitmap, NUM_SUBPIXEL_ROWS_COLS, NUM_SUBPIXEL_ROWS_COLS, FILTER_MITCHELL);
	assert(scaled_bitmap != 0);
    }
    else
    {
	scaled_bitmap = bitmap_copy(pixel->bitmap);
	assert(scaled_bitmap != 0);
    }

    //bitmap_write(scaled_bitmap, "/tmp/debug.png");

    assert(NUM_SUBPIXELS <= WAVELET_IMAGE_PIXELS);

    assert(scaled_bitmap->color == COLOR_RGB_8);
    assert(scaled_bitmap->pixel_stride == NUM_CHANNELS);
    assert(scaled_bitmap->row_stride == NUM_SUBPIXEL_ROWS_COLS * NUM_CHANNELS);

    memcpy(pixel->subpixels_rgb, scaled_bitmap->data, NUM_SUBPIXELS * 3);
    metapixel_complete_subpixel(pixel);

    bitmap_free(scaled_bitmap);
}

metapixel_t*
metapixel_new (const char *name, unsigned int scaled_width, unsigned int scaled_height,
	       float aspect_ratio)
{
    metapixel_t *metapixel = (metapixel_t*)malloc(sizeof(metapixel_t));

    assert(metapixel != 0);

    memset(metapixel, 0, sizeof(metapixel_t));

    metapixel->name = strdup(name);
    assert(metapixel->name != 0);

    metapixel->width = scaled_width;
    metapixel->height = scaled_height;
    metapixel->aspect_ratio = aspect_ratio;
    metapixel->enabled = 1;
    metapixel->anti_x = metapixel->anti_y = -1;

    return metapixel;
}

metapixel_t*
metapixel_new_from_bitmap (bitmap_t *bitmap, const char *name,
			   unsigned int scaled_width, unsigned int scaled_height)
{
    metapixel_t *metapixel = metapixel_new(name, scaled_width, scaled_height,
					   (float)bitmap->width / (float)bitmap->height);

    assert(metapixel != 0);

    metapixel->bitmap = bitmap_scale(bitmap, scaled_width, scaled_height, FILTER_MITCHELL);
    assert(metapixel->bitmap != 0);

    generate_coefficients(metapixel);

    return metapixel;
}

void
metapixel_free (metapixel_t *metapixel)
{
    free(metapixel->name);
    if (metapixel->filename != 0)
	free(metapixel->filename);
    if (metapixel->bitmap != 0)
	bitmap_free(metapixel->bitmap);
}

bitmap_t*
metapixel_get_bitmap (metapixel_t *metapixel)
{
    if (metapixel->bitmap != 0)
	return bitmap_copy(metapixel->bitmap);
    else
    {
	char *filename;
	bitmap_t *bitmap;

	assert(metapixel->library != 0 && metapixel->filename != 0);

	filename = (char*)malloc(strlen(metapixel->library->path) + 1 + strlen(metapixel->filename) + 1);
	assert(filename != 0);

	strcpy(filename, metapixel->library->path);
	strcat(filename, "/");
	strcat(filename, metapixel->filename);

	bitmap = bitmap_read(filename);

	if (bitmap == 0)
	{
	    error_info_t info = error_make_string_info(filename);

	    free(filename);

	    error_report(ERROR_CANNOT_READ_METAPIXEL_IMAGE, info);

	    return 0;
	}

	free(filename);

	return bitmap;
    }
}

int
metapixel_paste (metapixel_t *pixel, bitmap_t *image, unsigned int x, unsigned int y,
		 unsigned int small_width, unsigned int small_height)
{
    bitmap_t *bitmap;

    bitmap = metapixel_get_bitmap(pixel);
    if (bitmap == 0)
	return 0;

    if (bitmap->width != small_width || bitmap->height != small_height)
    {
	bitmap_t *scaled_bitmap = bitmap_scale(bitmap, small_width, small_height, FILTER_MITCHELL);

	assert(scaled_bitmap != 0);

	bitmap_free(bitmap);
	bitmap = scaled_bitmap;
    }

    bitmap_paste(image, bitmap, x, y);

    bitmap_free(bitmap);

    return 1;
}

void
metapixel_set_enabled (metapixel_t *metapixel, int enabled)
{
    /* FIXME: implement */
}
