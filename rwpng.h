/* -*- c -*- */

/*
 * rwpng.h
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

#ifndef __RWPNG_H__
#define __RWPNG_H__

void* open_png_file (char *filename, int *width, int *height);
void png_read_lines (void *data, unsigned char *lines, int num_lines);
void png_free_data (void *data);

void write_png_file (char *filename, int width, int height, unsigned char *data);

#endif
