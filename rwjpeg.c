/* -*- c -*- */

/*
 * rwjpeg.c
 *
 * metapixel
 *
 * Copyright (C) 2000 Mark Probst
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

JSAMPLE *image = 0;
int imageLength;

unsigned char*
read_jpeg_file (char *filename, int *width, int *height)
{
    struct jpeg_decompress_struct cinfo;
    FILE * infile;
    struct jpeg_error_mgr jerr;
    int row_stride, i;
    unsigned char *image_data;

    infile = fopen(filename, "r");
    assert(infile != 0);

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);

    if (cinfo.num_components == 1)
	cinfo.out_color_space = JCS_GRAYSCALE;
    else if (cinfo.num_components == 3)
	cinfo.out_color_space = JCS_RGB;
    else
	assert(0);

    *width = cinfo.image_width;
    *height = cinfo.image_height;

    jpeg_start_decompress(&cinfo);

    row_stride = cinfo.image_width * 3;
    image_data = (unsigned char*)malloc(row_stride * *height);

    for (i = 0; i < cinfo.image_height; ++i)
    {
	unsigned char *scanline = image_data + i * row_stride;

	jpeg_read_scanlines(&cinfo, &scanline, 1);

	if (cinfo.num_components == 1)
	{
	    int j;

	    for (j = *width - 1; j >= 0; --j)
	    {
		unsigned char value = scanline[j];
		int k;

		for (k = 0; k < 3; ++k)
		    scanline[j * 3 + k] = value;
	    }
	}
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    fclose(infile);

    return image_data;
}
