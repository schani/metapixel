/* -*- c -*- */

/*
 * readimage.c
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
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "readimage.h"
#include "rwjpeg.h"
#include "rwpng.h"

image_reader_t*
open_image_reading (char *filename)
{
    unsigned char magic[4];
    FILE *file;
    void *data;
    int width, height;
    image_reader_t *reader;
    image_read_func_t read_func;
    image_reader_free_func_t free_func;

    file = fopen(filename, "r");
    if (file == 0)
	return 0;

    if (fread(magic, 4, 1, file) != 1)
    {
	fclose(file);
	return 0;
    }

    fclose(file);

    if (memcmp(magic, "\xff\xd8", 2) == 0)
    {
	data = open_jpeg_file(filename, &width, &height);
	read_func = jpeg_read_lines;
	free_func = jpeg_free_data;
    }
    else if (memcmp(magic, "\x89PNG", 4) == 0)
    {
	data = open_png_file_reading(filename, &width, &height);
	read_func = png_read_lines;
	free_func = png_free_reader_data;
    }
    else
	return 0;

    if (data == 0)
	return 0;

    reader = (image_reader_t*)malloc(sizeof(image_reader_t));
    assert(reader != 0);

    reader->width = width;
    reader->height = height;
    reader->num_lines_read = 0;
    reader->data = data;
    reader->read_func = read_func;
    reader->free_func = free_func;

    return reader;
}

void
read_lines (image_reader_t *reader, unsigned char *lines, int num_lines)
{
    assert(reader->num_lines_read + num_lines <= reader->height);

    reader->read_func(reader->data, lines, num_lines);
    reader->num_lines_read += num_lines;
}

void
free_image_reader (image_reader_t *reader)
{
    reader->free_func(reader->data);
    free(reader);
}

unsigned char*
read_image (char *filename, int *width, int *height)
{
    image_reader_t *reader = open_image_reading(filename);
    unsigned char *data;

    if (reader == 0)
	return 0;

    *width = reader->width;
    *height = reader->height;

    data = (unsigned char*)malloc(*width * *height * 3);

    read_lines(reader, data, *height);

    free_image_reader(reader);

    return data;
}
