/* -*- c -*- */

/*
 * metapixel.c
 *
 * metapixel
 *
 * Copyright (C) 1997-2000 Mark Probst
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

#include "getopt.h"

#include "vector.h"
#include "readimage.h"
#include "writeimage.h"
#include "libzoom/raw.h"
#include "libzoom/zoom.h"

#ifndef MIN
#define MIN(a,b)           ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)           ((a)>(b)?(a):(b))
#endif

#define DEFAULT_WIDTH       64
#define DEFAULT_HEIGHT      64

#define NUM_CHANNELS        3

#define IMAGE_SIZE          64
#define ROW_LENGTH          (IMAGE_SIZE * NUM_CHANNELS)
#define SIGNIFICANT_COEFFS  40

#define NUM_COEFFS          (NUM_CHANNELS * SIGNIFICANT_COEFFS)

#define NUM_INDEXES         (IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS * 2)

#define NUM_SUBPIXEL_ROWS_COLS       5
#define NUM_SUBPIXELS                (NUM_SUBPIXEL_ROWS_COLS * NUM_SUBPIXEL_ROWS_COLS)

#define METHOD_WAVELET  1
#define METHOD_SUBPIXEL 2

typedef struct
{
    int index;
    float coeff;
} coefficient_with_index_t;

typedef unsigned short index_t;

typedef struct
{
    index_t coeffs[NUM_COEFFS];
} search_coefficients_t;

int index_order[IMAGE_SIZE * IMAGE_SIZE];

typedef struct _metapixel_t
{
    char filename[1024];	/* FIXME: should be done dynamically */
    search_coefficients_t coeffs;
    float means[NUM_CHANNELS];
    unsigned char subpixels[NUM_SUBPIXELS * NUM_CHANNELS];
    struct _metapixel_t *next;
} metapixel_t;

typedef union
{
    struct
    {
	search_coefficients_t coeffs;
	float means[NUM_CHANNELS];
	float sums[NUM_COEFFS];
    } wavelet;
    struct
    {
	unsigned char subpixels[NUM_SUBPIXELS * NUM_CHANNELS];
    } subpixel;
} coeffs_t;

typedef float(*compare_func_t)(coeffs_t*, metapixel_t*, float);

static metapixel_t *first_pixel = 0;

float sqrt_of_two, sqrt_of_image_size;

float weight_factors[NUM_CHANNELS] = { 1.0, 1.0, 1.0 };

float index_weights[NUM_INDEXES];
index_t weight_ordered_index_to_index[NUM_INDEXES];
index_t index_to_weight_ordered_index[NUM_INDEXES];

int small_width = DEFAULT_WIDTH, small_height = DEFAULT_HEIGHT;

static unsigned char*
scale_image (unsigned char *image, int image_width, int image_height, int x, int y, int width, int height, int new_width, int new_height)
{
    unsigned char *new_image = (unsigned char*)malloc(new_width * new_height * NUM_CHANNELS);
    Pic pic, new_pic;
    Window_box window, new_window;
    Filt *filt;

    pic_create_raw(&pic, image, image_width, image_height);
    pic_create_raw(&new_pic, new_image, new_width, new_height);

    window_set(x, y, x + width - 1, y + height - 1, (Window*)&window);
    window_box_set_size(&window);

    window_set(0, 0, new_width - 1, new_height - 1, (Window*)&new_window);
    window_box_set_size(&new_window);

    filt = filt_find("mitchell");
    assert(filt != 0);

    zoom(&pic, &window, &new_pic, &new_window, filt, filt);

    return ((raw_pic_t*)new_pic.data)->data;

    /*
    int i, j;

    for (j = 0; j < new_height; ++j)
	for (i = 0; i < new_width; ++i)
	{
	    int old_x = i * width / new_width;
	    int old_y = j * height / new_height;
	    int channel;

	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
		new_image[(j * new_width + i) * NUM_CHANNELS + channel] =
		    image[((y + old_y) * image_width + (x + old_x)) * NUM_CHANNELS + channel];
	}

    return new_image;
    */
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
    static Matrix3D conversion_matrix =
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
	MultMatrixVector3D(&yiq_vec, (const Matrix3D*)&conversion_matrix, &rgb_vec);
	image[NUM_CHANNELS * i + 0] = yiq_vec.x;
	image[NUM_CHANNELS * i + 1] = yiq_vec.y / 1.192 + 127.5;
	image[NUM_CHANNELS * i + 2] = yiq_vec.z / 1.051 + 128.106565176;
    }
}

void
transform_yiq_to_rgb (float *image)
{
    static Matrix3D conversion_matrix =
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
	MultMatrixVector3D(&rgb_vec, (const Matrix3D*)&conversion_matrix, &yiq_vec);
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
	    float dist = (int)coeffs->subpixel.subpixels[channel * NUM_SUBPIXELS + i] - (int)pixel->subpixels[channel * NUM_SUBPIXELS + i];

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
}

static int
compare_indexes (const void *p1, const void *p2)
{
    index_t *i1 = (index_t*)p1;
    index_t *i2 = (index_t*)p2;

    return *i1 - *i2;
}

void
generate_search_coeffs (search_coefficients_t *search_coeffs, float sums[NUM_COEFFS], coefficient_with_index_t raw_coeffs[NUM_COEFFS])
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

	scaled_data = scale_image(image_data, image_width, image_height, x, y, width, height, NUM_SUBPIXEL_ROWS_COLS, NUM_SUBPIXEL_ROWS_COLS);

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

static int
metapixel_in_array (metapixel_t *pixel, metapixel_t **array, int size)
{
    int i;

    for (i = 0; i < size; ++i)
	if (array[i] == pixel)
	    return 1;
    return 0;
}

static metapixel_t*
metapixel_nearest_to (coeffs_t *coeffs, metapixel_t **forbidden, int num_forbidden, int method)
{
    float best_score = 1e99;
    metapixel_t *best_fit = 0, *pixel;
    compare_func_t compare_func = 0;

    if (method == METHOD_WAVELET)
	compare_func = wavelet_compare;
    else if (method == METHOD_SUBPIXEL)
	compare_func = subpixel_compare;
    else
	assert(0);

    for (pixel = first_pixel; pixel != 0; pixel = pixel->next)
    {
	float score = compare_func(coeffs, pixel, best_score);

	if (score < best_score && !metapixel_in_array(pixel, forbidden, num_forbidden))
	{
	    best_score = score;
	    best_fit = pixel;
	}
    }

    return best_fit;
}

static void
paste_metapixel (metapixel_t *pixel, unsigned char *data, int width, int height, int x, int y)
{
    int i;
    int pixel_width, pixel_height;
    unsigned char *pixel_data = read_image(pixel->filename, &pixel_width, &pixel_height);

    if (pixel_width != small_width || pixel_height != small_height)
    {
	unsigned char *scaled_data = scale_image(pixel_data, pixel_width, pixel_height,
						 0, 0, pixel_width, pixel_height,
						 small_width, small_height);

	free(pixel_data);
	pixel_data = scaled_data;
    }

    for (i = 0; i < small_height; ++i)
	memcpy(data + NUM_CHANNELS * (x + (y + i) * width),
	       pixel_data + NUM_CHANNELS * i * small_width, NUM_CHANNELS * small_width);

    free(pixel_data);
}

static void
generate_classic (char *input_name, char *output_name, float scale, int min_distance, int method, int cheat)
{
    metapixel_t **metapixels, **neighborhood = 0;
    int x, y;
    int metawidth, metaheight;
    int neighborhood_diameter = min_distance * 2 + 1;
    int neighborhood_size = (neighborhood_diameter * neighborhood_diameter - 1) / 2;
    int in_image_width, in_image_height;
    int out_image_width, out_image_height;
    image_reader_t *reader;
    image_writer_t *writer;
    unsigned char *out_image_data;

    reader = open_image_reading(input_name);
    if (reader == 0)
    {
	fprintf(stderr, "cannot read image %s\n", input_name);
	exit(1);
    }

    in_image_width = reader->width;
    in_image_height = reader->height;

    out_image_width = (((int)((float)in_image_width * scale) - 1) / small_width + 1) * small_width;
    out_image_height = (((int)((float)in_image_height * scale) - 1) / small_height + 1) * small_height;

    assert(out_image_width % small_width == 0);
    assert(out_image_height % small_height == 0);

    writer = open_image_writing(output_name, out_image_width, out_image_height, IMAGE_FORMAT_PNG);
    if (writer == 0)
    {
	fprintf(stderr, "cannot write image %s\n", output_name);
	exit(1);
    }

    metawidth = out_image_width / small_width;
    metaheight = out_image_height / small_height;

    metapixels = (metapixel_t**)malloc(sizeof(metapixel_t*) * metawidth * metaheight);
    if (min_distance > 0)
	neighborhood = (metapixel_t**)malloc(sizeof(metapixel_t*) * neighborhood_size);

    out_image_data = (unsigned char*)malloc(out_image_width * small_height * NUM_CHANNELS);

    for (y = 0; y < metaheight; ++y)
    {
	int num_lines = (y + 1) * in_image_height / metaheight - y * in_image_height / metaheight;
	unsigned char *in_image_data = (unsigned char*)malloc(num_lines * in_image_width * NUM_CHANNELS);

	assert(in_image_data != 0);

	read_lines(reader, in_image_data, num_lines);

	for (x = 0; x < metawidth; ++x)
	{
	    metapixel_t *pixel;
	    int i;
	    coeffs_t coeffs;
	    int left_x = x * in_image_width / metawidth;
	    int width = (x + 1) * in_image_width / metawidth - left_x;

	    for (i = 0; i < neighborhood_size; ++i)
	    {
		int nx = x + i % neighborhood_diameter - min_distance;
		int ny = y + i / neighborhood_diameter - min_distance;

		if (nx < 0 || nx >= metawidth || ny < 0 || ny >= metaheight)
		    neighborhood[i] = 0;
		else
		    neighborhood[i] = metapixels[ny * metawidth + nx];
	    }

	    generate_search_coeffs_for_subimage(&coeffs, in_image_data, in_image_width, num_lines,
						left_x, 0, width, num_lines, method);

	    pixel = metapixel_nearest_to(&coeffs, neighborhood, neighborhood_size, method);
	    paste_metapixel(pixel, out_image_data, out_image_width, small_height, x * small_width, 0);

	    metapixels[y * metawidth + x] = pixel;

	    printf(".");
	    fflush(stdout);
	}

	if (cheat > 0)
	{
	    unsigned char *source_data;

	    if (in_image_width != out_image_width || num_lines != small_height)
		source_data = scale_image(in_image_data, in_image_width, num_lines,
					  0, 0, in_image_width, num_lines,
					  out_image_width, small_height);
	    else
		source_data = in_image_data;

	    alpha_compose(out_image_data, out_image_width, small_height, source_data, cheat);

	    if (source_data != in_image_data)
		free(source_data);
	}

	free(in_image_data);

	write_lines(writer, out_image_data, small_height);
    }

    free(out_image_data);

    free_image_writer(writer);
    free_image_reader(reader);

    printf("\n");
}

static void
generate_collage (char *input_name, char *output_name, float scale, int method, int cheat)
{
    unsigned char *in_image_data, *out_image_data;
    int in_image_width, in_image_height;
    char *bitmap;
    int num_pixels_done = 0;

    in_image_data = read_image(input_name, &in_image_width, &in_image_height);
    if (in_image_data == 0)
    {
	fprintf(stderr, "could not read image %s\n", input_name);
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
	metapixel_t *pixel;

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
	generate_search_coeffs_for_subimage(&coeffs, in_image_data, in_image_width, in_image_height, x, y, small_width, small_height, method);

	pixel = metapixel_nearest_to(&coeffs, 0, 0, method);
	paste_metapixel(pixel, out_image_data, in_image_width, in_image_height, x, y);

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
	   "Options:\n"
	   "  -w, --width=WIDTH           set width for small images\n"
	   "  -h, --height=HEIGHT         set height for small images\n"
	   "  -y, --y-weight=WEIGHT       assign relative weight for the Y-channel\n"
	   "  -i, --i-weight=WEIGHT       assign relative weight for the I-channel\n"
	   "  -q, --q-weight=WEIGHT       assign relative weight for the Q-channel\n"
	   "  -s  --scale=SCALE           scale input image by specified factor\n"
	   "  -m, --metric=METRIC         choose metric (subpixel or wavelet)\n"
	   "  -c, --collage               collage mode\n"
	   "  -d, --distance=DIST         minimum distance between two instances of\n"
	   "                              the same constituent image\n"
	   "  -a, --cheat=PERC            cheat with specified percentage\n"
	   "\n"
	   "Report bugs to schani@complang.tuwien.ac.at\n");
}

#define MODE_NONE       0
#define MODE_PREPARE    1
#define MODE_METAPIXEL  2

int
main (int argc, char *argv[])
{
    int mode = MODE_NONE;
    int method = METHOD_SUBPIXEL;
    int collage = 0;
    int min_distance = 0;
    int cheat = 0;
    float scale = 1.0;

    while (1)
    {
	static struct option long_options[] =
            {
		{ "version", no_argument, 0, 256 },
		{ "help", no_argument, 0, 257 },
		{ "prepare", no_argument, 0, 258 },
		{ "metapixel", no_argument, 0, 259 },
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
		{ 0, 0, 0, 0 }
	    };

	int option,
	    option_index;

	option = getopt_long(argc, argv, "m:w:h:y:i:q:s:cd:a:", long_options, &option_index);

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

	    case 'm' :
		if (strcmp(optarg, "wavelet") == 0)
		    method = METHOD_WAVELET;
		else if (strcmp(optarg, "subpixel") == 0)
		    method = METHOD_SUBPIXEL;
		else
		{
		    fprintf(stderr, "method must either be subpixel or wavelet\n");
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
		    fprintf(stderr, "invalid scale factor\n");
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

	    case 256 :
		printf("metapixel " METAPIXEL_VERSION "\n"
		       "\n"
		       "Copyright (C) 1997-2000 Mark Probst\n"
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
	}
    }

    sqrt_of_two = sqrt(2);
    sqrt_of_image_size = sqrt(IMAGE_SIZE);

    compute_index_weights();

    if (mode == MODE_PREPARE)
    {
	float *float_image = (float*)malloc(sizeof(float) * NUM_CHANNELS * IMAGE_SIZE * IMAGE_SIZE);
	int i, channel;
	coefficient_with_index_t highest_coeffs[NUM_COEFFS];
	unsigned char *image_data;
	unsigned char *scaled_data;
	char *inimage_name, *outimage_name, *tables_name;
	FILE *tables_file;
	int in_width, in_height;

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
	    fprintf(stderr, "could not read image %s\n", inimage_name);
	    return 1;
	}

	tables_file = fopen(tables_name, "a");
	if (tables_file == 0)
	{
	    fprintf(stderr, "could not open file %s for writing\n", tables_name);
	    return 1;
	}

	/* generate small image */
	scaled_data = scale_image(image_data, in_width, in_height, 0, 0, in_width, in_height, small_width, small_height);
	assert(scaled_data != 0);

	write_image(outimage_name, small_width, small_height, scaled_data, IMAGE_FORMAT_PNG);

	/* generate wavelet coefficients */
	if (small_width != IMAGE_SIZE || small_height != IMAGE_SIZE)
	{
	    free(scaled_data);

	    scaled_data = scale_image(image_data, in_width, in_height, 0, 0, in_width, in_height, IMAGE_SIZE, IMAGE_SIZE);
	    assert(scaled_data != 0);
	}

	for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS; ++i)
	    float_image[i] = scaled_data[i];

	free(scaled_data);

	transform_rgb_to_yiq(float_image, IMAGE_SIZE * IMAGE_SIZE);
	decompose_image(float_image);
	
	find_highest_coefficients(float_image, highest_coeffs);

	fprintf(tables_file, "%s\n", outimage_name);
	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    fprintf(tables_file, "%f ", float_image[channel]);
	fprintf(tables_file, "\n");
	for (i = 0; i < NUM_COEFFS; ++i)
	    fprintf(tables_file, "%d ", highest_coeffs[i].index);
	fprintf(tables_file, "\n");

	/* generate subpixel coefficients */
	scaled_data = scale_image(image_data, in_width, in_height, 0, 0, in_width, in_height, NUM_SUBPIXEL_ROWS_COLS, NUM_SUBPIXEL_ROWS_COLS);
	assert(scaled_data != 0);

	for (i = 0; i < NUM_SUBPIXELS * NUM_CHANNELS; ++i)
	    float_image[i] = scaled_data[i];

	transform_rgb_to_yiq(float_image, NUM_SUBPIXELS);

	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	{
	    for (i = 0; i < NUM_SUBPIXELS; ++i)
		fprintf(tables_file, "%d ", (int)float_image[i * NUM_CHANNELS + channel]);
	    fprintf(tables_file, "\n");
	}

	free(scaled_data);

	fclose(tables_file);
    }
    else if (mode == MODE_METAPIXEL)
    {
	if (argc - optind != 2)
	{
	    usage();
	    return 1;
	}

	do
	{
	    coefficient_with_index_t coeffs[NUM_COEFFS];
	    metapixel_t *pixel = (metapixel_t*)malloc(sizeof(metapixel_t));
	    int i, channel;
	    float sums[NUM_COEFFS];

	    scanf("%s", pixel->filename);
	    if (feof(stdin))
		break;
	    assert(strlen(pixel->filename) > 0);
	    for (channel = 0; channel < 3; ++channel)
		scanf("%f", &pixel->means[channel]);
	    for (i = 0; i < NUM_COEFFS; ++i)
		scanf("%d", &coeffs[i].index);

	    generate_search_coeffs(&pixel->coeffs, sums, coeffs);

	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
		for (i = 0; i < NUM_SUBPIXELS; ++i)
		{
		    int val;

		    scanf("%d", &val);
		    pixel->subpixels[channel * NUM_SUBPIXELS + i] = val;
		}

	    add_metapixel(pixel);
	} while (!feof(stdin));

	if (!collage)
	    generate_classic(argv[optind], argv[optind + 1], scale, min_distance, method, cheat);
	else
	    generate_collage(argv[optind], argv[optind + 1], scale, method, cheat);
    }
    else
    {
	usage();
	return 1;
    }

    return 0;
}
