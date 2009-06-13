/*
 * millions.c
 *
 * metapixel
 *
 * Copyright (C) 2009 Mark Probst
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

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "lispreader/pools.h"

#include "api.h"

#if NUM_CHANNELS != 3
#error only works with 3 channels
#endif

typedef struct _pixel_list_t
{
    unsigned char color[NUM_CHANNELS];
    gpointer data;
    int length;
    struct _pixel_list_t *next;
} pixel_list_t;

typedef struct
{
    int row;
    int col;
    metapixel_t *metapixel;
} pixel_assignment_t;

typedef struct
{
    pixel_list_t *big_pixels;
    pixel_list_t *small_pixels;
} subcube_t;

typedef struct
{
    pixel_list_t *pixel;
    float weight;
} pixel_with_weight_t;

static pixel_list_t*
cons_pixel (pixel_list_t *pixel, pixel_list_t *list)
{
    pixel->next = list;
    if (list != NULL)
	pixel->length = list->length + 1;
    else
	pixel->length = 1;
    return pixel;
}

static int
num_pixels (pixel_list_t *list)
{
    if (list == NULL)
	return 0;
    return list->length;
}

static pixel_list_t*
subcube_get_pixels (subcube_t *subcube, gboolean is_big)
{
    if (is_big)
	return subcube->big_pixels;
    else
	return subcube->small_pixels;
}

static void
subcube_add_pixel (subcube_t *subcube, pixel_list_t *pixel, gboolean is_big)
{
    if (is_big)
	subcube->big_pixels = cons_pixel(pixel, subcube->big_pixels);
    else
	subcube->small_pixels = cons_pixel(pixel, subcube->small_pixels);
}

static void
categorize_pixels (pixel_list_t *pixels, gboolean is_big,
		   subcube_t subcubes[2][2][2],
		   int x_start, int x_dim,
		   int y_start, int y_dim,
		   int z_start, int z_dim)
{
    pixel_list_t *pixel;

    pixel = pixels;
    while (pixel != NULL)
    {
	pixel_list_t *next = pixel->next;
	int x = (pixel->color[0] >= x_start + (x_dim >> 1)) ? 1 : 0;
	int y = (pixel->color[1] >= y_start + (y_dim >> 1)) ? 1 : 0;
	int z = (pixel->color[2] >= z_start + (z_dim >> 1)) ? 1 : 0;

	subcube_add_pixel(&subcubes[x][y][z], pixel, is_big);

	pixel = next;
    }
}

static int
compare_pixel_with_weights (const void *p1, const void *p2)
{
    const pixel_with_weight_t *pw1 = p1;
    const pixel_with_weight_t *pw2 = p2;

    if (pw1->weight < pw2->weight)
	return -1;
    if (pw1->weight > pw2->weight)
	return 1;
    return 0;
}

/* assumes the pixel is not linked */
static void
redistribute_pixel (pixel_list_t *pixel, subcube_t subcubes[2][2][2], gboolean to_big,
		    int forbid_x, int forbid_y, int forbid_z,
		    int x_start, int x_dim,
		    int y_start, int y_dim,
		    int z_start, int z_dim)
{
    int x, y, z;
    int best_x, best_y, best_z, best_dist;

    best_x = best_y = best_z = best_dist = -1;

    for (x = 0; x < 2; ++x)
	for (y = 0; y < 2; ++y)
	    for (z = 0; z < 2; ++z)
	    {
		subcube_t *subcube = &subcubes[x][y][z];

		if (x == forbid_x && y == forbid_y && z == forbid_z)
		    continue;
		if (to_big && num_pixels(subcube->small_pixels) == 0)
		    continue;

		if (to_big || num_pixels(subcube->small_pixels) < num_pixels(subcube->big_pixels))
		{
		    int sub_x = x_start + x * (x_dim >> 1) + (x_dim >> 2);
		    int sub_y = y_start + y * (y_dim >> 1) + (y_dim >> 2);
		    int sub_z = z_start + z * (z_dim >> 1) + (z_dim >> 2);
		    /* FIXME: use Euclid instead of Manhattan */
		    int dist = ABS(pixel->color[0] - sub_x) + ABS(pixel->color[1] - sub_y) + ABS(pixel->color[2] - sub_z);

		    if (best_dist < 0 || dist < best_dist)
		    {
			best_x = x;
			best_y = y;
			best_z = z;
			best_dist = dist;
		    }
		}
	    }

    g_assert(best_dist >= 0);

    subcube_add_pixel(&subcubes[best_x][best_y][best_z], pixel, to_big);
}

static void
redistribute_pixels (subcube_t subcubes[2][2][2],
		     gboolean redistribute_big,
		     int x, int y, int z,
		     int x_start, int x_dim,
		     int y_start, int y_dim,
		     int z_start, int z_dim)
{
    subcube_t *from_cube = &subcubes[x][y][z];
    pixel_list_t *from_pixels = subcube_get_pixels(from_cube, redistribute_big);
    int num_from_pixels = num_pixels(from_pixels);
    int num_pixels_to_redistribute = num_from_pixels - num_pixels(subcube_get_pixels(from_cube, !redistribute_big));
    pixel_with_weight_t *pixel_weight_table;
    pixel_list_t *pixel;
    int i;

    g_assert(num_pixels_to_redistribute > 0);

    pixel_weight_table = g_new(pixel_with_weight_t, num_from_pixels);

    i = 0;
    for (pixel = from_pixels; pixel != NULL; pixel = pixel->next)
    {
	int weight;

	/*
	 * FIXME: for each of those we could first check whether the
	 * subcube bordering on that side has still room for small
	 * pixels and ignore the weight if it doesn't.
	 *
	 * If no bordering subcube has room we could take the distance
	 * to the center of the cube as the weight.
	 */
	weight = ABS(pixel->color[0] - (x_start + (x_dim >> 1)));
	weight = MIN(weight, ABS(pixel->color[1] - (y_start + (y_dim >> 1))));
	weight = MIN(weight, ABS(pixel->color[2] - (z_start + (z_dim >> 1))));

	pixel_weight_table[i].pixel = pixel;
	pixel_weight_table[i].weight = weight;

	++i;
    }
    g_assert(i == num_from_pixels);

    qsort(pixel_weight_table, num_from_pixels, sizeof(pixel_with_weight_t), compare_pixel_with_weights);

    if (redistribute_big)
	from_cube->big_pixels = NULL;
    else
	from_cube->small_pixels = NULL;
    for (i = num_pixels_to_redistribute; i < num_from_pixels; ++i)
	subcube_add_pixel(from_cube, pixel_weight_table[i].pixel, redistribute_big);
    g_assert(num_pixels(from_cube->small_pixels) == num_pixels(from_cube->big_pixels));

    for (i = 0; i < num_pixels_to_redistribute; ++i)
	redistribute_pixel(pixel_weight_table[i].pixel, subcubes, redistribute_big,
			   x, y, z,
			   x_start, x_dim,
			   y_start, y_dim,
			   z_start, z_dim);

    g_free(pixel_weight_table);
}

static void
assign_pixels (pixel_list_t *small_pixels, pixel_list_t *big_pixels)
{
    pixel_list_t *small_pixel = NULL;
    pixel_list_t *big_pixel;

    for (big_pixel = big_pixels; big_pixel != NULL; big_pixel = big_pixel->next)
    {
	pixel_assignment_t *assignment = big_pixel->data;

	if (small_pixel == NULL)
	    small_pixel = small_pixels;

	g_assert(assignment->metapixel == NULL);
	assignment->metapixel = small_pixel->data;

	small_pixel = small_pixel->next;
    }
}

static void
indent (int depth)
{
    int i;

    for (i = 0; i < depth; ++i)
	g_print("  ");
}

static void
process_cube (subcube_t *cube,
	      int depth, int max_depth,
	      int x_start, int x_dim,
	      int y_start, int y_dim,
	      int z_start, int z_dim)
{
    subcube_t subcubes[2][2][2];
    int x, y, z;

    if (depth >= max_depth || num_pixels(cube->small_pixels) == 1)
    {
	assign_pixels(cube->small_pixels, cube->big_pixels);
	return;
    }

    g_assert(num_pixels(cube->small_pixels) <= num_pixels(cube->big_pixels));

    memset(subcubes, 0, sizeof(subcubes));

    categorize_pixels(cube->big_pixels, TRUE, subcubes, x_start, x_dim, y_start, y_dim, z_start, z_dim);
    categorize_pixels(cube->small_pixels, FALSE, subcubes, x_start, x_dim, y_start, y_dim, z_start, z_dim);

    /* First pass: Redistribute small pixels from subcubes which have
       too many. */
    for (x = 0; x < 2; ++x)
	for (y = 0; y < 2; ++y)
	    for (z = 0; z < 2; ++z)
	    {
		subcube_t *subcube = &subcubes[x][y][z];

#ifdef DEBUG_OUTPUT
		indent(depth);
		g_print("first pass %d%d%d - %d small %d big\n", x, y, z,
			num_pixels(subcube->small_pixels), num_pixels(subcube->big_pixels));
#endif

		if (num_pixels(subcube->small_pixels) > num_pixels(subcube->big_pixels))
		    redistribute_pixels(subcubes, FALSE, x, y, z, x_start, x_dim, y_start, y_dim, z_start, z_dim);
	    }

    /* Second pass: Redistribute big pixels from subcubes which don't
       have any small pixels. */
    for (x = 0; x < 2; ++x)
	for (y = 0; y < 2; ++y)
	    for (z = 0; z < 2; ++z)
	    {
		subcube_t *subcube = &subcubes[x][y][z];

#ifdef DEBUG_OUTPUT
		indent(depth);
		g_print("second pass %d%d%d - %d small %d big\n", x, y, z,
			num_pixels(subcube->small_pixels), num_pixels(subcube->big_pixels));
#endif

		if (num_pixels(subcube->small_pixels) == 0 && num_pixels(subcube->big_pixels) > 0)
		    redistribute_pixels(subcubes, TRUE, x, y, z, x_start, x_dim, y_start, y_dim, z_start, z_dim);
	    }

    for (x = 0; x < 2; ++x)
	for (y = 0; y < 2; ++y)
	    for (z = 0; z < 2; ++z)
	    {
		subcube_t *subcube = &subcubes[x][y][z];

#ifdef DEBUG_OUTPUT
		indent(depth);
		g_print("recursing %d%d%d - %d small %d big\n", x, y, z,
			num_pixels(subcube->small_pixels), num_pixels(subcube->big_pixels));
#endif

		g_assert(num_pixels(subcube->small_pixels) <= num_pixels(subcube->big_pixels));
		g_assert(num_pixels(subcube->small_pixels) > 0 || num_pixels(subcube->big_pixels) == 0);
		if (num_pixels(subcube->small_pixels) > 0)
		    process_cube(subcube, depth + 1, max_depth,
				 x_start + x * (x_dim >> 1), x_dim >> 1,
				 y_start + y * (y_dim >> 1), y_dim >> 1,
				 z_start + z * (z_dim >> 1), z_dim >> 1);
	    }
}

static pixel_list_t*
make_pixel_from_metapixel (pools_t *pools, metapixel_t *metapixel)
{
    pixel_list_t *pixel = pools_alloc(pools, sizeof(pixel_list_t));

    memcpy(pixel->color, metapixel->average_rgb, sizeof(pixel->color));
    pixel->data = metapixel;
    pixel->length = 1;
    pixel->next = NULL;

    return pixel;
}

static pixel_list_t*
make_pixel_from_image_pixel (pools_t *pools, unsigned char *color, pixel_assignment_t *assignment)
{
    pixel_list_t *pixel = pools_alloc(pools, sizeof(pixel_list_t));

    memcpy(pixel->color, color, sizeof(pixel->color));
    pixel->data = assignment;
    pixel->length = 1;
    pixel->next = NULL;

    return pixel;
}

bitmap_t*
millions_generate_from_bitmap (int num_libraries, library_t **libraries, bitmap_t *in_image)
{
    pools_t pools;
    subcube_t cube;
    int i, row;
    pixel_assignment_t *pixel_assignments;
    bitmap_t *out_image;
    GTimer *timer;

    cube.big_pixels = NULL;
    cube.small_pixels = NULL;

    init_pools(&pools);

    timer = g_timer_new();

    /* add small pixels */
    for (i = 0; i < num_libraries; ++i)
    {
	metapixel_t *metapixel;

	for (metapixel = libraries[i]->metapixels; metapixel != NULL; metapixel = metapixel->next)
	    subcube_add_pixel(&cube, make_pixel_from_metapixel(&pools, metapixel), FALSE);
    }

    g_print("small pixels added: %f s\n", g_timer_elapsed(timer, NULL));

    /* add big pixels */
    pixel_assignments = g_new(pixel_assignment_t, in_image->width * in_image->height);
    for (row = 0; row < in_image->height; ++row)
    {
	unsigned char *p = in_image->data + row * in_image->row_stride;
	int col;

	for (col = 0; col < in_image->width; ++col)
	{
	    pixel_assignment_t *assignment = &pixel_assignments[row * in_image->width + col];

	    assignment->row = row;
	    assignment->col = col;
	    assignment->metapixel = NULL;

	    subcube_add_pixel(&cube, make_pixel_from_image_pixel(&pools, p + col * in_image->pixel_stride, assignment), TRUE);
	}
    }

    g_print("big pixels added: %f s\n", g_timer_elapsed(timer, NULL));

#ifdef DEBUG_OUTPUT
    g_print("have %d small and %d big pixels\n", num_pixels(cube.small_pixels), num_pixels(cube.big_pixels));
#endif

    process_cube(&cube, 0, 6, 0, 256, 0, 256, 0, 256);

    free_pools(&pools);

    g_print("processed: %f s\n", g_timer_elapsed(timer, NULL));

    out_image = bitmap_new_empty(COLOR_RGB_8, in_image->width, in_image->height);
    g_assert(out_image != NULL);

    for (row = 0; row < out_image->height; ++row)
    {
	unsigned char *p = out_image->data + row * out_image->row_stride;
	int col;

	for (col = 0; col < out_image->width; ++col)
	{
	    pixel_assignment_t *assignment = &pixel_assignments[row * out_image->width + col];

	    g_assert(assignment->metapixel != NULL);

	    memcpy(p + col * out_image->pixel_stride, assignment->metapixel->average_rgb, NUM_CHANNELS);
	}
    }

    g_print("done: %f s\n", g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);

    g_free(pixel_assignments);

    return out_image;
}
