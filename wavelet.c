/*
 * wavelet.c
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

#include <stdlib.h>

#include "api.h"

static float index_weights[NUM_WAVELET_INDEXES];
static index_t weight_ordered_index_to_index[NUM_WAVELET_INDEXES];
static index_t index_to_weight_ordered_index[NUM_WAVELET_INDEXES];

static int
compute_index (int real_index, int channel, int sign)
{
    return real_index + (channel + (sign > 0 ? 1 : 0) * NUM_CHANNELS) * WAVELET_IMAGE_PIXELS;
}

static void
uncompute_index (int index, int *real_index, int *channel, int *sign)
{
    *real_index = index % (WAVELET_IMAGE_PIXELS);
    *channel = (index / WAVELET_IMAGE_PIXELS) % NUM_CHANNELS;
    *sign = (index / (NUM_CHANNELS * WAVELET_IMAGE_PIXELS)) ? 1 : -1;
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

    i = real_index % WAVELET_IMAGE_SIZE;
    j = real_index / WAVELET_IMAGE_SIZE;
    bin = MIN(MAX(i, j), 5);

    return weight_table[channel][bin] * weight_factors[channel];
}

static void
compute_index_weights (void)
{
    int i;

    for (i = 0; i < NUM_WAVELET_INDEXES; ++i)
	index_weights[i] = weight_function(i);

    for (i = 0; i < NUM_WAVELET_INDEXES; ++i)
	weight_ordered_index_to_index[i] = i;
    qsort(weight_ordered_index_to_index, NUM_WAVELET_INDEXES, sizeof(index_t), compare_indexes_by_weight_descending);
    for (i = 0; i < NUM_WAVELET_INDEXES; ++i)
	index_to_weight_ordered_index[weight_ordered_index_to_index[i]] = i;
}

static void
decompose_row (float *row)
{
    int h = WAVELET_IMAGE_SIZE, i;
    float new_row[WAVELET_ROW_LENGTH];

    for (i = 0; i < WAVELET_ROW_LENGTH; ++i)
	row[i] = row[i] / SQRT_OF_WAVELET_IMAGE_SIZE;

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

		new_row[channel + i * NUM_CHANNELS] = (val1 + val2) / SQRT_OF_TWO;
		new_row[channel + (h + i) * NUM_CHANNELS] = (val1 - val2) / SQRT_OF_TWO;
	    }
	}
	memcpy(row, new_row, sizeof(float) * NUM_CHANNELS * h * 2);
    }
}

static void
decompose_column (float *column)
{
    int h = WAVELET_IMAGE_SIZE, i, channel;
    float new_column[WAVELET_ROW_LENGTH];

    for (i = 0; i < WAVELET_IMAGE_SIZE; ++i)
	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    column[channel + i * WAVELET_ROW_LENGTH] =
		column[channel + i * WAVELET_ROW_LENGTH] / SQRT_OF_WAVELET_IMAGE_SIZE;

    while (h > 1)
    {
	h = h / 2;
	for (i = 0; i < h; ++i)
	{
	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    {
		float val1 = column[channel + (2 * i) * WAVELET_ROW_LENGTH],
		    val2 = column[channel + (2 * i + 1) * WAVELET_ROW_LENGTH];

		new_column[channel + i * NUM_CHANNELS] = (val1 + val2) / SQRT_OF_TWO;
		new_column[channel + (h + i) * NUM_CHANNELS] = (val1 - val2) / SQRT_OF_TWO;
	    }
	}

	for (i = 0; i < h * 2; ++i)
	    for (channel = 0; channel < NUM_CHANNELS; ++channel)
		column[channel + i * WAVELET_ROW_LENGTH] = new_column[channel + i * NUM_CHANNELS];
    }
}

void
wavelet_decompose_image (float *image)
{
    int row;

    for (row = 0; row < WAVELET_IMAGE_SIZE; ++row)
	decompose_row(image + WAVELET_ROW_LENGTH * row);
    for (row = 0; row < WAVELET_IMAGE_SIZE; ++row)
	decompose_column(image + NUM_CHANNELS * row);
}

static int
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
wavelet_find_highest_coeffs (float *image, coefficient_with_index_t highest_coeffs[NUM_COEFFS])
{
    int index, channel;

    for (channel = 0; channel < NUM_CHANNELS; ++channel)
    {
	for (index = 1; index < SIGNIFICANT_WAVELET_COEFFS + 1; ++index)
	{
	    float coeff = image[channel + NUM_CHANNELS * index];
	    int sign = coeff > 0.0 ? 1 : -1;

	    highest_coeffs[index - 1 + channel * SIGNIFICANT_WAVELET_COEFFS].index = compute_index(index, channel, sign);
	    highest_coeffs[index - 1 + channel * SIGNIFICANT_WAVELET_COEFFS].coeff = coeff;
	}

	qsort(highest_coeffs + channel * SIGNIFICANT_WAVELET_COEFFS, SIGNIFICANT_WAVELET_COEFFS,
	      sizeof(coefficient_with_index_t), compare_coeffs_with_index);
    }

    for (index = SIGNIFICANT_COEFFS + 1; index < WAVELET_IMAGE_PIXELS; ++index)
    {
	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	{
	    float coeff = image[channel + NUM_CHANNELS * index];

	    if (fabs(coeff) > fabs(highest_coeffs[(channel + 1) * SIGNIFICANT_WAVELET_COEFFS - 1].coeff))
	    {
		int significance;
		int sign = coeff > 0.0 ? 1 : -1;

		for (significance = (channel + 1) * SIGNIFICANT_WAVELET_COEFFS - 2;
		     significance >= channel * SIGNIFICANT_WAVELET_COEFFS;
		     --significance)
		    if (fabs(coeff) <= fabs(highest_coeffs[significance].coeff))
			break;
		++significance;
		memmove(highest_coeffs + significance + 1,
			highest_coeffs + significance,
			sizeof(coefficient_with_index_t) * ((channel + 1) * SIGNIFICANT_WAVELET_COEFFS
							    - 1 - significance));
		highest_coeffs[significance].index = compute_index(index, channel, sign);
		highest_coeffs[significance].coeff = coeff;
	    }
	}
    }
}

static int
compare_indexes (const void *p1, const void *p2)
{
    index_t *i1 = (index_t*)p1;
    index_t *i2 = (index_t*)p2;

    return *i1 - *i2;
}

void
wavelet_generate_coeffs (wavelet_coefficients_t *search_coeffs, float sums[NUM_WAVELET_COEFFS],
			 coefficient_with_index_t raw_coeffs[NUM_WAVELET_COEFFS])
{
    int i;
    float sum;

    for (i = 0; i < NUM_WAVELET_COEFFS; ++i)
	search_coeffs->coeffs[i] = index_to_weight_ordered_index[raw_coeffs[i].index];

    qsort(search_coeffs->coeffs, NUM_WAVELET_COEFFS, sizeof(index_t), compare_indexes);

    sum = 0.0;
    for (i = NUM_WAVELET_COEFFS - 1; i >= 0; --i)
    {
	sum += index_weights[weight_ordered_index_to_index[search_coeffs->coeffs[i]]];
	sums[i] = sum;
    }
}

void
init_wavelet (void)
{
    compute_index_weights();
}
