/*
 * bitmap.c
 *
 * metapixel
 *
 * Copyright (C) 2004-2007 Mark Probst
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
#include <stdlib.h>
#include <string.h>

#include "rwimg/readimage.h"
#include "rwimg/writeimage.h"

#include "api.h"

static unsigned int
color_channels (int color)
{
    assert(color == COLOR_RGB_8);

    return 3;
}

static void
ref_bitmap (bitmap_t *bitmap)
{
    assert(bitmap->refcount != 0);
    if (bitmap->refcount > 0)
	++bitmap->refcount;
    else
	--bitmap->refcount;
}

bitmap_t*
bitmap_new (int color, unsigned int width, unsigned int height,
	    unsigned int pixel_stride, unsigned int row_stride, unsigned char *data)
{
    bitmap_t *bitmap = (bitmap_t*)malloc(sizeof(bitmap_t));

    assert(bitmap != 0);

    assert(color == COLOR_RGB_8);
    assert(width > 0 && height > 0);
    assert(pixel_stride >= color_channels(color));
    assert(row_stride >= pixel_stride * width);
    assert(data != 0);

    bitmap->color = color;
    bitmap->width = width;
    bitmap->height = height;
    bitmap->pixel_stride = pixel_stride;
    bitmap->row_stride = row_stride;
    bitmap->data = data;
    bitmap->refcount = 1;
    bitmap->super = 0;

    return bitmap;
}

bitmap_t*
bitmap_new_copying (int color, unsigned int width, unsigned int height,
		    unsigned int pixel_stride, unsigned int row_stride, unsigned char *data)
{
    unsigned char *new_data = (unsigned char*)malloc(height * row_stride);

    assert(new_data != 0);

    memcpy(new_data, data, height * row_stride);

    return bitmap_new(color, width, height, pixel_stride, row_stride, new_data);
}

bitmap_t*
bitmap_new_dont_possess (int color, unsigned int width, unsigned int height,
			 unsigned int pixel_stride, unsigned int row_stride, unsigned char *data)
{
    bitmap_t *bitmap = bitmap_new(color, width, height, pixel_stride, row_stride, data);

    assert(bitmap != 0);

    bitmap->refcount = -1;

    return bitmap;
}

bitmap_t*
bitmap_new_packed (int color, unsigned int width, unsigned int height,
		   unsigned char *data)
{
    unsigned int num_channels = color_channels(color);

    return bitmap_new(color, width, height, num_channels, width * num_channels, data);
}

bitmap_t*
bitmap_new_empty (int color, unsigned int width, unsigned int height)
{
    unsigned int size = width * height * color_channels(color);
    unsigned char *data = (unsigned char*)malloc(size);

    assert(data != 0);

    // memset(data, 0, size);

    return bitmap_new_packed(color, width, height, data);
}

void
bitmap_free (bitmap_t *bitmap)
{
    assert(bitmap->refcount != 0);

    if (bitmap->refcount > 0)
    {
	--bitmap->refcount;

	if (bitmap->refcount == 0)
	{
	    if (bitmap->super != 0)
		bitmap_free(bitmap->super);
	    else
		free(bitmap->data);
	    free(bitmap);
	}
    }
    else
    {
	++bitmap->refcount;

	assert(bitmap->super == 0);

	if (bitmap->refcount == 0)
	    free(bitmap);
    }
}

bitmap_t*
bitmap_sub (bitmap_t *super, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
    bitmap_t *bitmap;

    assert(width > 0 && height > 0);
    assert(x + width <= super->width);
    assert(y + height <= super->height);

    bitmap = (bitmap_t*)malloc(sizeof(bitmap_t));
    assert(bitmap != 0);

    bitmap->color = super->color;
    bitmap->width = width;
    bitmap->height = height;
    bitmap->pixel_stride = super->pixel_stride;
    bitmap->row_stride = super->row_stride;

    bitmap->data = super->data + y * super->row_stride + x * super->pixel_stride;

    bitmap->refcount = 1;
    bitmap->super = super;

    ref_bitmap(super);

    return bitmap;
}

bitmap_t*
bitmap_scale (bitmap_t *src, unsigned int scaled_width, unsigned int scaled_height, int filter_id)
{
    filter_t *filter = get_filter(filter_id);
    unsigned int num_channels = color_channels(src->color);
    bitmap_t *bitmap;

    if (filter == 0)
	return 0;

    bitmap = bitmap_new_empty(src->color, scaled_width, scaled_height);
    assert(bitmap != 0);

    zoom_image(bitmap->data, src->data, filter, num_channels,
	       scaled_width, scaled_height, bitmap->pixel_stride, bitmap->row_stride,
	       src->width, src->height, src->pixel_stride, src->row_stride);

    return bitmap;
}

bitmap_t*
bitmap_copy (bitmap_t *bitmap)
{
    ref_bitmap(bitmap);

    return bitmap;
}

bitmap_t*
bitmap_read (const char *filename)
{
    int width, height;
    unsigned char *data = read_image(filename, &width, &height);

    if (data != 0)
    {
	bitmap_t *bitmap = bitmap_new(COLOR_RGB_8, width, height, 3, width * 3, data);

	assert(bitmap != 0);

	return bitmap;
    }
    else
	return 0;
}

int
bitmap_write (bitmap_t *bitmap, const char *filename)
{
    assert(bitmap->color == COLOR_RGB_8);
    assert(bitmap->pixel_stride == 3);

    /* FIXME: implement error reporting in write_image */
    write_image(filename, bitmap->width, bitmap->height, bitmap->data,
		bitmap->pixel_stride, bitmap->row_stride, IMAGE_FORMAT_PNG);

    return 1;
}

void
bitmap_paste (bitmap_t *dst, bitmap_t *src, unsigned int x, unsigned int y)
{
    unsigned int i;

    assert(dst->color == src->color);
    assert(dst->pixel_stride == src->pixel_stride);

    assert(x + src->width <= dst->width);
    assert(y + src->height <= dst->height);

    for (i = 0; i < src->height; ++i)
	memcpy(dst->data + x * dst->pixel_stride + (y + i) * dst->row_stride,
	       src->data + i * src->row_stride,
	       src->width * src->pixel_stride);
}

void
bitmap_alpha_compose (bitmap_t *dst, bitmap_t *src, unsigned int opacity)
{
    unsigned int num_channels = color_channels(dst->color);
    unsigned int row, column;

    assert(dst->color == src->color);
    assert(dst->width == src->width && dst->height == src->height);

    for (row = 0; row < dst->height; ++row)
    {
	unsigned char *dst_data = dst->data + row * dst->row_stride;
	unsigned char *src_data = src->data + row * src->row_stride;

	for (column = 0; column < dst->width; ++column)
	{
	    int i;

	    for (i = 0; i < num_channels; ++i)
		dst_data[i] = (((unsigned int)dst_data[i] * (0x10000 - opacity)) >> 16)
		    + (((unsigned int)src_data[i] * opacity) >> 16);

	    src_data += src->pixel_stride;
	    dst_data += dst->pixel_stride;
	}
    }
}
