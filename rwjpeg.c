/* -*- c -*- */

/*
 * rwjpeg.c
 *
 * metapixel
 *
 * Copyright (C) 2000-2004 Mark Probst
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
#include <assert.h>
#include <stdlib.h>

#include <jpeglib.h>

typedef struct
{
    FILE *file;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
} jpeg_data_t;

void*
open_jpeg_file (const char *filename, int *width, int *height)
{
    jpeg_data_t *data = (jpeg_data_t*)malloc(sizeof(jpeg_data_t));

    assert(data != 0);

    data->file = fopen(filename, "r");
    assert(data->file != 0);

    data->cinfo.err = jpeg_std_error(&data->jerr);
    jpeg_create_decompress(&data->cinfo);
    jpeg_stdio_src(&data->cinfo, data->file);
    jpeg_read_header(&data->cinfo, TRUE);

    if (data->cinfo.num_components == 1)
	data->cinfo.out_color_space = JCS_GRAYSCALE;
    else if (data->cinfo.num_components == 3)
	data->cinfo.out_color_space = JCS_RGB;
    else
	assert(0);

    *width = data->cinfo.image_width;
    *height = data->cinfo.image_height;

    jpeg_start_decompress(&data->cinfo);

    return data;
}

void
jpeg_read_lines (void *_data, unsigned char *lines, int num_lines)
{
    jpeg_data_t *data = (jpeg_data_t*)_data;
    int row_stride, i;

    row_stride = data->cinfo.image_width * 3;

    for (i = 0; i < num_lines; ++i)
    {
	unsigned char *scanline = lines + i * row_stride;

	jpeg_read_scanlines(&data->cinfo, &scanline, 1);

	if (data->cinfo.num_components == 1)
	{
	    int j;

	    for (j = data->cinfo.image_width - 1; j >= 0; --j)
	    {
		unsigned char value = scanline[j];
		int k;

		for (k = 0; k < 3; ++k)
		    scanline[j * 3 + k] = value;
	    }
	}
    }
}

void
jpeg_free_data (void *_data)
{
    jpeg_data_t *data = (jpeg_data_t*)_data;

    jpeg_finish_decompress(&data->cinfo);
    jpeg_destroy_decompress(&data->cinfo);

    fclose(data->file);

    free(data);
}
