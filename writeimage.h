/* -*- c -*- */

/*
 * writeimage.h
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

#ifndef __WRITEIMAGE_H__
#define __WRITEIMAGE_H__

typedef void (*image_write_func_t) (void *data, unsigned char *lines, int num_lines);
typedef void (*image_writer_free_func_t) (void *data);

typedef struct
{
    int width;
    int height;
    int num_lines_written;
    void *data;
    image_write_func_t write_func;
    image_writer_free_func_t free_func;
} image_writer_t;

#define IMAGE_FORMAT_PNG    1
#define IMAGE_FORMAT_JPEG   2

image_writer_t* open_image_writing (char *filename, int width, int height, int format);
void write_lines (image_writer_t *writer, unsigned char *lines, int num_lines);
void free_image_writer (image_writer_t *writer);

void write_image (char *filename, int width, int height, unsigned char *lines, int format);

#endif
