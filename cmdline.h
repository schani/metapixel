/*
 * metapixel.h
 *
 * metapixel
 *
 * Copyright (C) 1997-2004 Mark Probst
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

#ifndef __METAPIXEL_H__
#define __METAPIXEL_H__

#include "readimage.h"

#define DEFAULT_WIDTH       64
#define DEFAULT_HEIGHT      64

#define DEFAULT_PREPARE_WIDTH       128
#define DEFAULT_PREPARE_HEIGHT      128

#define DEFAULT_CLASSIC_MIN_DISTANCE     5
#define DEFAULT_COLLAGE_MIN_DISTANCE   256

#define SEARCH_LOCAL    1
#define SEARCH_GLOBAL   2

typedef struct
{
    int flag;
} client_metapixel_data_t;

#define CLIENT_METAPIXEL_DATA_T client_metapixel_data_t

#include "api.h"

typedef struct
{
    int metawidth;
    int metaheight;
    match_t *matches;
} mosaic_t;

typedef struct
{
    image_reader_t *image_reader;
    int in_image_width;
    int in_image_height;
    int out_image_width;
    int out_image_height;
    int metawidth;
    int metaheight;
    int y;
    int num_lines;
    bitmap_t *in_image;
} classic_reader_t;

typedef struct _string_list_t
{
    char *str;
    struct _string_list_t *next;
} string_list_t;

#endif
