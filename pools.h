/*
 * pools.h
 *
 * MathMap
 *
 * Copyright (C) 2002-2004 Mark Probst
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

#ifndef __POOLS_H__
#define __POOLS_H__

#include <stdlib.h>

/* these settings allow a pools to grow to up to 16 GB (last pool 8GB) */
#define GRANULARITY                sizeof(long)
#define FIRST_POOL_SIZE            ((size_t)2048)
#define NUM_POOLS                  20

typedef struct
{
    int active_pool;
    size_t fill_ptr;
    long *pools[NUM_POOLS];
} pools_t;

void init_pools (pools_t *pools);
void reset_pools (pools_t *pools);
void free_pools (pools_t *pools);

void* pools_alloc (pools_t *pools, size_t size);

#endif
