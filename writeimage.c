/* -*- c -*- */

/*
 * writeimage.c
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

#include <assert.h>
#include <stdlib.h>

#include "rwpng.h"

#include "writeimage.h"

image_writer_t*
open_image_writing (char *filename, int width, int height, int format)
{
    image_writer_t *writer;
    void *data = 0;
    image_write_func_t write_func = 0;
    image_writer_free_func_t free_func = 0;

    if (format == IMAGE_FORMAT_PNG)
    {
	data = open_png_file_writing(filename, width, height);
	write_func = png_write_lines;
	free_func = png_free_writer_data;
    }
    else
	assert(0);

    if (data == 0)
	return 0;

    writer = (image_writer_t*)malloc(sizeof(image_writer_t));
    writer->width = width;
    writer->height = height;
    writer->num_lines_written = 0;
    writer->data = data;
    writer->write_func = write_func;
    writer->free_func = free_func;

    return writer;
}

void
write_lines (image_writer_t *writer, unsigned char *lines, int num_lines)
{
    assert(writer->num_lines_written + num_lines <= writer->height);

    writer->write_func(writer->data, lines, num_lines);
}

void
free_image_writer (image_writer_t *writer)
{
    writer->free_func(writer->data);
    free(writer);
}

void
write_image (char *filename, int width, int height, unsigned char *lines, int format)
{
    image_writer_t *writer = open_image_writing(filename, width, height, format);

    assert(writer != 0);

    write_lines(writer, lines, height);
    free_image_writer(writer);
}
