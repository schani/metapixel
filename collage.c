/*
 * collage.c
 *
 * metapixel
 *
 * Copyright (C) 1997-2007 Mark Probst
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

#include "lispreader/lispreader.h"

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

static float
frand (void)
{
    return rand() / (float)RAND_MAX;
}

collage_mosaic_t*
collage_generate_from_bitmap (int num_libraries, library_t **libraries, bitmap_t *in_bitmap,
			      unsigned int min_small_width, unsigned int min_small_height,
			      unsigned int max_small_width, unsigned int max_small_height,
			      unsigned int min_distance, metric_t *metric, unsigned int allowed_flips,
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

    if (min_small_width == 0 || min_small_height == 0
	|| max_small_width < min_small_width || max_small_height < min_small_height)
    {
	error_report(ERROR_ILLEGAL_SMALL_IMAGE_SIZE, error_make_null_info());
	return 0;
    }

    if (in_bitmap->width < max_small_width || in_bitmap->height < max_small_height)
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

    num_matches_alloced = (in_bitmap->width / max_small_width) * (in_bitmap->height / max_small_height) * 2;
    assert(num_matches_alloced >= 2);
    matches = (collage_match_t*)malloc(sizeof(collage_match_t) * num_matches_alloced);
    assert(matches != 0);

    START_PROGRESS;

    while (num_pixels_done < in_bitmap->width * in_bitmap->height)
    {
	unsigned int width, height;
	int i, j;
	int x, y;
	coeffs_union_t coeffs;
	metapixel_match_t match;
	collage_position_valid_data_t valid_data = { min_distance, collage_positions };
	float size_rand = frand();

	width = min_small_width + (unsigned int)(size_rand * (max_small_width - min_small_width));
	height = min_small_height + (unsigned int)(size_rand * (max_small_height - min_small_height));

	while (1)
	{
	    x = random() % in_bitmap->width - width / 2;
	    y = random() % in_bitmap->height - height / 2;

	    if (x < 0)
		x = 0;
	    if (x + width > in_bitmap->width)
		x = in_bitmap->width - width;

	    if (y < 0)
		y = 0;
	    if (y + height > in_bitmap->height)
		y = in_bitmap->height - height;

	    for (j = 0; j < height; ++j)
		for (i = 0; i < width; ++i)
		    if (!bitmap[(y + j) * in_bitmap->width + x + i])
			goto out;
	}

    out:
	metric_generate_coeffs_for_subimage(&coeffs, in_bitmap, x, y, width, height, metric);

	match = search_metapixel_nearest_to(num_libraries, libraries, &coeffs, metric, x, y,
					    0, 0, 0, allowed_flips,
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
	    num_matches_alloced += (in_bitmap->width / max_small_width) * (in_bitmap->height / max_small_height);
	    matches = (collage_match_t*)realloc(matches, sizeof(collage_match_t) * num_matches_alloced);
	    assert(matches != 0);
	}

	assert(num_out_metapixels < num_matches_alloced);
	matches[num_out_metapixels].x = x;
	matches[num_out_metapixels].y = y;
	matches[num_out_metapixels].width = width;
	matches[num_out_metapixels].height = height;
	matches[num_out_metapixels].match = match;
	++num_out_metapixels;

	if (min_distance > 0)
	    add_collage_position(&collage_positions[match.pixel_index], x, y);

	for (j = 0; j < height; ++j)
	    for (i = 0; i < width; ++i)
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
	width = scale_coord(match->x + match->width, mosaic->in_image_width, out_width) - x;
	height = scale_coord(match->y + match->height, mosaic->in_image_height, out_height) - y;

	assert(x < out_width && y < out_width);
	assert(width > 0 && height > 0);

	if (!metapixel_paste(match->match.pixel, out_bitmap, x, y, width, height, match->match.orientation))
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

collage_mosaic_t*
collage_read (int num_libraries, library_t **libraries, const char *filename,
	      int *num_new_libraries, library_t ***new_libraries)
{
    lisp_stream_t stream;
    lisp_object_t *obj;
    int type;
    collage_mosaic_t *mosaic = (collage_mosaic_t*)malloc(sizeof(collage_mosaic_t));

    assert(mosaic != 0);

    *num_new_libraries = 0;

    if (lisp_stream_init_path(&stream, filename) == 0)
    {
	error_report(ERROR_PROTOCOL_FILE_CANNOT_OPEN, error_make_string_info(filename));
	return 0;
    }

    obj = lisp_read(&stream);
    type = lisp_type(obj);

    if (type != LISP_TYPE_EOF && type != LISP_TYPE_PARSE_ERROR)
    {
	lisp_object_t *vars[3];

	if (lisp_match_string("(collage-mosaic (input-size #?(integer) #?(integer)) "
			      "                (metapixels . #?(list)))", obj, vars))
	{
	    lisp_object_t *lst;
	    unsigned int i;

	    mosaic->in_image_width = lisp_integer(vars[0]);
	    mosaic->in_image_height = lisp_integer(vars[1]);

	    mosaic->num_matches = lisp_list_length(vars[2]);
	    assert(mosaic->num_matches > 0);

	    mosaic->matches = (collage_match_t*)malloc(sizeof(collage_match_t) * mosaic->num_matches);

	    lst = vars[2];
	    for (i = 0; i < mosaic->num_matches; ++i)
	    {
		lisp_object_t *vars[7];

		if (lisp_match_string("(#?(integer) #?(integer) #?(integer) #?(integer) #?(string) #?(string) #?(real))",
				      lisp_car(lst), vars))
		{
		    mosaic->matches[i].x = lisp_integer(vars[0]);
		    mosaic->matches[i].y = lisp_integer(vars[1]);
		    mosaic->matches[i].width = lisp_integer(vars[2]);
		    mosaic->matches[i].height = lisp_integer(vars[3]);

		    mosaic->matches[i].match.pixel = metapixel_find_in_libraries(num_libraries, libraries,
										 lisp_string(vars[4]), lisp_string(vars[5]),
										 num_new_libraries, new_libraries);
		    mosaic->matches[i].match.score = lisp_real(vars[6]);

		    if (mosaic->matches[i].match.pixel == 0)
		    {
			/* FIXME: free stuff */
			return 0;
		    }
		}
		else
		{
		    /* FIXME: free stuff */

		    error_report(ERROR_PROTOCOL_SYNTAX_ERROR, error_make_string_info(filename));
		    return 0;
		}

		lst = lisp_cdr(lst);
	    }

	}
	else
	{
	    /* FIXME: free stuff */

	    error_report(ERROR_PROTOCOL_SYNTAX_ERROR, error_make_string_info(filename));
	    return 0;
	}
    }
    else
    {
	/* FIXME: free stuff */

	error_report(ERROR_PROTOCOL_PARSE_ERROR, error_make_string_info(filename));
	return 0;
    }
    lisp_free(obj);

    lisp_stream_free_path(&stream);

    return mosaic;
}

int
collage_write (collage_mosaic_t *mosaic, FILE *out)
{
    unsigned int i;

    for (i = 0; i < mosaic->num_matches; ++i)
	if (mosaic->matches[i].match.pixel->library == 0
	    || mosaic->matches[i].match.pixel->library->path == 0)
	{
	    error_report(ERROR_METAPIXEL_NOT_IN_SAVED_LIBRARY,
			 error_make_string_info(mosaic->matches[i].match.pixel->name));
	    return 0;
	}

    /* FIXME: handle write errors */
    fprintf(out, "(collage-mosaic (input-size %d %d) (metapixels \n",
	    mosaic->in_image_width, mosaic->in_image_height);
    for (i = 0; i < mosaic->num_matches; ++i)
    {
	collage_match_t *match = &mosaic->matches[i];
	lisp_object_t *library_obj = lisp_make_string(match->match.pixel->library->path);
	lisp_object_t *filename_obj = lisp_make_string(match->match.pixel->filename);

	fprintf(out, "(%u %u %u %u ", match->x, match->y, match->width, match->height);
	lisp_dump(library_obj, out);
	fprintf(out, " ");
	lisp_dump(filename_obj, out);
	fprintf(out, " %.3f)\n", match->match.score);

	lisp_free(library_obj);
	lisp_free(filename_obj);
    }
    fprintf(out, "))\n");

    return 1;
}
