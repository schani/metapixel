/*
 * metapixel.h
 *
 * metapixel
 *
 * Copyright (C) 1997-2007 Mark Probst
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

#include "rwimg/readimage.h"

#define DEFAULT_WIDTH       64
#define DEFAULT_HEIGHT      64

#define DEFAULT_PREPARE_WIDTH       128
#define DEFAULT_PREPARE_HEIGHT      128

#define DEFAULT_CLASSIC_MIN_DISTANCE     5
#define DEFAULT_COLLAGE_MIN_DISTANCE   256

#define SEARCH_LOCAL         1
#define SEARCH_GLOBAL        2

#include "api.h"

typedef struct _string_list_t
{
    char *str;
    struct _string_list_t *next;
} string_list_t;

#endif
