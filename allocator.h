/*
 * allocator.h
 *
 * lispreader
 *
 * Copyright (C) 2004 Mark Probst
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#include <stdlib.h>

#include <pools.h>

typedef struct
{
    void* (*alloc) (void *allocator_data, size_t size);
    void (*free) (void *allocator_data, void *chunk);
    void *allocator_data;
} allocator_t;

extern allocator_t malloc_allocator;

void init_pools_allocator (allocator_t *allocator, pools_t *pools);

#define allocator_alloc(a,s)      ((a)->alloc((a)->allocator_data, (s)))
#define allocator_free(a,c)       ((a)->free((a)->allocator_data, (c)))

char* allocator_strdup (allocator_t *allocator, const char *str);

#endif
