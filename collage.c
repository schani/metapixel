/*
 * collage.c
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
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "api.h"

typedef struct _position_t
{
    int x, y;
    struct _position_t *next;
} position_t;

typedef struct
{
    int min_distance;
    position_t **collage_positions;
} collage_position_valid_data_t;

static int
pixel_valid_for_collage_position (void *_data, metapixel_t *pixel, unsigned int pixel_index, int x, int y)
{
    collage_position_valid_data_t *data = (collage_position_valid_data_t*)_data;
    position_t *position;

    if (data->min_distance <= 0)
	return 1;

    for (position = data->collage_positions[pixel_index]; position != 0; position = position->next)
	if (utils_manhattan_distance(x, y, position->x, position->y) < data->min_distance)
	    return 0;

    return 1;
}

static void
add_collage_position (position_t **positions, int x, int y)
{
    position_t *position = (position_t*)malloc(sizeof(position_t));

    assert(position != 0);

    position->x = x;
    position->y = y;

    position->next = *positions;
    *positions = position;
}

static void
free_positions (position_t *positions)
{
    while (positions != 0)
    {
	position_t *next = positions->next;

	free(positions);

	positions = next;
    }
}

collage_mosaic_t*
collage_generate_from_bitmap (int num_libraries, library_t **libraries, bitmap_t *in_bitmap,
			      unsigned int small_width, unsigned int small_height,
			      unsigned int min_distance, metric_t *metric,
			      progress_report_func_t report_func)
{
    char *bitmap;
    unsigned int num_pixels_done = 0;
    position_t **collage_positions;
    unsigned int num_metapixels = library_count_metapixels(num_libraries, libraries);
    unsigned int i;
    unsigned int num_out_metapixels = 0;
    unsigned int num_matches_alloced;
    collage_match_t *matches;
    collage_mosaic_t *mosaic;
    PROGRESS_DECLS;

    if (small_width == 0 || small_height == 0)
    {
	error_report(ERROR_ZERO_SMALL_IMAGE_SIZE, error_make_null_info());
	return 0;
    }

    if (in_bitmap->width < small_width || in_bitmap->height < small_height)
    {
	error_report(ERROR_IMAGE_TOO_SMALL, error_make_null_info());
	return 0;
    }

    if (num_metapixels == 0)
    {
	error_report(ERROR_CANNOT_FIND_COLLAGE_MATCH, error_make_null_info());
	return 0;
    }

    bitmap = (char*)malloc(in_bitmap->width * in_bitmap->height);
    assert(bitmap != 0);
    memset(bitmap, 0, in_bitmap->width * in_bitmap->height);

    collage_positions = (position_t**)malloc(num_metapixels * sizeof(position_t*));
    assert(collage_positions != 0);
    memset(collage_positions, 0, num_metapixels * sizeof(position_t*));

    num_matches_alloced = (in_bitmap->width / small_width) * (in_bitmap->height / small_height) * 2;
    assert(num_matches_alloced >= 2);
    matches = (collage_match_t*)malloc(sizeof(collage_match_t) * num_matches_alloced);
    assert(matches != 0);

    START_PROGRESS;

    while (num_pixels_done < in_bitmap->width * in_bitmap->height)
    {
	int i, j;
	int x, y;
	coeffs_t coeffs;
	metapixel_match_t match;
	collage_position_valid_data_t valid_data = { min_distance, collage_positions };

	while (1)
	{
	    x = random() % in_bitmap->width - small_width / 2;
	    y = random() % in_bitmap->height - small_height / 2;

	    if (x < 0)
		x = 0;
	    if (x + small_width > in_bitmap->width)
		x = in_bitmap->width - small_width;

	    if (y < 0)
		y = 0;
	    if (y + small_height > in_bitmap->height)
		y = in_bitmap->height - small_height;

	    for (j = 0; j < small_height; ++j)
		for (i = 0; i < small_width; ++i)
		    if (!bitmap[(y + j) * in_bitmap->width + x + i])
			goto out;
	}

    out:
	metric_generate_coeffs_for_subimage(&coeffs, in_bitmap, x, y, small_width, small_height, metric);

	match = search_metapixel_nearest_to(num_libraries, libraries, &coeffs, metric, x, y,
					    0, 0, 0,
					    pixel_valid_for_collage_position, &valid_data);
	if (match.pixel == 0)
	{
	    /* FIXME: free stuff */

	    error_report(ERROR_CANNOT_FIND_COLLAGE_MATCH, error_make_null_info());

	    return 0;
	}

	assert(num_out_metapixels <= num_matches_alloced);
	if (num_out_metapixels == num_matches_alloced)
	{
	    num_matches_alloced += (in_bitmap->width / small_width) * (in_bitmap->height / small_height);
	    matches = (collage_match_t*)realloc(matches, sizeof(collage_match_t) * num_matches_alloced);
	    assert(matches != 0);
	}

	assert(num_out_metapixels < num_matches_alloced);
	matches[num_out_metapixels].x = x;
	matches[num_out_metapixels].y = y;
	matches[num_out_metapixels].match = match;
	++num_out_metapixels;

	if (min_distance > 0)
	    add_collage_position(&collage_positions[match.pixel_index], x, y);

	for (j = 0; j < small_height; ++j)
	    for (i = 0; i < small_width; ++i)
		if (!bitmap[(y + j) * in_bitmap->width + x + i])
		{
		    bitmap[(y + j) * in_bitmap->width + x + i] = 1;
		    ++num_pixels_done;
		}

#ifdef CONSOLE_OUTPUT
	printf(".");
	fflush(stdout);
#endif

	REPORT_PROGRESS((float)num_pixels_done / (float)(in_bitmap->width * in_bitmap->height));
    }

    free(bitmap);

    for (i = 0; i < num_metapixels; ++i)
	if (collage_positions[i] != 0)
	    free_positions(collage_positions[i]);
    free(collage_positions);

    mosaic = (collage_mosaic_t*)malloc(sizeof(collage_mosaic_t));
    assert(mosaic != 0);

    mosaic->in_image_width = in_bitmap->width;
    mosaic->in_image_height = in_bitmap->height;
    mosaic->small_image_width = small_width;
    mosaic->small_image_height = small_height;
    mosaic->num_matches = num_out_metapixels;
    mosaic->matches = matches;

    return mosaic;
}

void
collage_free (collage_mosaic_t *mosaic)
{
    free(mosaic->matches);
    free(mosaic);
}

static unsigned int
scale_coord (unsigned int x, unsigned int old_limit, unsigned int new_limit)
{
    if (new_limit == 0)
	return 0;
    /* FIXME: could overflow */
    return x * new_limit / old_limit;
}

bitmap_t*
collage_paste_to_bitmap (collage_mosaic_t *mosaic, unsigned int out_width, unsigned int out_height,
			 bitmap_t *in_image, unsigned int cheat, progress_report_func_t report_func)
{
    bitmap_t *out_bitmap;
    unsigned int i;
    PROGRESS_DECLS;

    /* FIXME: these should not be asserts */
    assert(out_width > 0 && out_height > 0);

    out_bitmap = bitmap_new_empty(COLOR_RGB_8, out_width, out_height);
    assert(out_bitmap != 0);

    START_PROGRESS;

    for (i = 0; i < mosaic->num_matches; ++i)
    {
	collage_match_t *match = &mosaic->matches[i];
	unsigned int x, y, width, height;

	x = scale_coord(match->x, mosaic->in_image_width - 1, out_width - 1);
	y = scale_coord(match->y, mosaic->in_image_height - 1, out_height - 1);
	width = scale_coord(match->x + mosaic->small_image_width, mosaic->in_image_width, out_width) - x;
	height = scale_coord(match->y + mosaic->small_image_height, mosaic->in_image_height, out_height) - y;

	assert(x < out_width && y < out_width);
	assert(width > 0 && height > 0);

	if (!metapixel_paste(match->match.pixel, out_bitmap, x, y, width, height))
	{
	    /* FIXME: free stuff */

	    return 0;
	}

	REPORT_PROGRESS((float)(i + 1) / (float)mosaic->num_matches);
    }

    if (cheat > 0)
    {
	bitmap_t *scaled_bitmap;

	assert(in_image != 0);

	if (out_width != in_image->width || out_height != in_image->height)
	    scaled_bitmap = bitmap_scale(in_image, out_width, out_height, FILTER_MITCHELL);
	else
	    scaled_bitmap = bitmap_copy(in_image);
	assert(scaled_bitmap != 0);

	bitmap_alpha_compose(out_bitmap, scaled_bitmap, cheat);

	bitmap_free(scaled_bitmap);
    }

    return out_bitmap;
}
