/*
 * tiling.c
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

#include <assert.h>

#include "api.h"

tiling_t*
tiling_init_rectangular (tiling_t *tiling, unsigned int metawidth, unsigned int metaheight)
{
    tiling->kind = TILING_RECTANGULAR;
    tiling->metawidth = metawidth;
    tiling->metaheight = metaheight;

    return tiling;
}

static unsigned int
get_rectangular_coord (unsigned int num_tiles, unsigned int image_size, unsigned int metacoord)
{
    assert(metacoord <= num_tiles);

    /* FIXME: this could overflow */
    return image_size * metacoord / num_tiles;
}

unsigned int
tiling_get_rectangular_x (tiling_t *tiling, unsigned int image_width, unsigned int metapixel_x)
{
    return get_rectangular_coord(tiling->metawidth, image_width, metapixel_x);
}

unsigned int
tiling_get_rectangular_width (tiling_t *tiling, unsigned int image_width, unsigned int metapixel_x)
{
    return tiling_get_rectangular_x(tiling, image_width, metapixel_x + 1)
	- tiling_get_rectangular_x(tiling, image_width, metapixel_x);
}

unsigned int
tiling_get_rectangular_y (tiling_t *tiling, unsigned int image_height, unsigned int metapixel_y)
{
    return get_rectangular_coord(tiling->metaheight, image_height, metapixel_y);
}

unsigned int
tiling_get_rectangular_height (tiling_t *tiling, unsigned int image_height, unsigned int metapixel_y)
{
    return tiling_get_rectangular_y(tiling, image_height, metapixel_y + 1)
	- tiling_get_rectangular_y(tiling, image_height, metapixel_y);
}

void
tiling_get_metapixel_coords (tiling_t *tiling, unsigned int image_width, unsigned int image_height,
			     unsigned int metapixel_x, unsigned int metapixel_y,
			     unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height)
{
    assert(tiling->kind == TILING_RECTANGULAR);

    *x = tiling_get_rectangular_x(tiling, image_width, metapixel_x);
    *y = tiling_get_rectangular_y(tiling, image_height, metapixel_y);
    *width = tiling_get_rectangular_width(tiling, image_width, metapixel_x);
    *height = tiling_get_rectangular_height(tiling, image_height, metapixel_y);
}
