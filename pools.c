/*
 * pools.c
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

#include <string.h>
#include <assert.h>

#include "pools.h"

void
init_pools (pools_t *pools)
{
    int i;

    pools->active_pool = -1;
    pools->fill_ptr = 0;

    for (i = 0; i < NUM_POOLS; ++i)
	pools->pools[i] = 0;
}

void
reset_pools (pools_t *pools)
{
    pools->active_pool = -1;
    pools->fill_ptr = 0;
}

void
free_pools (pools_t *pools)
{
    int i;

    /* printf("alloced %d pools\n", active_pool + 1); */
    for (i = 0; i < NUM_POOLS; ++i)
	if (pools->pools[i] != 0)
	    free(pools->pools[i]);

    init_pools(pools);
}

void*
pools_alloc (pools_t *pools, size_t size)
{
    size_t pool_size;
    void *p;

    if (pools->active_pool < 0)
    {
	pools->active_pool = 0;
	if (pools->pools[0] == 0)
	    pools->pools[0] = (long*)malloc(GRANULARITY * FIRST_POOL_SIZE);
	pools->fill_ptr = 0;

	memset(pools->pools[0], 0, GRANULARITY * FIRST_POOL_SIZE);
    }

    pool_size = FIRST_POOL_SIZE << pools->active_pool;
    size = (size + GRANULARITY - 1) / GRANULARITY;

    if (pools->fill_ptr + size >= pool_size)
    {
	++pools->active_pool;
	assert(pools->active_pool < NUM_POOLS);
	if (pools->pools[pools->active_pool] == 0)
	    pools->pools[pools->active_pool] = (long*)malloc(GRANULARITY * (FIRST_POOL_SIZE << pools->active_pool));
	pools->fill_ptr = 0;

	memset(pools->pools[pools->active_pool], 0, GRANULARITY * (FIRST_POOL_SIZE << pools->active_pool));
    }

    assert(pools->fill_ptr + size < pool_size);

    p = pools->pools[pools->active_pool] + pools->fill_ptr;
    pools->fill_ptr += size;

    return p;
}
