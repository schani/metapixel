/* -*- c -*- */

/*
 * rwpng.c
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

#include <assert.h>
#include <stdlib.h>

#include <png.h>

#include "rwpng.h"

typedef struct
{
    FILE *file;
    png_structp png_ptr;
    png_infop info_ptr, end_info;
} png_data_t;

void*
open_png_file (char *filename, int *width, int *height)
{
    png_data_t *data = (png_data_t*)malloc(sizeof(png_data_t));

    assert(data != 0);

    data->file = fopen(filename, "r");
    assert(data->file != 0);

    data->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    assert(data->png_ptr != 0);

    data->info_ptr = png_create_info_struct(data->png_ptr);
    assert(data->info_ptr != 0);

    data->end_info = png_create_info_struct(data->png_ptr);
    assert(data->end_info != 0);

    if (setjmp(data->png_ptr->jmpbuf))
	assert(0);

    png_init_io(data->png_ptr, data->file);

    png_read_info(data->png_ptr, data->info_ptr);

    *width = data->info_ptr->width;
    *height = data->info_ptr->height;

    assert(data->info_ptr->bit_depth == 8 || data->info_ptr->bit_depth == 16);
    assert(data->info_ptr->color_type == PNG_COLOR_TYPE_RGB || data->info_ptr->color_type == PNG_COLOR_TYPE_RGB_ALPHA);
    assert(data->info_ptr->interlace_type == PNG_INTERLACE_NONE);

    return data;
}

void
png_read_lines (void *_data, unsigned char *lines, int num_lines)
{
    png_data_t *data = (png_data_t*)_data;
    int i;
    int bps, spp;
    unsigned char *row;

    if (setjmp(data->png_ptr->jmpbuf))
	assert(0);

    if (data->info_ptr->color_type == PNG_COLOR_TYPE_RGB)
	spp = 3;
    else
	spp = 4;

    if (data->info_ptr->bit_depth == 16)
	bps = 2;
    else
	bps = 1;

    row = (unsigned char*)malloc(data->info_ptr->width * spp * bps);

    for (i = 0; i < num_lines; ++i)
    {
	int j, channel;

	png_read_row(data->png_ptr, (png_bytep)row, 0);
	for (j = 0; j < data->info_ptr->width; ++j)
	    for (channel = 0; channel < 3; ++channel)
		lines[i * data->info_ptr->width * 3 + j * 3 + channel] = row[j * spp * bps + channel * bps];
    }

    free(row);
}

void
png_free_data (void *_data)
{
    png_data_t *data = (png_data_t*)_data;

    if (setjmp(data->png_ptr->jmpbuf))
	assert(0);

    png_read_end(data->png_ptr, data->end_info);
    png_destroy_read_struct(&data->png_ptr, &data->info_ptr, &data->end_info);
    fclose(data->file);

    free(data);
}

void
write_png_file (char *filename, int width, int height, unsigned char *data)
{
    FILE *outFile = fopen(filename, "w");
    png_structp pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop infoPtr;
    int i;

    assert(outFile != 0);
    assert(pngPtr != 0);

    infoPtr = png_create_info_struct(pngPtr);
    assert(infoPtr != 0);

    if (setjmp(pngPtr->jmpbuf))
	assert(0);

    png_init_io(pngPtr, outFile);

    infoPtr->width = width;
    infoPtr->height = height;
    infoPtr->valid = 0;
    infoPtr->rowbytes = width * 3;
    infoPtr->palette = 0;
    infoPtr->num_palette = 0;
    infoPtr->num_trans = 0;
    infoPtr->bit_depth = 8;
    infoPtr->color_type = PNG_COLOR_TYPE_RGB;
    infoPtr->compression_type = PNG_COMPRESSION_TYPE_BASE;
    infoPtr->filter_type = PNG_FILTER_TYPE_BASE;
    infoPtr->interlace_type = PNG_INTERLACE_NONE;

    png_write_info(pngPtr, infoPtr);

    for (i = 0; i < height; ++i)
	png_write_row(pngPtr, (png_bytep)(data + i * 3 * width));

    png_write_end(pngPtr, infoPtr);
    png_destroy_write_struct(&pngPtr, &infoPtr);
    fclose(outFile);
}
