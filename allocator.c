/*
 * allocator.c
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

#include <allocator.h>

#include <stdlib.h>
#include <string.h>

static void*
malloc_allocator_alloc (void *allocator_data, size_t size)
{
    return malloc(size);
}

static void
malloc_allocator_free (void *allocator_data, void *chunk)
{
    free(chunk);
}

allocator_t malloc_allocator = { malloc_allocator_alloc, malloc_allocator_free, 0 };

static void
pools_allocator_free (void *allocator_data, void *chunk)
{
}

void
init_pools_allocator (allocator_t *allocator, pools_t *pools)
{
    allocator->alloc = (void* (*) (void*, size_t))pools_alloc;
    allocator->free = pools_allocator_free;
    allocator->allocator_data = pools;
}

char*
allocator_strdup (allocator_t *allocator, const char *str)
{
    size_t len = strlen(str) + 1;
    char *copy = (char*)allocator_alloc(allocator, len);

    if (copy != 0)
	memcpy(copy, str, len);

    return copy;
}
