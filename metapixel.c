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
#include "rwpng.h"
#include "readimage.h"
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

#define NUM_Y_CLUSTERS      10
#define NUM_I_CLUSTERS      10
#define NUM_Q_CLUSTERS      10
#define MAX_NUM_CLUSTERS    MAX(NUM_Y_CLUSTERS,MAX(NUM_I_CLUSTERS,NUM_Q_CLUSTERS))

#define CLUSTER(c)          (clusters[(c)[0]][(c)[1]][(c)[2]])

#define NUM_COEFFS          (NUM_CHANNELS * SIGNIFICANT_COEFFS)

#define NUM_INDEXES         (IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS * 2)

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
    struct _metapixel_t *next;
} metapixel_t;

typedef struct
{
    float min_values[NUM_CHANNELS];
    float max_values[NUM_CHANNELS];
    struct _metapixel_t *first_pixel;
} cluster_t;

static cluster_t clusters[NUM_Y_CLUSTERS][NUM_I_CLUSTERS][NUM_Q_CLUSTERS];

float sqrt_of_two, sqrt_of_image_size;

float weight_factors[NUM_CHANNELS] = { 1.0, 1.0, 1.0 };

float index_weights[NUM_INDEXES];
index_t weight_ordered_index_to_index[NUM_INDEXES];
index_t index_to_weight_ordered_index[NUM_INDEXES];

int small_width = DEFAULT_WIDTH, small_height = DEFAULT_HEIGHT;

int num_clusters[NUM_CHANNELS] = { NUM_Y_CLUSTERS, NUM_I_CLUSTERS, NUM_Q_CLUSTERS };

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
transform_rgb_to_yiq (float *image)
{
    static Matrix3D conversion_matrix =
    {
	{ 0.299, 0.587, 0.114 },
	{ 0.596, -0.275, -0.321 },
	{ 0.212, -0.528, 0.311 }
    };

    int i;

    for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE; ++i)
    {
	Vector3D rgb_vec,
	    yiq_vec;

	InitVector3D(&rgb_vec,
		     image[NUM_CHANNELS * i + 0],
		     image[NUM_CHANNELS * i + 1],
		     image[NUM_CHANNELS * i + 2]);
	MultMatrixVector3D(&yiq_vec, (const Matrix3D*)&conversion_matrix, &rgb_vec);
	image[NUM_CHANNELS * i + 0] = yiq_vec.x;
	image[NUM_CHANNELS * i + 1] = yiq_vec.y + 127.5;
	image[NUM_CHANNELS * i + 2] = yiq_vec.z + 127.5;
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

static int num_instant_rejects = 0;

static float
compare_images (search_coefficients_t *query, float *query_means,
		search_coefficients_t *target, float *target_means,
		float sums[NUM_COEFFS], float best_score)
{
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
	{
	    if (i == 0)
		++num_instant_rejects;
	    return 1e99;
	}

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
generate_search_coeffs_for_subimage (search_coefficients_t *search_coeffs, float sums[NUM_COEFFS], float means[NUM_CHANNELS],
				     unsigned char *image_data, int width, int height, int x, int y, int use_crop)
{
    static float *float_image = 0;

    int i;
    coefficient_with_index_t raw_coeffs[NUM_COEFFS];

    if (float_image == 0)
	float_image = (float*)malloc(sizeof(float) * IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS);

    if (use_crop)
    {
	unsigned char *scaled_data;

	scaled_data = scale_image(image_data, width, height, x, y, small_width, small_height, IMAGE_SIZE, IMAGE_SIZE);
	assert(scaled_data != 0);

	for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS; ++i)
	    float_image[i] = scaled_data[i];

	free(scaled_data);
    }
    else
    {
	int j, channel;

	if (small_width != IMAGE_SIZE || small_height != IMAGE_SIZE)
	{
	    x = x / small_width * IMAGE_SIZE;
	    y = y / small_height * IMAGE_SIZE;
	}

	for (j = 0; j < IMAGE_SIZE; ++j)
	    for (i = 0; i < IMAGE_SIZE; ++i)
		for (channel = 0; channel < NUM_CHANNELS; ++channel)
		    float_image[(j * IMAGE_SIZE + i) * NUM_CHANNELS + channel] =
			image_data[((y + j) * width + (x + i)) * NUM_CHANNELS + channel];
    }

    transform_rgb_to_yiq(float_image);
    decompose_image(float_image);
    find_highest_coefficients(float_image, raw_coeffs);

    generate_search_coeffs(search_coeffs, sums, raw_coeffs);

    for (i = 0; i < NUM_CHANNELS; ++i)
	means[i] = float_image[i];
}

static void
init_clusters (void)
{
    int y, i, q;

    for (y = 0; y < NUM_Y_CLUSTERS; ++y)
    {
	float y_min = (float)y / (float)NUM_Y_CLUSTERS * 255.0;
	float y_max = (float)(y + 1) / (float)NUM_Y_CLUSTERS * 255.0;

	for (i = 0; i < NUM_I_CLUSTERS; ++i)
	{
	    float i_min = (float)i / (float)NUM_I_CLUSTERS * 255.0;
	    float i_max = (float)(i + 1) / (float)NUM_I_CLUSTERS * 255.0;

	    for (q = 0; q < NUM_Q_CLUSTERS; ++q)
	    {
		float q_min = (float)q / (float)NUM_Q_CLUSTERS * 255.0;
		float q_max = (float)(q + 1) / (float)NUM_Q_CLUSTERS * 255.0;

		clusters[y][i][q].min_values[0] = y_min;
		clusters[y][i][q].min_values[1] = i_min;
		clusters[y][i][q].min_values[2] = q_min;

		clusters[y][i][q].max_values[0] = y_max;
		clusters[y][i][q].max_values[1] = i_max;
		clusters[y][i][q].max_values[2] = q_max;

		clusters[y][i][q].first_pixel = 0;
	    }
	}
    }
}

static void
cluster_coordinates_for_means (int coords[NUM_CHANNELS], float means[NUM_CHANNELS])
{
    coords[0] = MIN(floor(means[0] / 255.0 * NUM_Y_CLUSTERS), NUM_Y_CLUSTERS - 1);
    coords[1] = MIN(floor(means[1] / 255.0 * NUM_I_CLUSTERS), NUM_I_CLUSTERS - 1);
    coords[2] = MIN(floor(means[2] / 255.0 * NUM_Q_CLUSTERS), NUM_Q_CLUSTERS - 1);

    assert(coords[0] >= 0 && coords[0] < NUM_Y_CLUSTERS);
    assert(coords[1] >= 0 && coords[1] < NUM_I_CLUSTERS);
    assert(coords[2] >= 0 && coords[2] < NUM_Q_CLUSTERS);
}

float
best_possible_score_in_cluster (cluster_t *cluster, float sums[NUM_COEFFS], float means[NUM_CHANNELS])
{
    int i;
    float score = 0;

    for (i = 0; i < NUM_CHANNELS; ++i)
    {
	if (means[i] < cluster->min_values[i] || means[i] > cluster->max_values[i])
	{
	    float dist;

	    if (means[i] < cluster->min_values[i])
		dist = cluster->min_values[i] - means[i];
	    else
		dist = means[i] - cluster->max_values[i];

	    score += index_weights[compute_index(0, i, 0)] * dist * 0.05;
	}
    }

    return score - sums[0];
}

void
add_metapixel (metapixel_t *pixel)
{
    int coords[NUM_CHANNELS];

    cluster_coordinates_for_means(coords, pixel->means);

    pixel->next = CLUSTER(coords).first_pixel;
    CLUSTER(coords).first_pixel = pixel;
}

static int num_cluster_rejects = 0;

static metapixel_t*
metapixel_nearest_to (search_coefficients_t *search_coeffs, float sums[NUM_COEFFS], float query_means[NUM_CHANNELS])
{
    int center_coords[NUM_CHANNELS];
    int dist;
    float best_score = 1e99;
    metapixel_t *best_fit = 0, *pixel;
    int num_max;

    cluster_coordinates_for_means(center_coords, query_means);

    for (dist = 0; dist < MAX_NUM_CLUSTERS - 1; ++dist)
    {
	int dists[NUM_CHANNELS];
	int i;
	int out_of_reach = 1;

	for (i = 0; i < NUM_CHANNELS; ++i)
	    dists[i] = -dist;
	num_max = NUM_CHANNELS;

	do
	{
	    int coords[NUM_CHANNELS];

	    for (i = 0; i < NUM_CHANNELS; ++i)
	    {
		coords[i] = center_coords[i] + dists[i];
		if (coords[i] < 0 || coords[i] >= num_clusters[i])
		    goto next_cluster;
	    }

	    if (best_score > best_possible_score_in_cluster(&CLUSTER(coords), sums, query_means))
	    {
		for (pixel = CLUSTER(coords).first_pixel; pixel != 0; pixel = pixel->next)
		{
		    float score = compare_images(search_coeffs, query_means, &pixel->coeffs, pixel->means, sums, best_score);

		    if (score < best_score)
		    {
			best_score = score;
			best_fit = pixel;
		    }
		}

		out_of_reach = 0;
	    }
	    else
		++num_cluster_rejects;

	next_cluster:
	    for (i = NUM_CHANNELS - 1; i >= 0; --i)
		if (i == NUM_CHANNELS - 1 && num_max == 1 && (dists[i] == -dist || dists[i] == dist))
		{
		    dists[i] = -dists[i];

		    if (dists[i] == dist)
			break;
		}
		else
		{
		    if (++dists[i] > dist)
		    {
			if (i == 0)
			    goto next_dist;
			dists[i] = -dist;
		    }
		    else
		    {
			if (dists[i] == dist)
			    ++num_max;
			else if (dists[i] == -dist + 1)
			    --num_max;
			break;
		    }
		}

	} while (1);

    next_dist:
	if (out_of_reach)
	    break;
    }

    return best_fit;
}

static void
paste_metapixel (metapixel_t *pixel, unsigned char *data, int width, int height, int x, int y)
{
    int i;
    int pixel_width, pixel_height;
    unsigned char *pixel_data = read_png_file(pixel->filename, &pixel_width, &pixel_height);

    for (i = 0; i < small_height; ++i)
	memcpy(data + NUM_CHANNELS * (x + (y + i) * width),
	       pixel_data + NUM_CHANNELS * i * small_width, NUM_CHANNELS * small_width);

    free(pixel_data);
}

static void
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
	   "  -c, --collage               collage mode\n"
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
    int collage = 0;

    while (1)
    {
	static struct option long_options[] =
            {
		{ "version", no_argument, 0, 256 },
		{ "help", no_argument, 0, 257 },
		{ "prepare", no_argument, 0, 'p' },
		{ "metapixel", no_argument, 0, 'm' },
		{ "width", required_argument, 0, 'w' },
		{ "height", required_argument, 0, 'h' },
		{ "y-weight", required_argument, 0, 'y' },
		{ "i-weight", required_argument, 0, 'i' },
		{ "q-weight", required_argument, 0, 'q' },
		{ "collage", no_argument, 0, 'c' },
		{ 0, 0, 0, 0 }
	    };

	int option,
	    option_index;

	option = getopt_long(argc, argv, "pmw:h:y:i:q:c", long_options, &option_index);

	if (option == -1)
	    break;

	switch (option)
	{
	    case 'p' :
		mode = MODE_PREPARE;
		break;

	    case 'm' :
		mode = MODE_METAPIXEL;
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

	    case 'c' :
		collage = 1;
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

    init_clusters();

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

	write_png_file(outimage_name, small_width, small_height, scaled_data);

	/* generate coefficients */
	if (small_width != IMAGE_SIZE || small_height != IMAGE_SIZE)
	{
	    free(scaled_data);

	    scaled_data = scale_image(image_data, in_width, in_height, 0, 0, in_width, in_height, IMAGE_SIZE, IMAGE_SIZE);
	    assert(scaled_data != 0);
	}

	for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS; ++i)
	    float_image[i] = scaled_data[i];

	transform_rgb_to_yiq(float_image);
	decompose_image(float_image);
	
	find_highest_coefficients(float_image, highest_coeffs);

	fprintf(tables_file, "%s\n", outimage_name);
	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    fprintf(tables_file, "%f ", float_image[channel]);
	fprintf(tables_file, "\n");
	for (i = 0; i < NUM_COEFFS; ++i)
	    fprintf(tables_file, "%d ", highest_coeffs[i].index);
	fprintf(tables_file, "\n");

	fclose(tables_file);
    }
    else if (mode == MODE_METAPIXEL)
    {
	unsigned char *in_image_data, *out_image_data;
	int in_image_width, in_image_height;
	int use_crop = 1;

	if (argc - optind != 2)
	{
	    usage();
	    return 1;
	}

	do
	{
	    coefficient_with_index_t coeffs[NUM_COEFFS];
	    metapixel_t *pixel = (metapixel_t*)malloc(sizeof(metapixel_t));
	    int channel;
	    int i;
	    float sums[NUM_COEFFS];

	    scanf("%s", pixel->filename);
	    if (feof(stdin))
		break;
	    for (channel = 0; channel < 3; ++channel)
		scanf("%f", &pixel->means[channel]);
	    for (i = 0; i < NUM_COEFFS; ++i)
		scanf("%d", &coeffs[i].index);

	    generate_search_coeffs(&pixel->coeffs, sums, coeffs);

	    add_metapixel(pixel);
	} while (!feof(stdin));

	in_image_data = read_image(argv[optind], &in_image_width, &in_image_height);
	if (in_image_data == 0)
	{
	    fprintf(stderr, "could not read image %s\n", argv[optind]);
	    return 1;
	}

	if (!collage && (small_width != IMAGE_SIZE || small_height != IMAGE_SIZE))
	{
	    unsigned char *scaled_data;

	    scaled_data = scale_image(in_image_data, in_image_width, in_image_height, 0, 0, in_image_width, in_image_height,
				      in_image_width / small_width * IMAGE_SIZE, in_image_height / small_height * IMAGE_SIZE);
	    assert(scaled_data != 0);
	    free(in_image_data);
	    in_image_data = scaled_data;
	}

	if (!collage || (small_width == IMAGE_SIZE || small_height == IMAGE_SIZE))
	    use_crop = 0;

	out_image_data = (unsigned char*)malloc(in_image_width * in_image_height * NUM_CHANNELS);

	if (!collage)
	{
	    int x, y;

	    assert(in_image_width % small_width == 0);
	    assert(in_image_height % small_height == 0);

	    for (y = 0; y < in_image_height / small_height; ++y)
		for (x = 0; x < in_image_width / small_width; ++x)
		{
		    search_coefficients_t search_coeffs;
		    metapixel_t *pixel;
		    float means[3];
		    float sums[NUM_COEFFS];

		    generate_search_coeffs_for_subimage(&search_coeffs, sums, means, in_image_data, in_image_width, in_image_height,
							x * small_width, y * small_height, use_crop);

		    pixel = metapixel_nearest_to(&search_coeffs, sums, means);
		    paste_metapixel(pixel, out_image_data, in_image_width, in_image_height, x * small_width, y * small_height);

		    printf(".");
		    fflush(stdout);
		}
	}
	else
	{
	    char *bitmap;
	    int num_pixels_done = 0;

	    bitmap = (char*)malloc(in_image_width * in_image_height);
	    memset(bitmap, 0, in_image_width * in_image_height);

	    while (num_pixels_done < in_image_width * in_image_height)
	    {
		int i, j;
		int x, y;
		search_coefficients_t search_coeffs;
		float sums[NUM_COEFFS];
		metapixel_t *pixel;
		float means[NUM_CHANNELS];

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
		generate_search_coeffs_for_subimage(&search_coeffs, sums, means, in_image_data, in_image_width, in_image_height, x, y, use_crop);

		pixel = metapixel_nearest_to(&search_coeffs, sums, means);
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
	}

	printf("done\n");

	write_png_file(argv[optind + 1], in_image_width, in_image_height, out_image_data);

	printf("instant rejects: %d    cluster rejects: %d\n", num_instant_rejects, num_cluster_rejects);
    }
    else
    {
	usage();
	return 1;
    }

    return 0;
}
