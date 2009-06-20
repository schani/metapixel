/*
 * metric.c
 *
 * metapixel
 *
 * Copyright (C) 1997-2009 Mark Probst
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "api.h"

metric_t*
metric_init (metric_t *metric, int kind, int color_space, float weights[])
{
    assert(kind == METRIC_SUBPIXEL);

    metric->kind = kind;
    metric->color_space = color_space;
    memcpy(metric->weights, weights, sizeof(float) * NUM_CHANNELS);

    return metric;
}

void
metric_generate_coeffs_for_subimage (coeffs_union_t *coeffs, bitmap_t *bitmap,
				     int x, int y, int width, int height, metric_t *metric)
{
    if (metric->kind == METRIC_SUBPIXEL)
    {
	bitmap_t *sub_bitmap, *scaled_bitmap;

	sub_bitmap = bitmap_sub(bitmap, x, y, width, height);
	assert(sub_bitmap != 0);

	//bitmap_write(sub_bitmap, "/tmp/sub.png");

	scaled_bitmap = bitmap_scale(sub_bitmap, NUM_SUBPIXEL_ROWS_COLS, NUM_SUBPIXEL_ROWS_COLS, FILTER_MITCHELL);
	assert(scaled_bitmap != 0);

	bitmap_free(sub_bitmap);

	//bitmap_write(scaled_bitmap, "/tmp/scaled.png");

	assert(scaled_bitmap->color == COLOR_RGB_8);
	assert(scaled_bitmap->pixel_stride == NUM_CHANNELS);
	assert(scaled_bitmap->row_stride == NUM_SUBPIXEL_ROWS_COLS * NUM_CHANNELS);

	color_convert_rgb_pixels(coeffs->subpixel.subpixels, scaled_bitmap->data,
				 NUM_SUBPIXELS, metric->color_space);

	bitmap_free(scaled_bitmap);
    }
    else
	assert(0);
}

#define COMPARE_FUNC_NAME   subpixel_compare_no_flip
#define FLIP_X(x)           ((x))
#define FLIP_Y(y)           ((y))
#include "subpixel_compare.h"
#undef COMPARE_FUNC_NAME
#undef FLIP_X
#undef FLIP_Y

#define COMPARE_FUNC_NAME   subpixel_compare_hor_flip
#define FLIP_X(x)           (NUM_SUBPIXEL_ROWS_COLS - 1 - (x))
#define FLIP_Y(y)           ((y))
#include "subpixel_compare.h"
#undef COMPARE_FUNC_NAME
#undef FLIP_X
#undef FLIP_Y

#define COMPARE_FUNC_NAME   subpixel_compare_ver_flip
#define FLIP_X(x)           ((x))
#define FLIP_Y(y)           (NUM_SUBPIXEL_ROWS_COLS - 1 - (y))
#include "subpixel_compare.h"
#undef COMPARE_FUNC_NAME
#undef FLIP_X
#undef FLIP_Y

#define COMPARE_FUNC_NAME   subpixel_compare_hor_ver_flip
#define FLIP_X(x)           (NUM_SUBPIXEL_ROWS_COLS - 1 - (x))
#define FLIP_Y(y)           (NUM_SUBPIXEL_ROWS_COLS - 1 - (y))
#include "subpixel_compare.h"
#undef COMPARE_FUNC_NAME
#undef FLIP_X
#undef FLIP_Y

compare_func_set_t*
metric_compare_func_set_for_metric (metric_t *metric)
{
    if (metric->kind == METRIC_SUBPIXEL)
    {
	static compare_func_set_t set =
	    {
		subpixel_compare_no_flip,
		subpixel_compare_hor_flip,
		subpixel_compare_ver_flip,
		subpixel_compare_hor_ver_flip
	    };

	return &set;
    }
    else
	assert(0);
    return 0;
}
