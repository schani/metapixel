/* -*- c -*- */

/*
 * readimage.h
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

#ifndef __READIMAGE_H__
#define __READIMAGE_H__

typedef void (*image_read_func_t) (void *data, unsigned char *lines, int num_lines);
typedef void (*image_reader_free_func_t) (void *data);

typedef struct
{
    int width;
    int height;
    int num_lines_read;
    void *data;
    image_read_func_t read_func;
    image_reader_free_func_t free_func;
} image_reader_t;

image_reader_t* open_image_reading (const char *filename);
void read_lines (image_reader_t *reader, unsigned char *lines, int num_lines);
void free_image_reader (image_reader_t *reader);

unsigned char* read_image (const char *filename, int *width, int *height);

#endif
