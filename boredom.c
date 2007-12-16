/*
 * boredom.c
 *
 * metapixel
 *
 * Copyright (C) 2005 Mark Probst
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
#include <string.h>

#include "readimage.h"

#define SQR(x)       ((x) * (x))

unsigned char *data;
int width, height;
int num_pixels;

int
tiling_variance (int num_x_tiles, int num_y_tiles)
{
    int num_tiles = num_x_tiles * num_y_tiles;
    int tile_width = width / num_x_tiles;
    int tile_height = height / num_y_tiles;
    int x_tile, y_tile, x, y;
    unsigned char *avg_tile = (unsigned char*)malloc(tile_width * tile_height * 3);
    long long var_sum;

    assert(avg_tile != 0);

    for (y = 0; y < tile_height; ++y)
	for (x = 0; x < tile_width; ++x)
	{
	    int r_sum = 0, g_sum = 0, b_sum = 0;

	    for (y_tile = 0; y_tile < num_y_tiles; ++y_tile)
		for (x_tile = 0; x_tile < num_x_tiles; ++x_tile)
		{
		    int abs_x = x_tile * width / num_x_tiles + x;
		    int abs_y = y_tile * height / num_y_tiles + y;

		    r_sum += data[(abs_y * width + abs_x) * 3 + 0];
		    g_sum += data[(abs_y * width + abs_x) * 3 + 1];
		    b_sum += data[(abs_y * width + abs_x) * 3 + 2];
		}

	    avg_tile[(y * tile_width + x) * 3 + 0] = r_sum / num_tiles;
	    avg_tile[(y * tile_width + x) * 3 + 1] = g_sum / num_tiles;
	    avg_tile[(y * tile_width + x) * 3 + 2] = b_sum / num_tiles;
	}

    var_sum = 0;
    for (y_tile = 0; y_tile < num_y_tiles; ++y_tile)
	for (x_tile = 0; x_tile < num_x_tiles; ++x_tile)
	{

	    for (y = 0; y < tile_height; ++y)
		for (x = 0; x < tile_width; ++x)
		{
		    int abs_x = x_tile * width / num_x_tiles + x;
		    int abs_y = y_tile * height / num_y_tiles + y;
		    int i;

		    for (i = 0; i < 3; ++i)
			var_sum += SQR((int)data[(abs_y * width + abs_x) * 3 + i]
				       - (int)avg_tile[(y * tile_width + x) * 3 + i]);
		}
	}

    free(avg_tile);

    return var_sum / num_pixels;
}

int
main (int argc, char *argv[])
{
    int i;
    long long r_sum, g_sum, b_sum, var_sum;
    int r_avg, g_avg, b_avg;
    int variance;
    int max_x_tiles, max_y_tiles;
    int min_tiling_variance, min_tiling_variance_x, min_tiling_variance_y;
    int x, y;

    assert(argc == 4);

    max_x_tiles = atoi(argv[1]);
    max_y_tiles = atoi(argv[2]);

    data = read_image(argv[3], &width, &height);
    assert(data != 0);

    num_pixels = width * height;

    r_sum = g_sum = b_sum = 0;
    for (i = 0; i < num_pixels; ++i)
    {
	r_sum += data[i * 3 + 0];
	g_sum += data[i * 3 + 1];
	b_sum += data[i * 3 + 2];
    }

    r_avg = r_sum / num_pixels;
    g_avg = g_sum / num_pixels;
    b_avg = b_sum / num_pixels;

    var_sum = 0;
    for (i = 0; i < num_pixels; ++i)
	var_sum += SQR((int)data[i * 3 + 0] - r_avg)
	    + SQR((int)data[i * 3 + 1] - g_avg)
	    + SQR((int)data[i * 3 + 2] - b_avg);
    variance = var_sum / num_pixels;

    min_tiling_variance = -1;
    for (x = 2; x <= max_x_tiles; ++x)
	for (y = 2; y < max_y_tiles; ++y)
	{
	    int v = tiling_variance(x, y);

	    if (min_tiling_variance < 0 || v < min_tiling_variance)
	    {
		min_tiling_variance = v;
		min_tiling_variance_x = x;
		min_tiling_variance_y = y;
	    }
	}

    printf("%d  %d (%dx%d)\n", variance, min_tiling_variance, min_tiling_variance_x, min_tiling_variance_y);

    return 0;
}
