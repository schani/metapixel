/* -*- c -*- */

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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "getopt.h"

#include "vector.h"
#include "zoom.h"
#include "readimage.h"
#include "writeimage.h"
#include "lispreader.h"

#include "metapixel.h"

#ifndef MIN
#define MIN(a,b)           ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)           ((a)>(b)?(a):(b))
#endif

int index_order[IMAGE_SIZE * IMAGE_SIZE];

static metapixel_t *first_pixel = 0;
static int num_metapixels = 0;

float sqrt_of_two, sqrt_of_image_size;

float weight_factors[NUM_CHANNELS] = { 1.0, 1.0, 1.0 };

float index_weights[NUM_INDEXES];
index_t weight_ordered_index_to_index[NUM_INDEXES];
index_t index_to_weight_ordered_index[NUM_INDEXES];

int small_width = DEFAULT_WIDTH, small_height = DEFAULT_HEIGHT;

int forbid_reconstruction_radius = 0;

static unsigned char*
scale_image (unsigned char *image, int image_width, int image_height, int x, int y,
	     int width, int height, int new_width, int new_height)
{
    unsigned char *new_image = (unsigned char*)malloc(new_width * new_height * NUM_CHANNELS);
    unsigned char *image_start = image + (x + y * image_width) * NUM_CHANNELS;

    zoom_image(new_image, image_start, get_filter(FILTER_MITCHELL), NUM_CHANNELS,
	       new_width, new_height, new_width * NUM_CHANNELS,
	       width, height, image_width * NUM_CHANNELS);

    return new_image;
}

void
alpha_compose (unsigned char *dst, int width, int height, unsigned char *src, int perc)
{
    int i;

    for (i = 0; i < width * height * NUM_CHANNELS; ++i)
	dst[i] = dst[i] * (100 - perc) / 100 + src[i] * perc / 100;
}

void
generate_index_order (void)
{
    int index = 0,
	begin = 0,
	row = 0,
	col = 0,
	first_half = 1;

    do
    {
	index_order[index++] = col + IMAGE_SIZE * row;

	if (first_half)
	{
	    if (row == 0)
	    {
		if (begin == IMAGE_SIZE - 1)
		{
		    first_half = 0;
		    col = begin = 1;
		    row = IMAGE_SIZE - 1;
		}
		else
		{
		    ++begin;
		    row = begin;
		    col = 0;
		}
	    }
	    else
	    {
		++col;
		--row;
	    }
	}
	else
	{
	    if (col == IMAGE_SIZE - 1)
	    {
		++begin;
		col = begin;
		row = IMAGE_SIZE - 1;
	    }
	    else
	    {
		++col;
		--row;
	    }
	}
    } while (index < IMAGE_SIZE * IMAGE_SIZE);
}

static int
compute_index (int real_index, int channel, int sign)
{
    return real_index + (channel + (sign > 0 ? 1 : 0) * NUM_CHANNELS) * IMAGE_SIZE * IMAGE_SIZE;
}

static void
uncompute_index (int index, int *real_index, int *channel, int *sign)
{
    *real_index = index % (IMAGE_SIZE * IMAGE_SIZE);
    *channel = (index / (IMAGE_SIZE * IMAGE_SIZE)) % NUM_CHANNELS;
    *sign = (index / (NUM_CHANNELS * IMAGE_SIZE * IMAGE_SIZE)) ? 1 : -1;
}

void
cut_last_coefficients (float *image, int channel, int howmany)
{
    int i;

    for (i = IMAGE_SIZE * IMAGE_SIZE - howmany; i < IMAGE_SIZE * IMAGE_SIZE; ++i)
	image[index_order[i] * NUM_CHANNELS + channel] = 0.0;
}

void
transform_rgb_to_yiq (float *image, int num_pixels)
{
    const static Matrix3D conversion_matrix =
    {
	{ 0.299, 0.587, 0.114 },
	{ 0.596, -0.275, -0.321 },
	{ 0.212, -0.528, 0.311 }
    };

    int i;

    for (i = 0; i < num_pixels; ++i)
    {
	Vector3D rgb_vec,
	    yiq_vec;

	InitVector3D(&rgb_vec,
		     image[NUM_CHANNELS * i + 0],
		     image[NUM_CHANNELS * i + 1],
		     image[NUM_CHANNELS * i + 2]);
	MultMatrixVector3D(&yiq_vec, &conversion_matrix, &rgb_vec);
	image[NUM_CHANNELS * i + 0] = yiq_vec.x;
	image[NUM_CHANNELS * i + 1] = yiq_vec.y / 1.192 + 127.5;
	image[NUM_CHANNELS * i + 2] = yiq_vec.z / 1.051 + 128.106565176;
    }
}

void
transform_yiq_to_rgb (float *image)
{
    const static Matrix3D conversion_matrix =
    {
	{ 1.00308929854,  0.954849063112,   0.61785970812 },
	{ 0.996776058337, -0.270706233074, -0.644788332692 },
	{ 1.00849783766,   -1.1104851847,   1.69956753125 }
    };

    int i;

    for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE; ++i)
    {
	Vector3D rgb_vec,
	    yiq_vec;

	InitVector3D(&yiq_vec,
		     image[NUM_CHANNELS * i + 0],
		     image[NUM_CHANNELS * i + 1] - 127.5,
		     image[NUM_CHANNELS * i + 2] - 127.5);
	MultMatrixVector3D(&rgb_vec, &conversion_matrix, &yiq_vec);
	image[NUM_CHANNELS * i + 0] = rgb_vec.x;
	image[NUM_CHANNELS * i + 1] = rgb_vec.y;
	image[NUM_CHANNELS * i + 2] = rgb_vec.z;
    }
}

void
transpose_image (float *old_image, float *new_image)
{
    int i,
	j,
	channel;

    for (i = 0; i < IMAGE_SIZE; ++i)
	for (j = 0; j < IMAGE_SIZE; ++j)
	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
		new_image[channel + (i + j * IMAGE_SIZE) * NUM_CHANNELS] =
		    old_image[channel + (j + i * IMAGE_SIZE) * NUM_CHANNELS];
}

void
decompose_row (float *row)
{
    int h = IMAGE_SIZE,
	i;
    float new_row[NUM_CHANNELS * IMAGE_SIZE];

    for (i = 0; i < NUM_CHANNELS * IMAGE_SIZE; ++i)
	row[i] = row[i] / sqrt_of_image_size;

    while (h > 1)
    {
	h = h / 2;
	for (i = 0; i < h; ++i)
	{
	    int channel;

	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    {
		float val1 = row[channel + 2 * i * NUM_CHANNELS],
		    val2 = row[channel + (2 * i + 1) * NUM_CHANNELS];

		new_row[channel + i * NUM_CHANNELS] = (val1 + val2) / sqrt_of_two;
		new_row[channel + (h + i) * NUM_CHANNELS] = (val1 - val2) / sqrt_of_two;
	    }
	}
	memcpy(row, new_row, sizeof(float) * NUM_CHANNELS * h * 2);
    }
}

void
decompose_column (float *column)
{
    int h = IMAGE_SIZE,
	i,
	channel;
    float new_column[ROW_LENGTH];

    for (i = 0; i < IMAGE_SIZE; ++i)
	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    column[channel + i * ROW_LENGTH] =
		column[channel + i * ROW_LENGTH] / sqrt_of_image_size;

    while (h > 1)
    {
	h = h / 2;
	for (i = 0; i < h; ++i)
	{
	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    {
		float val1 = column[channel + (2 * i) * ROW_LENGTH],
		    val2 = column[channel + (2 * i + 1) * ROW_LENGTH];

		new_column[channel + i * NUM_CHANNELS] = (val1 + val2) / sqrt_of_two;
		new_column[channel + (h + i) * NUM_CHANNELS] = (val1 - val2) / sqrt_of_two;
	    }
	}

	for (i = 0; i < h * 2; ++i)
	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
		column[channel + i * ROW_LENGTH] = new_column[channel + i * NUM_CHANNELS];
    }
}

void
decompose_image (float *image)
{
    int row;

    for (row = 0; row < IMAGE_SIZE; ++row)
	decompose_row(image + NUM_CHANNELS * IMAGE_SIZE * row);
    for (row = 0; row < IMAGE_SIZE; ++row)
	decompose_column(image + NUM_CHANNELS * row);
}

void
compose_row (float *row)
{
    int h = 1,
	i;
    float new_row[NUM_CHANNELS * IMAGE_SIZE];

    memcpy(new_row, row, sizeof(float) * NUM_CHANNELS * IMAGE_SIZE);
    while (h < IMAGE_SIZE)
    {
	for (i = 0; i < h; ++i)
	{
	    int channel;

	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    {
		float val1 = row[channel + i * NUM_CHANNELS],
		    val2 = row[channel + (h + i) * NUM_CHANNELS];

		new_row[channel + 2 * i * NUM_CHANNELS] = (val1 + val2) / sqrt_of_two;
		new_row[channel + (2 * i + 1) * NUM_CHANNELS] = (val1 - val2) / sqrt_of_two;
	    }
	}
	memcpy(row, new_row, sizeof(float) * NUM_CHANNELS * IMAGE_SIZE);
	h = h * 2;
    }

    for (i = 0; i < NUM_CHANNELS * IMAGE_SIZE; ++i)
	row[i] = row[i] * sqrt_of_image_size;
}

void
compose_image (float *image)
{
    int row;
    float *transposed_image = (float*)malloc(sizeof(float) * 3 * IMAGE_SIZE * IMAGE_SIZE);

    transpose_image(image, transposed_image);
    for (row = 0; row < IMAGE_SIZE; ++row)
	compose_row(transposed_image + NUM_CHANNELS * IMAGE_SIZE * row);
    transpose_image(transposed_image, image);
    for (row = 0; row < IMAGE_SIZE; ++row)
	compose_row(image + NUM_CHANNELS * IMAGE_SIZE * row);

    free(transposed_image);
}

int
compare_coeffs_with_index (const void *p1, const void *p2)
{
    const coefficient_with_index_t *coeff1 = (const coefficient_with_index_t*)p1;
    const coefficient_with_index_t *coeff2 = (const coefficient_with_index_t*)p2;

    if (fabs(coeff1->coeff) < fabs(coeff2->coeff))
	return 1;
    else if (fabs(coeff1->coeff) == fabs(coeff2->coeff))
	return 0;
    return -1;
}

void
find_highest_coefficients (float *image, coefficient_with_index_t highest_coeffs[NUM_COEFFS])
{
    int index, channel;

    for (channel = 0; channel < NUM_CHANNELS; ++channel)
    {
	for (index = 1; index < SIGNIFICANT_COEFFS + 1; ++index)
	{
	    float coeff = image[channel + NUM_CHANNELS * index];
	    int sign = coeff > 0.0 ? 1 : -1;

	    highest_coeffs[index - 1 + channel * SIGNIFICANT_COEFFS].index = compute_index(index, channel, sign);
	    highest_coeffs[index - 1 + channel * SIGNIFICANT_COEFFS].coeff = coeff;
	}

	qsort(highest_coeffs + channel * SIGNIFICANT_COEFFS, SIGNIFICANT_COEFFS,
	      sizeof(coefficient_with_index_t), compare_coeffs_with_index);
    }

    for (index = SIGNIFICANT_COEFFS + 1; index < IMAGE_SIZE * IMAGE_SIZE; ++index)
    {
	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	{
	    float coeff = image[channel + NUM_CHANNELS * index];

	    if (fabs(coeff) > fabs(highest_coeffs[(channel + 1) * SIGNIFICANT_COEFFS - 1].coeff))
	    {
		int significance;
		int sign = coeff > 0.0 ? 1 : -1;

		for (significance = (channel + 1) * SIGNIFICANT_COEFFS - 2;
		     significance >= channel * SIGNIFICANT_COEFFS;
		     --significance)
		    if (fabs(coeff) <= fabs(highest_coeffs[significance].coeff))
			break;
		++significance;
		memmove(highest_coeffs + significance + 1,
			highest_coeffs + significance,
			sizeof(coefficient_with_index_t) * ((channel + 1) * SIGNIFICANT_COEFFS - 1 - significance));
		highest_coeffs[significance].index = compute_index(index, channel, sign);
		highest_coeffs[significance].coeff = coeff;
	    }
	}
    }
}

static float
weight_function (int index)
{
    static float weight_table[NUM_CHANNELS][6] =
    {
	{ 5.00, 0.83, 1.01, 0.52, 0.47, 0.30 },
	{ 19.21, 1.26, 0.44, 0.53, 0.28, 0.14 },
	{ 34, 0.36, 0.45, 0.14, 0.18, 0.27 }
    };

    int real_index, channel, sign;
    int i, j, bin;

    uncompute_index(index, &real_index, &channel, &sign);

    i = real_index % IMAGE_SIZE;
    j = real_index / IMAGE_SIZE;
    bin = MIN(MAX(i, j), 5);

    return weight_table[channel][bin] * weight_factors[channel];
}

static int
compare_indexes_by_weight_descending (const void *p1, const void *p2)
{
    const index_t *i1 = (const index_t*)p1, *i2 = (const index_t*)p2;

    if (index_weights[*i1] < index_weights[*i2])
	return 1;
    else if (index_weights[*i1] == index_weights[*i2])
	return 0;
    return -1;
}

static void
compute_index_weights (void)
{
    int i;

    for (i = 0; i < NUM_INDEXES; ++i)
	index_weights[i] = weight_function(i);

    for (i = 0; i < NUM_INDEXES; ++i)
	weight_ordered_index_to_index[i] = i;
    qsort(weight_ordered_index_to_index, NUM_INDEXES, sizeof(index_t), compare_indexes_by_weight_descending);
    for (i = 0; i < NUM_INDEXES; ++i)
	index_to_weight_ordered_index[weight_ordered_index_to_index[i]] = i;
}

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

static float
subpixel_compare (coeffs_t *coeffs, metapixel_t *pixel, float best_score)
{
    int channel;
    float score = 0.0;

    for (channel = 0; channel < NUM_CHANNELS; ++channel)
    {
	int i;

	for (i = 0; i < NUM_SUBPIXELS; ++i)
	{
	    float dist = (int)coeffs->subpixel.subpixels[channel * NUM_SUBPIXELS + i]
		- (int)pixel->subpixels[channel * NUM_SUBPIXELS + i];

	    score += dist * dist * weight_factors[channel];

	    if (score >= best_score)
		return 1e99;
	}
    }

    return score;
}

void
add_metapixel (metapixel_t *pixel)
{
    pixel->next = first_pixel;
    first_pixel = pixel;
    ++num_metapixels;
}

static int
compare_indexes (const void *p1, const void *p2)
{
    index_t *i1 = (index_t*)p1;
    index_t *i2 = (index_t*)p2;

    return *i1 - *i2;
}

void
generate_search_coeffs (search_coefficients_t *search_coeffs, float sums[NUM_COEFFS],
			coefficient_with_index_t raw_coeffs[NUM_COEFFS])
{
    int i;
    float sum;

    for (i = 0; i < NUM_COEFFS; ++i)
	search_coeffs->coeffs[i] = index_to_weight_ordered_index[raw_coeffs[i].index];

    qsort(search_coeffs->coeffs, NUM_COEFFS, sizeof(index_t), compare_indexes);

    sum = 0.0;
    for (i = NUM_COEFFS - 1; i >= 0; --i)
    {
	sum += index_weights[weight_ordered_index_to_index[search_coeffs->coeffs[i]]];
	sums[i] = sum;
    }
}

void
generate_search_coeffs_for_subimage (coeffs_t *coeffs, unsigned char *image_data, int image_width, int image_height,
				     int x, int y, int width, int height, int method)
{
    static float *float_image = 0;

    if (method == METHOD_WAVELET)
    {
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
    }
    else if (method == METHOD_SUBPIXEL)
    {
	unsigned char *scaled_data;
	int i;
	int channel;

	if (float_image == 0)
	    float_image = (float*)malloc(sizeof(float) * NUM_SUBPIXELS * NUM_CHANNELS);

	scaled_data = scale_image(image_data, image_width, image_height, x, y, width, height,
				  NUM_SUBPIXEL_ROWS_COLS, NUM_SUBPIXEL_ROWS_COLS);

	for (i = 0; i < NUM_SUBPIXELS * NUM_CHANNELS; ++i)
	    float_image[i] = scaled_data[i];

	free(scaled_data);

	transform_rgb_to_yiq(float_image, NUM_SUBPIXELS);

	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    for (i = 0; i < NUM_SUBPIXELS; ++i)
		coeffs->subpixel.subpixels[channel * NUM_SUBPIXELS + i] = float_image[i * NUM_CHANNELS + channel];
    }
    else
	assert(0);
}

static void
generate_metapixel_coefficients (metapixel_t *pixel, unsigned char *image_data,
				 coefficient_with_index_t raw_coeffs[NUM_COEFFS])
{
    static float float_image[NUM_CHANNELS * IMAGE_SIZE * IMAGE_SIZE];
    static float sums[NUM_COEFFS];

    unsigned char *scaled_data;
    int i, channel;

    /* generate wavelet coefficients */
    if (small_width != IMAGE_SIZE || small_height != IMAGE_SIZE)
    {
	scaled_data = scale_image(image_data, small_width, small_height,
				  0, 0, small_width, small_height, IMAGE_SIZE, IMAGE_SIZE);
	assert(scaled_data != 0);
    }
    else
	scaled_data = image_data;

    for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS; ++i)
	float_image[i] = scaled_data[i];

    if (scaled_data != image_data)
	free(scaled_data);

    transform_rgb_to_yiq(float_image, IMAGE_SIZE * IMAGE_SIZE);
    decompose_image(float_image);

    find_highest_coefficients(float_image, raw_coeffs);

    generate_search_coeffs(&pixel->coeffs, sums, raw_coeffs);

    for (i = 0; i < NUM_CHANNELS; ++i)
	pixel->means[i] = float_image[i];

    /* generate subpixel coefficients */
    if (small_width != NUM_SUBPIXEL_ROWS_COLS || small_height != NUM_SUBPIXEL_ROWS_COLS)
    {
	scaled_data = scale_image(image_data, small_width, small_height, 0, 0, small_width, small_height,
				  NUM_SUBPIXEL_ROWS_COLS, NUM_SUBPIXEL_ROWS_COLS);
	assert(scaled_data != 0);
    }
    else
	scaled_data = image_data;

    assert(NUM_SUBPIXELS <= IMAGE_SIZE * IMAGE_SIZE);

    for (i = 0; i < NUM_SUBPIXELS * NUM_CHANNELS; ++i)
	float_image[i] = scaled_data[i];

    transform_rgb_to_yiq(float_image, NUM_SUBPIXELS);

    for (channel = 0; channel < NUM_CHANNELS; ++channel)
	for (i = 0; i < NUM_SUBPIXELS; ++i)
	    pixel->subpixels[channel * NUM_SUBPIXELS + i] = (int)float_image[i * NUM_CHANNELS + channel];

    if (scaled_data != image_data)
	free(scaled_data);
}

static int
metapixel_in_array (metapixel_t *pixel, metapixel_t **array, int size)
{
    int i;

    for (i = 0; i < size; ++i)
	if (array[i] == pixel)
	    return 1;
    return 0;
}

compare_func_t
compare_func_for_method (int method)
{
    if (method == METHOD_WAVELET)
	return wavelet_compare;
    else if (method == METHOD_SUBPIXEL)
	return subpixel_compare;
    else
	assert(0);
    return 0;
}

static int
manhattan_distance (int x1, int y1, int x2, int y2)
{
    return abs(x1 - x2) + abs(y1 - y2);
}

static match_t
metapixel_nearest_to (coeffs_t *coeffs, int method, int x, int y,
		      metapixel_t **forbidden, int num_forbidden,
		      int (*validity_func) (void*, metapixel_t*, int, int),
		      void *validity_func_data)
{
    float best_score = 1e99;
    metapixel_t *best_fit = 0, *pixel;
    compare_func_t compare_func = compare_func_for_method(method);
    match_t match;

    for (pixel = first_pixel; pixel != 0; pixel = pixel->next)
    {
	float score;

	if (manhattan_distance(x, y, pixel->anti_x, pixel->anti_y)
	    < forbid_reconstruction_radius)
	    continue;

	score = compare_func(coeffs, pixel, best_score);

	if (score < best_score && !metapixel_in_array(pixel, forbidden, num_forbidden)
	    && (validity_func == 0 || validity_func(validity_func_data, pixel, x, y)))
	{
	    best_score = score;
	    best_fit = pixel;
	}
    }

    match.pixel = best_fit;
    match.score = best_score;

    return match;
}

static void
get_n_metapixel_nearest_to (int n, global_match_t *matches, coeffs_t *coeffs, int method)
{
    compare_func_t compare_func = compare_func_for_method(method);
    int i;
    metapixel_t *pixel;

    assert(num_metapixels >= n);

    i = 0;
    for (pixel = first_pixel; pixel != 0; pixel = pixel->next)
    {
	float score = compare_func(coeffs, pixel, (i < n) ? 1e99 : matches[n - 1].score);

	if (i < n || score < matches[n - 1].score)
	{
	    int j, m;

	    m = MIN(i, n);

	    for (j = 0; j < m; ++j)
		if (matches[j].score > score)
		    break;

	    assert(j <= m && j < n);

	    memmove(matches + j + 1, matches + j, sizeof(global_match_t) * (MIN(n, m + 1) - (j + 1)));

	    matches[j].pixel = pixel;
	    matches[j].score = score;
	}

	++i;
    }

    assert(i >= n);
}

static void
paste_metapixel (metapixel_t *pixel, unsigned char *data, int width, int height, int x, int y)
{
    int i;
    int pixel_width, pixel_height;
    unsigned char *pixel_data;

    if (pixel->data != 0)
	pixel_data = pixel->data;
    else
    {
	pixel_data = read_image(pixel->filename, &pixel_width, &pixel_height);

	if (pixel_data == 0)
	{
	    fprintf(stderr, "cannot read metapixel file `%s'\n", pixel->filename);
	    exit(1);
	}

	if (pixel_width != small_width || pixel_height != small_height)
	{
	    unsigned char *scaled_data = scale_image(pixel_data, pixel_width, pixel_height,
						     0, 0, pixel_width, pixel_height,
						     small_width, small_height);

	    free(pixel_data);
	    pixel_data = scaled_data;
	}
    }

    for (i = 0; i < small_height; ++i)
	memcpy(data + NUM_CHANNELS * (x + (y + i) * width),
	       pixel_data + NUM_CHANNELS * i * small_width, NUM_CHANNELS * small_width);

    if (pixel_data != pixel->data)
	free(pixel_data);
}

static classic_reader_t*
init_classic_reader (char *input_name, float scale)
{
    classic_reader_t *reader = (classic_reader_t*)malloc(sizeof(classic_reader_t));

    assert(reader != 0);

    reader->image_reader = open_image_reading(input_name);
    if (reader->image_reader == 0)
    {
	fprintf(stderr, "cannot read image `%s'\n", input_name);
	exit(1);
    }

    reader->in_image_width = reader->image_reader->width;
    reader->in_image_height = reader->image_reader->height;

    reader->out_image_width = (((int)((float)reader->in_image_width * scale) - 1) / small_width + 1) * small_width;
    reader->out_image_height = (((int)((float)reader->in_image_height * scale) - 1) / small_height + 1) * small_height;

    assert(reader->out_image_width % small_width == 0);
    assert(reader->out_image_height % small_height == 0);

    reader->metawidth = reader->out_image_width / small_width;
    reader->metaheight = reader->out_image_height / small_height;

    reader->in_image_data = 0;
    reader->y = 0;

    return reader;
}

static void
read_classic_row (classic_reader_t *reader)
{
    if (reader->in_image_data != 0)
    {
	assert(reader->y > 0);
	free(reader->in_image_data);
    }
    else
	assert(reader->y == 0);

    reader->num_lines = (reader->y + 1) * reader->in_image_height / reader->metaheight
	- reader->y * reader->in_image_height / reader->metaheight;

    reader->in_image_data = (unsigned char*)malloc(reader->num_lines * reader->in_image_width * NUM_CHANNELS);
    assert(reader->in_image_data != 0);

    read_lines(reader->image_reader, reader->in_image_data, reader->num_lines);

    ++reader->y;
}

static void
compute_classic_column_coords (classic_reader_t *reader, int x, int *left_x, int *width)
{
    *left_x = x * reader->in_image_width / reader->metawidth;
    *width = (x + 1) * reader->in_image_width / reader->metawidth - *left_x;
}

static void
generate_search_coeffs_for_classic_subimage (classic_reader_t *reader, int x, coeffs_t *coeffs, int method)
{
    int left_x, width;

    compute_classic_column_coords(reader, x, &left_x, &width);
    generate_search_coeffs_for_subimage(coeffs, reader->in_image_data, reader->in_image_width, reader->num_lines,
					left_x, 0, width, reader->num_lines, method);
}

static void
free_classic_reader (classic_reader_t *reader)
{
    if (reader->in_image_data != 0)
	free(reader->in_image_data);
    free_image_reader(reader->image_reader);
    free(reader);
}

static mosaic_t*
init_mosaic_from_reader (classic_reader_t *reader)
{
    mosaic_t *mosaic = (mosaic_t*)malloc(sizeof(mosaic_t));
    int metawidth = reader->metawidth, metaheight = reader->metaheight;
    int i;

    assert(mosaic != 0);

    mosaic->metawidth = metawidth;
    mosaic->metaheight = metaheight;
    mosaic->matches = (match_t*)malloc(sizeof(match_t) * metawidth * metaheight);

    for (i = 0; i < metawidth * metaheight; ++i)
	mosaic->matches[i].pixel = 0;

    return mosaic;
}

static mosaic_t*
generate_local_classic (classic_reader_t *reader, int min_distance, int method)
{
    mosaic_t *mosaic = init_mosaic_from_reader(reader);
    int metawidth = reader->metawidth, metaheight = reader->metaheight;
    int x, y;
    metapixel_t **neighborhood = 0;
    int neighborhood_diameter = min_distance * 2 + 1;
    int neighborhood_size = (neighborhood_diameter * neighborhood_diameter - 1) / 2;

    if (min_distance > 0)
	neighborhood = (metapixel_t**)malloc(sizeof(metapixel_t*) * neighborhood_size);

    for (y = 0; y < metaheight; ++y)
    {
	read_classic_row(reader);

	for (x = 0; x < metawidth; ++x)
	{
	    match_t match;
	    int i;
	    coeffs_t coeffs;

	    for (i = 0; i < neighborhood_size; ++i)
	    {
		int nx = x + i % neighborhood_diameter - min_distance;
		int ny = y + i / neighborhood_diameter - min_distance;

		if (nx < 0 || nx >= metawidth || ny < 0 || ny >= metaheight)
		    neighborhood[i] = 0;
		else
		    neighborhood[i] = mosaic->matches[ny * metawidth + nx].pixel;
	    }

	    generate_search_coeffs_for_classic_subimage(reader, x, &coeffs, method);

	    match = metapixel_nearest_to(&coeffs, method, x, y, neighborhood, neighborhood_size, 0, 0);

	    mosaic->matches[y * metawidth + x] = match;

	    printf(".");
	    fflush(stdout);
	}
    }

    free(neighborhood);

    printf("\n");

    return mosaic;
}

static int
compare_global_matches (const void *_m1, const void *_m2)
{
    global_match_t *m1 = (global_match_t*)_m1;
    global_match_t *m2 = (global_match_t*)_m2;

    if (m1->score < m2->score)
	return -1;
    if (m1->score > m2->score)
	return 1;
    return 0;
}

static mosaic_t*
generate_global_classic (classic_reader_t *reader, int method)
{
    mosaic_t *mosaic = init_mosaic_from_reader(reader);
    int metawidth = reader->metawidth, metaheight = reader->metaheight;
    int x, y;
    global_match_t *matches, *m;
    int num_matches = (metawidth * metaheight) * (metawidth * metaheight);
    int i, ignore_forbidden;
    int num_locations_filled;
    metapixel_t *pixel;

    if (num_metapixels < metawidth * metaheight)
    {
	fprintf(stderr,
		"global search method needs at least as much\n"
		"metapixels as there are locations\n");
	exit(1);
    }

    matches = (global_match_t*)malloc(sizeof(global_match_t) * num_matches);
    assert(matches != 0);

    m = matches;
    for (y = 0; y < metaheight; ++y)
    {
	read_classic_row(reader);

	for (x = 0; x < metawidth; ++x)
	{
	    coeffs_t coeffs;

	    generate_search_coeffs_for_classic_subimage(reader, x, &coeffs, method);
	    
	    get_n_metapixel_nearest_to(metawidth * metaheight, m, &coeffs, method);
	    for (i = 0; i < metawidth * metaheight; ++i)
	    {
		m[i].x = x;
		m[i].y = y;
	    }

	    m += metawidth * metaheight;

	    printf(".");
	    fflush(stdout);
	}
    }

    qsort(matches, num_matches, sizeof(global_match_t), compare_global_matches);

    for (pixel = first_pixel; pixel != 0; pixel = pixel->next)
	pixel->flag = 0;

    num_locations_filled = 0;
    for (ignore_forbidden = 0; ignore_forbidden < 2; ++ignore_forbidden)
    {
	for (i = 0; i < num_matches; ++i)
	{
	    int index = matches[i].y * metawidth + matches[i].x;

	    if (!ignore_forbidden
		&& manhattan_distance(matches[i].x, matches[i].y,
				      matches[i].pixel->anti_x, matches[i].pixel->anti_y)
		  < forbid_reconstruction_radius)
		continue;

	    if (matches[i].pixel->flag)
		continue;

	    if (num_locations_filled >= metawidth * metaheight)
		break;

	    if (mosaic->matches[index].pixel == 0)
	    {
		if (forbid_reconstruction_radius > 0 && ignore_forbidden)
		{
		    printf("!");
		    fflush(stdout);
		}
		mosaic->matches[index].pixel = matches[i].pixel;
		mosaic->matches[index].score = matches[i].score;

		matches[i].pixel->flag = 1;

		++num_locations_filled;
	    }
	}
	if (forbid_reconstruction_radius == 0)
	    break;
    }
    assert(num_locations_filled == metawidth * metaheight);

    free(matches);

    printf("\n");

    return mosaic;
}

static void
paste_classic (mosaic_t *mosaic, char *input_name, char *output_name, int cheat)
{
    image_reader_t *reader;
    image_writer_t *writer;
    int out_image_width, out_image_height;
    int x, y;
    unsigned char *out_image_data;
    int in_image_width, in_image_height;
    int metawidth = mosaic->metawidth, metaheight = mosaic->metaheight;

    if (cheat > 0)
    {
	reader = open_image_reading(input_name);
	if (reader == 0)
	{
	    fprintf(stderr, "cannot read image `%s'\n", input_name);
	    exit(1);
	}

	in_image_width = reader->width;
	in_image_height = reader->height;
    }
    else
    {
	reader = 0;
	in_image_width = in_image_height = 0;
    }

    out_image_width = mosaic->metawidth * small_width;
    out_image_height = mosaic->metaheight * small_height;

    writer = open_image_writing(output_name, out_image_width, out_image_height, IMAGE_FORMAT_PNG);
    if (writer == 0)
    {
	fprintf(stderr, "cannot write image `%s'\n", output_name);
	exit(1);
    }

    out_image_data = (unsigned char*)malloc(out_image_width * small_height * NUM_CHANNELS);

    for (y = 0; y < metaheight; ++y)
    {
	for (x = 0; x < metawidth; ++x)
	{
	    paste_metapixel(mosaic->matches[y * metawidth + x].pixel, out_image_data, out_image_width,
			    small_height, x * small_width, 0);
	    printf("X");
	    fflush(stdout);
	}

	if (cheat > 0)
	{
	    int num_lines = (y + 1) * in_image_height / metaheight - y * in_image_height / metaheight;
	    unsigned char *in_image_data = (unsigned char*)malloc(num_lines * in_image_width * NUM_CHANNELS);
	    unsigned char *source_data;

	    assert(in_image_data != 0);

	    read_lines(reader, in_image_data, num_lines);

	    if (in_image_width != out_image_width || num_lines != small_height)
		source_data = scale_image(in_image_data, in_image_width, num_lines,
					  0, 0, in_image_width, num_lines,
					  out_image_width, small_height);
	    else
		source_data = in_image_data;

	    alpha_compose(out_image_data, out_image_width, small_height, source_data, cheat);

	    if (source_data != in_image_data)
		free(source_data);

	    free(in_image_data);
	}

	write_lines(writer, out_image_data, small_height);
    }

    free(out_image_data);

    free_image_writer(writer);
    if (cheat > 0)
	free_image_reader(reader);

    printf("\n");
}

static void
pixel_add_collage_position (metapixel_t *pixel, int x, int y)
{
    position_t *position = (position_t*)malloc(sizeof(position_t));

    assert(position != 0);

    position->x = x;
    position->y = y;

    position->next = pixel->collage_positions;
    pixel->collage_positions = position;
}

static int
pixel_valid_for_collage_position (void *data, metapixel_t *pixel, int x, int y)
{
    int min_distance = (int)data;
    position_t *position;

    if (min_distance <= 0)
	return 1;

    for (position = pixel->collage_positions; position != 0; position = position->next)
	if (manhattan_distance(x, y, position->x, position->y) < min_distance)
	    return 0;

    return 1;
}

static void
generate_collage (char *input_name, char *output_name, float scale, int min_distance, int method, int cheat)
{
    unsigned char *in_image_data, *out_image_data;
    int in_image_width, in_image_height;
    char *bitmap;
    int num_pixels_done = 0;

    in_image_data = read_image(input_name, &in_image_width, &in_image_height);
    if (in_image_data == 0)
    {
	fprintf(stderr, "could not read image `%s'\n", input_name);
	exit(1);
    }

    if (scale != 1.0)
    {
	int new_width = (float)in_image_width * scale;
	int new_height = (float)in_image_height * scale;
	unsigned char *scaled_data;

	if (new_width < small_width || new_height < small_height)
	{
	    fprintf(stderr, "source image or scaling factor too small\n");
	    exit(1);
	}

	printf("scaling source image to %dx%d\n", new_width, new_height);

	scaled_data = scale_image(in_image_data, in_image_width, in_image_height,
				  0, 0, in_image_width, in_image_height,
				  new_width, new_height);

	in_image_width = new_width;
	in_image_height = new_height;

	free(in_image_data);
	in_image_data = scaled_data;
    }

    out_image_data = (unsigned char*)malloc(in_image_width * in_image_height * NUM_CHANNELS);

    bitmap = (char*)malloc(in_image_width * in_image_height);
    memset(bitmap, 0, in_image_width * in_image_height);

    while (num_pixels_done < in_image_width * in_image_height)
    {
	int i, j;
	int x, y;
	coeffs_t coeffs;
	match_t match;

	while (1)
	{
	    x = random() % in_image_width - small_width / 2;
	    y = random() % in_image_height - small_height / 2;

	    if (x < 0)
		x = 0;
	    if (x + small_width > in_image_width)
		x = in_image_width - small_width;

	    if (y < 0)
		y = 0;
	    if (y + small_height > in_image_height)
		y = in_image_height - small_height;

	    for (j = 0; j < small_height; ++j)
		for (i = 0; i < small_width; ++i)
		    if (!bitmap[(y + j) * in_image_width + x + i])
			goto out;
	}

    out:
	generate_search_coeffs_for_subimage(&coeffs, in_image_data, in_image_width, in_image_height,
					    x, y, small_width, small_height, method);

	match = metapixel_nearest_to(&coeffs, method, x, y, 0, 0,
				     pixel_valid_for_collage_position, (void*)min_distance);
	paste_metapixel(match.pixel, out_image_data, in_image_width, in_image_height, x, y);

	if (min_distance > 0)
	    pixel_add_collage_position(match.pixel, x, y);

	for (j = 0; j < small_height; ++j)
	    for (i = 0; i < small_width; ++i)
		if (!bitmap[(y + j) * in_image_width + x + i])
		{
		    bitmap[(y + j) * in_image_width + x + i] = 1;
		    ++num_pixels_done;
		}

	printf(".");
	fflush(stdout);
    }

    write_image(output_name, in_image_width, in_image_height, out_image_data, IMAGE_FORMAT_PNG);

    free(bitmap);
    free(out_image_data);
    free(in_image_data);
}

static int
read_tables (const char *filename)
{
    lisp_object_t *pattern;
    lisp_object_t *obj;
    lisp_stream_t stream;
    int num_subs;
    pools_t pools;
    allocator_t allocator;

    if (lisp_stream_init_path(&stream, filename) == 0)
	return 0;

    pattern = lisp_read_from_string("(small-image #?(string) (size #?(integer) #?(integer))"
				    "  (wavelet (means #?(real) #?(real) #?(real)) (coeffs . #?(list)))"
				    "  (subpixel (y . #?(list)) (i . #?(list)) (q . #?(list))))");
    assert(pattern != 0
	   && lisp_type(pattern) != LISP_TYPE_EOF
	   && lisp_type(pattern) != LISP_TYPE_PARSE_ERROR);
    assert(lisp_compile_pattern(&pattern, &num_subs));
    assert(num_subs == 10);

    init_pools(&pools);
    init_pools_allocator(&allocator, &pools);

    for (;;)
    {
        int type;

	reset_pools(&pools);
        obj = lisp_read_with_allocator(&allocator, &stream);
        type = lisp_type(obj);
        if (type != LISP_TYPE_EOF && type != LISP_TYPE_PARSE_ERROR)
        {
            lisp_object_t *vars[10];

            if (lisp_match_pattern(pattern, obj, vars, num_subs))
	    {
		metapixel_t *pixel = (metapixel_t*)malloc(sizeof(metapixel_t));
		coefficient_with_index_t coeffs[NUM_COEFFS];
		lisp_object_t *lst;
		int channel, i;

		pixel->filename = strdup(lisp_string(vars[0]));

		for (channel = 0; channel < NUM_CHANNELS; ++channel)
		    pixel->means[channel] = lisp_real(vars[3 + channel]);

		if (lisp_list_length(vars[6]) != NUM_COEFFS)
		    fprintf(stderr, "wrong number of wavelet coefficients in `%s'\n", pixel->filename);
		else
		{
		    static float sums[NUM_COEFFS];

		    lst = vars[6];
		    for (i = 0; i < NUM_COEFFS; ++i)
		    {
			coeffs[i].index = lisp_integer(lisp_car(lst));
			lst = lisp_cdr(lst);
		    }

		    generate_search_coeffs(&pixel->coeffs, sums, coeffs);
		}

		for (channel = 0; channel < NUM_CHANNELS; ++channel)
		{
		    lst = vars[7 + channel];

		    if (lisp_list_length(lst) != NUM_SUBPIXELS)
			fprintf(stderr, "wrong number of subpixels in `%s'\n", pixel->filename);
		    else
			for (i = 0; i < NUM_SUBPIXELS; ++i)
			{
			    pixel->subpixels[channel * NUM_SUBPIXELS + i] = lisp_integer(lisp_car(lst));
			    lst = lisp_cdr(lst);
			}
		}

		pixel->data = 0;
		pixel->collage_positions = 0;

		add_metapixel(pixel);
	    }
	    else
	    {
		fprintf(stderr, "unknown expression ");
		lisp_dump(obj, stderr);
		fprintf(stderr, "\n");
	    }
        }
        else if (type == LISP_TYPE_PARSE_ERROR)
            fprintf(stderr, "parse error in tables file\n");

        if (type == LISP_TYPE_EOF)
            break;
    }

    lisp_stream_free_path(&stream);

    free_pools(&pools);

    return 1;
}

static metapixel_t*
find_metapixel (char *filename)
{
    metapixel_t *pixel;

    for (pixel = first_pixel; pixel != 0; pixel = pixel->next)
	if (strcmp(pixel->filename, filename) == 0)
	    return pixel;

    return 0;
}

static mosaic_t*
read_protocol (FILE *in)
{
    lisp_object_t *obj;
    lisp_stream_t stream;
    mosaic_t *mosaic = (mosaic_t*)malloc(sizeof(mosaic_t));
    int type;

    lisp_stream_init_file(&stream, in);

    obj = lisp_read(&stream);
    type = lisp_type(obj);
    if (type != LISP_TYPE_EOF && type != LISP_TYPE_PARSE_ERROR)
    {
	lisp_object_t *vars[3];

	if (lisp_match_string("(mosaic (size #?(integer) #?(integer)) (metapixels . #?(list)))",
			      obj, vars))
	{
	    int i;
	    int num_pixels;
	    lisp_object_t *lst;

	    mosaic->metawidth = lisp_integer(vars[0]);
	    mosaic->metaheight = lisp_integer(vars[1]);
	    num_pixels = mosaic->metawidth * mosaic->metaheight;
	    mosaic->matches = (match_t*)malloc(sizeof(match_t) * num_pixels);

	    for (i = 0; i < num_pixels; ++i)
		mosaic->matches[i].pixel = 0;

	    if (lisp_list_length(vars[2]) != num_pixels)
	    {
		fprintf(stderr, "mosaic should have %d metapixels, not %d\n", num_pixels, lisp_list_length(vars[2]));
		exit(1);
	    }

	    lst = vars[2];
	    for (i = 0; i < num_pixels; ++i)
	    {
		lisp_object_t *vars[5];

		if (lisp_match_string("(#?(integer) #?(integer) #?(integer) #?(integer) #?(string))",
				      lisp_car(lst), vars))
		{
		    int x = lisp_integer(vars[0]);
		    int y = lisp_integer(vars[1]);
		    int width = lisp_integer(vars[2]);
		    int height = lisp_integer(vars[3]);
		    metapixel_t *pixel;

		    if (width != 1 || height != 1)
		    {
			fprintf(stderr, "width and height in metapixel must both be 1\n");
			exit(1);
		    }

		    if (mosaic->matches[y * mosaic->metawidth + x].pixel != 0)
		    {
			fprintf(stderr, "location (%d,%d) is assigned to twice\n", x, y);
			exit(1);
		    }

		    pixel = find_metapixel(lisp_string(vars[4]));

		    if (pixel == 0)
		    {
			fprintf(stderr, "could not find metapixel `%s'\n", lisp_string(vars[4]));
			exit(1);
		    }

		    mosaic->matches[y * mosaic->metawidth + x].pixel = pixel;
		}
		else
		{
		    fprintf(stderr, "metapixel ");
		    lisp_dump(lisp_car(lst), stderr);
		    fprintf(stderr, " has wrong format\n");
		    exit(1);
		}

		lst = lisp_cdr(lst);
	    }
	}
	else
	{
	    fprintf(stderr, "malformed expression in protocol file\n");
	    exit(1);
	}
    }
    else
    {
	fprintf(stderr, "error in protocol file\n");
	exit(1);
    }
    lisp_free(obj);

    return mosaic;
}

void
free_mosaic (mosaic_t *mosaic)
{
    free(mosaic->matches);
    free(mosaic);
}

int
make_classic_mosaic (char *in_image_name, char *out_image_name,
		     int method, float scale, int search, int min_distance, int cheat,
		     char *in_protocol_name, char *out_protocol_name)
{
    mosaic_t *mosaic;
    int x, y;

    if (in_protocol_name != 0)
    {
	FILE *protocol_in = fopen(in_protocol_name, "r");

	if (protocol_in == 0)
	{
	    fprintf(stderr, "cannot open protocol file for reading `%s': %s\n", in_protocol_name, strerror(errno));
	    return 0;
	}

	mosaic = read_protocol(protocol_in);

	fclose(protocol_in);
    }
    else
    {
	classic_reader_t *reader = init_classic_reader(in_image_name, scale);

	if (search == SEARCH_LOCAL)
	    mosaic = generate_local_classic(reader, min_distance, method);
	else if (search == SEARCH_GLOBAL)
	    mosaic = generate_global_classic(reader, method);
	else
	    assert(0);

	free_classic_reader(reader);
    }

    if (out_protocol_name != 0)
    {
	FILE *protocol_out = fopen(out_protocol_name, "w");

	if (protocol_out == 0)
	{
	    fprintf(stderr, "cannot open protocol file `%s' for writing: %s\n", out_protocol_name, strerror(errno));
	    /* FIXME: free stuff */
	    return 0;
	}
	else
	{
	    fprintf(protocol_out, "(mosaic (size %d %d)\n(metapixels\n", mosaic->metawidth, mosaic->metaheight);
	    for (y = 0; y < mosaic->metaheight; ++y)
	    {
		for (x = 0; x < mosaic->metawidth; ++x)
		{
		    match_t *match = &mosaic->matches[y * mosaic->metawidth + x];
		    lisp_object_t *obj = lisp_make_string(match->pixel->filename);

		    fprintf(protocol_out, "(%d %d 1 1 ", x, y);
		    lisp_dump(obj, protocol_out);
		    fprintf(protocol_out, ") ; %f\n", match->score);

		    lisp_free(obj);
		}
	    }
	    fprintf(protocol_out, "))\n");
	}

	fclose(protocol_out);
    }

    paste_classic(mosaic, in_image_name, out_image_name, cheat);

    free_mosaic(mosaic);

    return 1;
}

void
usage (void)
{
    printf("Usage:\n"
	   "  metapixel --version\n"
	   "      print out version number\n"
	   "  metapixel --help\n"
	   "      print this help text\n"
	   "  metapixel [option ...] --prepare <inimage> <outimage> <tablefile>\n"
	   "      calculate and output tables for <file>\n"
	   "  metapixel [option ...] --metapixel <in> <out>\n"
	   "      transform <in> to <out>\n"
	   "  metapixel [option ...] --batch <batchfile>\n"
	   "      perform all the tasks in <batchfile>\n"
	   "Options:\n"
	   "  -t, --tables=FILE            read the tables from FILE\n"
	   "  -x, --antimosaic=PIC         use PIC as an antimosaic\n"
	   "  -w, --width=WIDTH            set width for small images\n"
	   "  -h, --height=HEIGHT          set height for small images\n"
	   "  -y, --y-weight=WEIGHT        assign relative weight for the Y-channel\n"
	   "  -i, --i-weight=WEIGHT        assign relative weight for the I-channel\n"
	   "  -q, --q-weight=WEIGHT        assign relative weight for the Q-channel\n"
	   "  -s  --scale=SCALE            scale input image by specified factor\n"
	   "  -m, --metric=METRIC          choose metric (subpixel or wavelet)\n"
	   "  -e, --search=SEARCH          choose search method (local or global)\n"
	   "  -c, --collage                collage mode\n"
	   "  -d, --distance=DIST          minimum distance between two instances of\n"
	   "                               the same constituent image\n"
	   "  -a, --cheat=PERC             cheat with specified percentage\n"
	   "  -f, --forbid-reconstruction=DIST\n"
	   "                               forbid placing antimosaic images on their\n"
	   "                               original locations or locations around it\n"
	   "  --out=FILE                   write protocol to file\n"
	   "  --in=FILE                    read protocol from file and use it\n"
	   "\n"
	   "Report bugs and suggestions to schani@complang.tuwien.ac.at\n");
}

#define MODE_NONE       0
#define MODE_PREPARE    1
#define MODE_METAPIXEL  2
#define MODE_BATCH      3

int
main (int argc, char *argv[])
{
    int mode = MODE_NONE;
    int method = METHOD_SUBPIXEL;
    int search = SEARCH_LOCAL;
    int collage = 0;
    int min_distance = -1;
    int cheat = 0;
    float scale = 1.0;
    char *out_filename = 0;
    char *in_filename = 0;
    char *antimosaic_filename = 0;
    char *tables_filename = 0;

    while (1)
    {
	static struct option long_options[] =
            {
		{ "version", no_argument, 0, 256 },
		{ "help", no_argument, 0, 257 },
		{ "prepare", no_argument, 0, 258 },
		{ "metapixel", no_argument, 0, 259 },
		{ "batch", no_argument, 0, 260 },
		{ "out", required_argument, 0, 261 },
		{ "in", required_argument, 0, 262 },
		{ "tables", required_argument, 0, 't' },
		{ "width", required_argument, 0, 'w' },
		{ "height", required_argument, 0, 'h' },
		{ "y-weight", required_argument, 0, 'y' },
		{ "i-weight", required_argument, 0, 'i' },
		{ "q-weight", required_argument, 0, 'q' },
		{ "scale", required_argument, 0, 's' },
		{ "collage", no_argument, 0, 'c' },
		{ "distance", required_argument, 0, 'd' },
		{ "cheat", required_argument, 0, 'a' },
		{ "metric", required_argument, 0, 'm' },
		{ "search", required_argument, 0, 'e' },
		{ "antimosaic", required_argument, 0, 'x' },
		{ "forbid-reconstruction", required_argument, 0, 'f' },
		{ 0, 0, 0, 0 }
	    };

	int option, option_index;

	option = getopt_long(argc, argv, "t:m:e:w:h:y:i:q:s:cd:a:x:f:", long_options, &option_index);

	if (option == -1)
	    break;

	switch (option)
	{
	    case 258 :
		mode = MODE_PREPARE;
		break;

	    case 259 :
		mode = MODE_METAPIXEL;
		break;

	    case 260 :
		mode = MODE_BATCH;
		break;

	    case 261 :
		if (out_filename != 0)
		{
		    fprintf(stderr, "the --out option can be used at most once\n");
		    return 1;
		}
		out_filename = strdup(optarg);
		assert(out_filename != 0);
		break;

	    case 262 :
		if (in_filename != 0)
		{
		    fprintf(stderr, "the --in option can be used at most once\n");
		    return 1;
		}
		in_filename = strdup(optarg);
		assert(in_filename != 0);
		break;

	    case 'm' :
		if (strcmp(optarg, "wavelet") == 0)
		    method = METHOD_WAVELET;
		else if (strcmp(optarg, "subpixel") == 0)
		    method = METHOD_SUBPIXEL;
		else
		{
		    fprintf(stderr, "metric must either be subpixel or wavelet\n");
		    return 1;
		}
		break;

	    case 'e' :
		if (strcmp(optarg, "local") == 0)
		    search = SEARCH_LOCAL;
		else if (strcmp(optarg, "global") == 0)
		    search = SEARCH_GLOBAL;
		else
		{
		    fprintf(stderr, "search method must either be local or global\n");
		    return 1;
		}
		break;

	    case 'w' :
		small_width = atoi(optarg);
		break;

	    case 'h' :
		small_height = atoi(optarg);
		break;

	    case 'y' :
		weight_factors[0] = atof(optarg);
		break;

	    case 'i' :
		weight_factors[1] = atof(optarg);
		break;

	    case 'q' :
		weight_factors[2] = atof(optarg);
		break;

	    case 's':
		scale = atof(optarg);
		if (scale <= 0.0)
		{
		    fprintf(stderr, "Error: invalid scale factor.\n");
		    return 1;
		}
		break;

	    case 'c' :
		collage = 1;
		break;

	    case 'd' :
		min_distance = atoi(optarg);
		assert(min_distance >= 0);
		break;

	    case 'a' :
		cheat = atoi(optarg);
		assert(cheat >= 0 && cheat <= 100);
		break;

	    case 't' :
		if (tables_filename != 0)
		{
		    fprintf(stderr, "Error: at most one tables file can be specified.\n");
		    return 1;
		}
		else if (antimosaic_filename != 0)
		{
		    fprintf(stderr, "Error: --tables and --antimosaic cannot be used together.\n");
		    return 1;
		}
		tables_filename = strdup(optarg);
		break;

	    case 'x' :
		if (antimosaic_filename != 0)
		{
		    fprintf(stderr, "Error: at most one antimosaic picture can be specified.\n");
		    return 1;
		}
		else if (tables_filename != 0)
		{
		    fprintf(stderr, "Error: --tables and --antimosaic cannot be used together.\n");
		    return 1;
		}
		antimosaic_filename = strdup(optarg);
		break;

	    case 'f' :
		forbid_reconstruction_radius = atoi(optarg) + 1;
		assert(forbid_reconstruction_radius > 0);
		break;

	    case 256 :
		printf("metapixel " METAPIXEL_VERSION "\n"
		       "\n"
		       "Copyright (C) 1997-2004 Mark Probst\n"
		       "\n"
		       "This program is free software; you can redistribute it and/or modify\n"
		       "it under the terms of the GNU General Public License as published by\n"
		       "the Free Software Foundation; either version 2 of the License, or\n"
		       "(at your option) any later version.\n"
		       "\n"
		       "This program is distributed in the hope that it will be useful,\n"
		       "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		       "GNU General Public License for more details.\n"
		       "\n"
		       "You should have received a copy of the GNU General Public License\n"
		       "along with this program; if not, write to the Free Software\n"
		       "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.\n");
		exit(0);

	    case 257 :
		usage();
		return 0;

	    default :
		assert(0);
	}
    }

    if (mode == MODE_METAPIXEL && min_distance < 0)
    {
	if (!collage)
	    min_distance = DEFAULT_CLASSIC_MIN_DISTANCE;
	else
	    min_distance = DEFAULT_COLLAGE_MIN_DISTANCE;
    }

    if (in_filename != 0 || out_filename != 0)
    {
	if (mode != MODE_METAPIXEL)
	{
	    fprintf(stderr, "the --in and --out options can only be used in metapixel mode\n");
	    return 1;
	}
	if (collage)
	{
	    fprintf(stderr, "the --in and --out options can only be used for classic mosaics\n");
	    return 1;
	}
    }

    sqrt_of_two = sqrt(2);
    sqrt_of_image_size = sqrt(IMAGE_SIZE);

    compute_index_weights();

    if (mode == MODE_PREPARE)
    {
	static coefficient_with_index_t highest_coeffs[NUM_COEFFS];

	int i, channel;
	unsigned char *image_data;
	unsigned char *scaled_data;
	char *inimage_name, *outimage_name, *tables_name;
	FILE *tables_file;
	int in_width, in_height;
	metapixel_t pixel;
	lisp_object_t *obj;

	if (argc - optind != 3)
	{
	    usage();
	    return 1;
	}

	inimage_name = argv[optind + 0];
	outimage_name = argv[optind + 1];
	tables_name = argv[optind + 2];

	image_data = read_image(inimage_name, &in_width, &in_height);

	if (image_data == 0)
	{
	    fprintf(stderr, "could not read image `%s'\n", inimage_name);
	    return 1;
	}

	tables_file = fopen(tables_name, "a");
	if (tables_file == 0)
	{
	    fprintf(stderr, "could not open file `%s' for writing\n", tables_name);
	    return 1;
	}

	/* generate small image */
	scaled_data = scale_image(image_data, in_width, in_height, 0, 0,
				  in_width, in_height, small_width, small_height);
	assert(scaled_data != 0);

	write_image(outimage_name, small_width, small_height, scaled_data, IMAGE_FORMAT_PNG);

	generate_metapixel_coefficients(&pixel, scaled_data, highest_coeffs);

	free(scaled_data);

	fprintf(tables_file, "(small-image ");
	obj = lisp_make_string(outimage_name);
	lisp_dump(obj, tables_file);
	lisp_free(obj);

	fprintf(tables_file, " (size %d %d) (wavelet (means %f %f %f) (coeffs",
		small_width, small_height,
		pixel.means[0], pixel.means[1], pixel.means[2]);
	for (i = 0; i < NUM_COEFFS; ++i)
	    fprintf(tables_file, " %d", highest_coeffs[i].index);

	fprintf(tables_file, ")) (subpixel");
	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	{
	    static char *channel_names[] = { "y", "i", "q" };

	    fprintf(tables_file, " (%s", channel_names[channel]);
	    for (i = 0; i < NUM_SUBPIXELS; ++i)
		fprintf(tables_file, " %d", (int)pixel.subpixels[channel * NUM_SUBPIXELS + i]);
	    fprintf(tables_file, ")");
	}
	fprintf(tables_file, "))\n");

	fclose(tables_file);
    }
    else if (mode == MODE_METAPIXEL
	     || mode == MODE_BATCH)
    {
	if ((mode == MODE_METAPIXEL && argc - optind != 2)
	    || (mode == MODE_BATCH && argc - optind != 1))
	{
	    usage();
	    return 1;
	}

	if (antimosaic_filename != 0)
	{
	    classic_reader_t *reader = init_classic_reader(antimosaic_filename, scale);
	    int x, y;

	    for (y = 0; y < reader->metaheight; ++y)
	    {
		read_classic_row(reader);

		for (x = 0; x < reader->metawidth; ++x)
		{
		    static coefficient_with_index_t highest_coeffs[NUM_COEFFS];

		    unsigned char *scaled_data;
		    metapixel_t *pixel = (metapixel_t*)malloc(sizeof(metapixel_t));
		    int left_x, width;

		    compute_classic_column_coords(reader, x, &left_x, &width);
		    scaled_data = scale_image(reader->in_image_data, reader->in_image_width, reader->num_lines,
					      left_x, 0, width, reader->num_lines, small_width, small_height);
		    assert(scaled_data != 0);

		    generate_metapixel_coefficients(pixel, scaled_data, highest_coeffs);

		    pixel->data = scaled_data;
		    pixel->anti_x = x;
		    pixel->anti_y = y;
		    pixel->collage_positions = 0;

		    pixel->filename = (char*)malloc(64);
		    sprintf(pixel->filename, "(%d,%d)", x, y);

		    add_metapixel(pixel);

		    printf(":");
		    fflush(stdout);
		}
	    }

	    printf("\n");

	    free_classic_reader(reader);
	}
	else if (tables_filename != 0)
	{
	    if (!read_tables(tables_filename))
	    {
		fprintf(stderr, "Error: cannot read tables file `%s'.\n", tables_filename);
		return 1;
	    }

	    forbid_reconstruction_radius = 0;
	}
	else
	    assert(0);

	if (mode == MODE_METAPIXEL)
	{
	    if (collage)
		generate_collage(argv[optind], argv[optind + 1], scale, min_distance,
				 method, cheat);
	    else
		make_classic_mosaic(argv[optind], argv[optind + 1],
				    method, scale, search, min_distance, cheat,
				    in_filename, out_filename);
	}
	else if (mode == MODE_BATCH)
	{
	    FILE *in = fopen(argv[optind], "r");
	    lisp_stream_t stream;

	    if (in == 0)
	    {
		fprintf(stderr, "cannot open batch file `%s': %s\n", argv[optind], strerror(errno));
		return 1;
	    }

	    lisp_stream_init_file(&stream, in);

	    for (;;)
	    {
		lisp_object_t *obj = lisp_read(&stream);
		int type = lisp_type(obj);

		if (type != LISP_TYPE_EOF && type != LISP_TYPE_PARSE_ERROR)
		{
		    lisp_object_t *vars[4];

		    if (lisp_match_string("(classic (#?(or image protocol) #?(string)) #?(string) . #?(list))",
					  obj, vars))
		    {
			float this_scale = scale;
			int this_search = search;
			int this_min_distance = min_distance;
			int this_cheat = cheat;
			int this_method = method;
			char *this_prot_in_filename = in_filename;
			char *this_prot_out_filename = out_filename;
			char *this_image_out_filename = lisp_string(vars[2]);
			char *this_image_in_filename = 0;
			lisp_object_t *lst;
			lisp_object_t *var;

			if (strcmp(lisp_symbol(vars[0]), "image") == 0)
			    this_image_in_filename = lisp_string(vars[1]);
			else
			    this_prot_in_filename = lisp_string(vars[1]);

			for (lst = vars[3]; lisp_type(lst) != LISP_TYPE_NIL; lst = lisp_cdr(lst))
			{
			    if (lisp_match_string("(scale #?(real))",
						  lisp_car(lst), &var))
			    {
				float val = lisp_real(var);

				if (val <= 0.0)
				    fprintf(stderr, "scale must be larger than 0\n");
				else
				    this_scale = val;
			    }
			    else if (lisp_match_string("(search #?(or local global))",
						       lisp_car(lst), &var))
			    {
				if (strcmp(lisp_symbol(var), "local") == 0)
				    this_search = SEARCH_LOCAL;
				else
				    this_search = SEARCH_GLOBAL;
			    }
			    else if (lisp_match_string("(min-distance #?(integer))",
						       lisp_car(lst), &var))
			    {
				int val = lisp_integer(var);

				if (val < 0)
				    fprintf(stderr, "min-distance cannot be negative\n");
				else
				    this_min_distance = val;
			    }
			    else if (lisp_match_string("(cheat #?(integer))",
						       lisp_car(lst), &var))
			    {
				int val = lisp_integer(var);

				if (val < 0 || val > 100)
				    fprintf(stderr, "cheat must be between 0 and 100, inclusively\n");
				else
				    this_cheat = val;
				    
			    }
			    else if (lisp_match_string("(method #?(or subpixel wavelet))",
						       lisp_car(lst), &var))
			    {
				if (strcmp(lisp_symbol(var), "subpixel") == 0)
				    this_method = METHOD_SUBPIXEL;
				else
				    this_method = METHOD_WAVELET;
			    }
			    else if (lisp_match_string("(protocol #?(string))",
						       lisp_car(lst), &var))
				this_prot_out_filename = lisp_string(var);
			    else
			    {
				fprintf(stderr, "unknown expression ");
				lisp_dump(lisp_car(lst), stderr);
				fprintf(stderr, "\n");
			    }
			}

			make_classic_mosaic(this_image_in_filename, this_image_out_filename,
					    this_method, this_scale, this_search, this_min_distance, this_cheat,
					    this_prot_in_filename, this_prot_out_filename);
		    }
		    else
		    {
			fprintf(stderr, "unknown expression ");
			lisp_dump(obj, stderr);
			fprintf(stderr, "\n");
		    }
		}
		else if (type == LISP_TYPE_PARSE_ERROR)
		    fprintf(stderr, "parse error in batch file\n");
		lisp_free(obj);

		if (type == LISP_TYPE_EOF)
		    break;
	    }
	}
	else
	    assert(0);
    }
    else
    {
	usage();
	return 1;
    }

    return 0;
}
