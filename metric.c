/*
 * metric.c
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "api.h"

metric_t*
metric_init_subpixel (metric_t *metric, float weights[])
{
    metric->kind = METRIC_SUBPIXEL;
    memcpy(metric->weights, weights, sizeof(float) * NUM_CHANNELS);

    return metric;
}

void
metric_generate_coeffs_for_subimage (coeffs_union_t *coeffs, bitmap_t *bitmap,
				     int x, int y, int width, int height, metric_t *metric)
{
    /* FIXME: not reentrant */
    static float *float_image = 0;

    if (metric->kind == METRIC_WAVELET)
    {
	/*
	coefficient_with_index_t raw_coeffs[NUM_COEFFS];
	int i;

	if (float_image == 0)
	    float_image = (float*)malloc(sizeof(float) * IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS);

	if (width != IMAGE_SIZE || height != IMAGE_SIZE)
	{
	    unsigned char *scaled_data;

	    scaled_data = scale_image(image_data, image_width, image_height, x, y, width, height, IMAGE_SIZE, IMAGE_SIZE);
	    assert(scaled_data != 0);

	    for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS; ++i)
		float_image[i] = scaled_data[i];

	    free(scaled_data);
	}
	else
	{
	    int j, channel;

	    for (j = 0; j < IMAGE_SIZE; ++j)
		for (i = 0; i < IMAGE_SIZE; ++i)
		    for (channel = 0; channel < NUM_CHANNELS; ++channel)
			float_image[(j * IMAGE_SIZE + i) * NUM_CHANNELS + channel] =
			    image_data[((y + j) * image_width + (x + i)) * NUM_CHANNELS + channel];
	}

	transform_rgb_to_yiq(float_image, IMAGE_SIZE * IMAGE_SIZE);
	decompose_image(float_image);
	find_highest_coefficients(float_image, raw_coeffs);

	generate_search_coeffs(&coeffs->wavelet.coeffs, coeffs->wavelet.sums, raw_coeffs);

	for (i = 0; i < NUM_CHANNELS; ++i)
	    coeffs->wavelet.means[i] = float_image[i];
	*/
    }
    else if (metric->kind == METRIC_SUBPIXEL)
    {
	bitmap_t *sub_bitmap, *scaled_bitmap;
	int i;
	int channel;

	if (float_image == 0)
	    float_image = (float*)malloc(sizeof(float) * NUM_SUBPIXELS * NUM_CHANNELS);
	assert(float_image != 0);

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

	for (i = 0; i < NUM_SUBPIXELS * NUM_CHANNELS; ++i)
	    float_image[i] = scaled_bitmap->data[i];

	bitmap_free(scaled_bitmap);

	transform_rgb_to_yiq(float_image, NUM_SUBPIXELS);

	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    for (i = 0; i < NUM_SUBPIXELS; ++i)
		coeffs->subpixel.subpixels[channel * NUM_SUBPIXELS + i] = float_image[i * NUM_CHANNELS + channel];
    }
    else if (metric->kind == METRIC_MIPMAP)
    {
    }
    else
	assert(0);
}

/*
static float
wavelet_compare (coeffs_t *coeffs, metapixel_t *pixel, float best_score)
{
    search_coefficients_t *query = &coeffs->wavelet.coeffs;
    float *query_means = coeffs->wavelet.means;
    float *sums = coeffs->wavelet.sums;
    search_coefficients_t *target = &pixel->coeffs;
    float *target_means = pixel->means;
    float score = 0.0;
    int i;
    int j;
    int channel;

    for (channel = 0; channel < NUM_CHANNELS; ++channel)
	score += index_weights[compute_index(0, channel, 0)]
	    * fabs(query_means[channel] - target_means[channel]) * 0.05;

    j = 0;
    for (i = 0; i < NUM_COEFFS; ++i)
    {
	if (score - sums[i] > best_score)
	    return 1e99;

	while (target->coeffs[j] < query->coeffs[i] && j < NUM_COEFFS)
	    ++j;

	if (j >= NUM_COEFFS)
	    break;

	if (target->coeffs[j] > query->coeffs[i])
	    continue;

	score -= index_weights[weight_ordered_index_to_index[target->coeffs[j]]];
    }

    return score;
}
*/

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
    if (metric->kind == METRIC_WAVELET)
	//return wavelet_compare;
	assert(0);
    else if (metric->kind == METRIC_SUBPIXEL)
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
