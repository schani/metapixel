/* -*- c -*- */

/*
 * zoom.h
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

#ifndef __ZOOM_H__
#define __ZOOM_H__

typedef float (*filter_func_t) (float);

typedef struct
{
    filter_func_t func;
    float support_radius;
} filter_t;

#define FILTER_BOX           0
#define FILTER_TRIANGLE      1
#define FILTER_MITCHELL      2

filter_t* get_filter (int index);

void zoom_image (unsigned char *dest, unsigned char *src,
		 filter_t *filter, int num_channels,
		 int dest_width, int dest_height, int dest_row_stride,
		 int src_width, int src_height, int src_row_stride);

#endif
