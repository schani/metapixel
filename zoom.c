/* -*- c -*- */

/*
 * zoom.c
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

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#include "zoom.h"

#ifndef MIN
#define MIN(a,b)           ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)           ((a)>(b)?(a):(b))
#endif

#define MAX_FILTER          FILTER_MITCHELL

typedef struct
{
    int index;
    float weight;
    int iweight;
} sample_t;

typedef struct
{
    int num_samples;
    sample_t samples[0];
} sample_window_t;

static float
filter_box (float x)
{
    if (x < -0.5)
	return 0.0;
    if (x <= 0.5)
	return 1.0;
    return 0.0;
}

static float
filter_triangle (float x)
{
    if (x < -1.0)
	return 0.0;
    if (x < 0.0)
	return 1.0 + x;
    if (x < 1.0)
	return 1.0 - x;
    return 0.0;
}

/*
 * see Mitchell&Netravali, "Reconstruction Filters in Computer
 * Graphics", Proceedings of the 15th annual conference on Computer
 * graphics and interactive techniques, ACM Press 1988
 */
static float
filter_mitchell (float x)
{
#define B (1.0 / 3.0)
#define C (1.0 / 3.0)

    static float a0 = (  6.0 -  2.0 * B           ) / 6.0;
    static float a2 = (-18.0 + 12.0 * B +  6.0 * C) / 6.0;
    static float a3 = ( 12.0 -  9.0 * B -  6.0 * C) / 6.0;

    static float b0 = (         8.0 * B + 24.0 * C) / 6.0;
    static float b1 = (      - 12.0 * B - 48.0 * C) / 6.0;
    static float b2 = (         6.0 * B + 30.0 * C) / 6.0;
    static float b3 = (      -        B -  6.0 * C) / 6.0;

    x = fabsf(x);

    if (x < 1.0)
	return a0 + (x * x) * (a2 + x * a3);
    if (x < 2.0)
	return b0 + x * (b1 + x * (b2 + x * b3));
    return 0.0;

#undef b
#undef c
}

static filter_t filters[] = {
    { &filter_box,          0.5 },
    { &filter_triangle,     1.0 },
    { &filter_mitchell,     2.0 }
};

filter_t*
get_filter (int index)
{
    if (index < 0 || index > MAX_FILTER)
	return 0;

    return &filters[index];
}

#define NUM_ACCURACY_BITS         12

static sample_window_t*
make_sample_window (float center, float scale, float support_radius, filter_func_t filter_func, int num_indexes)
{
    float lower_bound = center - support_radius;
    float upper_bound = center + support_radius;
    int lower_index = floor(lower_bound + 0.5);
    int upper_index = floor(upper_bound - 0.5);
    int num_samples;
    sample_window_t *window;
    int i;
    float weight_sum;

    lower_index = MAX(0, lower_index);
    upper_index = MIN(num_indexes - 1, upper_index);

    if (upper_index < lower_index)
	upper_index = lower_index = floor(center);

    num_samples = upper_index - lower_index + 1;

    assert(num_samples > 0);

    window = (sample_window_t*)malloc(sizeof(sample_window_t) + num_samples * sizeof(sample_t));
    assert(window != 0);

    window->num_samples = num_samples;

    weight_sum = 0.0;
    for (i = 0; i < num_samples; ++i)
    {
	int index = lower_index + i;
	float sample_center = (float)index + 0.5;

	window->samples[i].index = index;
	window->samples[i].weight = filter_func((sample_center - center) / scale);

	weight_sum += window->samples[i].weight;
    }

    assert(weight_sum > 0.0);

    for (i = 0; i < num_samples; ++i)
    {
	window->samples[i].weight /= weight_sum;
	window->samples[i].iweight = (1 << NUM_ACCURACY_BITS) * window->samples[i].weight;
    }

    return window;
}

static sample_window_t**
make_sample_windows (float filter_scale, float filter_support_radius, filter_func_t filter_func,
		     int dest_size, int src_size, float scale)
{
    sample_window_t **sample_windows;
    int i;

    sample_windows = (sample_window_t**)malloc(dest_size * sizeof(sample_window_t*));
    assert(sample_windows != 0);
    for (i = 0; i < dest_size; ++i)
    {
	float dest_center = (float)i + 0.5;
	float src_center = dest_center / scale;

	sample_windows[i] = make_sample_window(src_center, filter_scale, filter_support_radius,
					       filter_func, src_size);
	assert(sample_windows[i] != 0);
    }

    return sample_windows;
}

static void
free_sample_windows (sample_window_t **sample_windows, int size)
{
    int i;

    for (i = 0; i < size; ++i)
	free(sample_windows[i]);
    free(sample_windows);
}

static void
zoom_unidirectional (unsigned char *dest, unsigned char *src, int num_channels, sample_window_t **sample_windows,
		     int num_pixels_in_entity, int num_entities,
		     int dest_pixel_advance, int src_pixel_advance,
		     int dest_entity_advance, int src_entity_advance)
{
    int i;
    unsigned char *dest_entity, *src_entity;
    int channels[num_channels];

    dest_entity = dest;
    src_entity = src;
    for (i = 0; i < num_entities; ++i)
    {
	int j;
	unsigned char *dest_pixel;

	dest_pixel = dest_entity;
	for (j = 0; j < num_pixels_in_entity; ++j)
	{
	    
	    int k;

	    for (k = 0; k < num_channels; ++k)
		channels[k] = 0;

	    for (k = 0; k < sample_windows[j]->num_samples; ++k)
	    {
		int l;
		sample_t *sample = &sample_windows[j]->samples[k];
		unsigned char *src_pixel = &src_entity[sample->index * src_pixel_advance];

		for (l = 0; l < num_channels; ++l)
		    channels[l] += (int)src_pixel[l] * sample->iweight;
		/* ((((int)src_pixel[l]) << CHANNEL_SHIFT) + (1 << (CHANNEL_SHIFT - 1))) * sample->weight; */
	    }

	    for (k = 0; k < num_channels; ++k)
	    {
		int value = channels[k] >> NUM_ACCURACY_BITS;

		dest_pixel[k] = MAX(0, MIN(255, value));
	    }

	    dest_pixel += dest_pixel_advance;
	}

	dest_entity += dest_entity_advance;
	src_entity += src_entity_advance;
    }
}

void
zoom_image (unsigned char *dest, unsigned char *src,
	    filter_t *filter, int num_channels,
	    int dest_width, int dest_height, int dest_row_stride,
	    int src_width, int src_height, int src_row_stride)
{
    float x_scale, y_scale;
    float filter_x_scale, filter_y_scale;
    float filter_x_support_radius, filter_y_support_radius;
    sample_window_t **x_sample_windows, **y_sample_windows;
    unsigned char *temp_image;

    assert(dest != 0 && src != 0 && filter != 0);

    assert(dest_width > 0 && dest_height > 0);

    x_scale = (float)dest_width / (float)src_width;
    y_scale = (float)dest_height / (float)src_height;

    filter_x_scale = MAX(1.0, 1.0 / x_scale);
    filter_y_scale = MAX(1.0, 1.0 / y_scale);

    filter_x_support_radius = filter->support_radius * filter_x_scale;
    filter_y_support_radius = filter->support_radius * filter_y_scale;

    x_sample_windows = make_sample_windows(filter_x_scale, filter_x_support_radius, filter->func,
					   dest_width, src_width, x_scale);
    y_sample_windows = make_sample_windows(filter_y_scale, filter_y_support_radius, filter->func,
					   dest_height, src_height, y_scale);

    temp_image = (unsigned char*)malloc(num_channels * dest_width * src_height);

    zoom_unidirectional(temp_image, src, num_channels, x_sample_windows,
			dest_width, src_height,
			num_channels, num_channels,
			dest_row_stride, src_row_stride);
    zoom_unidirectional(dest, temp_image, num_channels, y_sample_windows,
			dest_height, dest_width,
			dest_row_stride, dest_row_stride,
			num_channels, num_channels);

    free(temp_image);

    free_sample_windows(x_sample_windows, dest_width);
    free_sample_windows(y_sample_windows, dest_height);
}

#ifdef TEST_ZOOM
#include <stdio.h>

#include "readimage.h"
#include "writeimage.h"

int
main (int argc, char *argv[])
{
    unsigned char *src, *dst;
    int src_width, src_height;
    int dst_width, dst_height;
    void *png_write_data;

    if (argc != 5)
    {
	fprintf(stderr, "Usage: %s <in-image> <out-width> <out-height> <out-image>\n", argv[0]);
	return 1;
    }

    src = read_image(argv[1], &src_width, &src_height);
    assert(src != 0);

    dst_width = atoi(argv[2]);
    dst_height = atoi(argv[3]);

    dst = (unsigned char*)malloc(3 * dst_width * dst_height);

    zoom_image(dst, src, get_filter(FILTER_TRIANGLE), 3,
	       dst_width, dst_height, dst_width * 3,
	       src_width, src_height, src_width * 3);

    write_image(argv[4], dst_width, dst_height, dst, IMAGE_FORMAT_PNG);

    return 0;
    
}
#endif
