/*
 * matcher.c
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

#include "api.h"

matcher_t*
matcher_init_local (matcher_t *matcher, metric_t *metric, unsigned int min_distance)
{
    matcher->kind = MATCHER_LOCAL;
    matcher->metric = *metric;
    matcher->v.local.min_distance = min_distance;

    return matcher;
}

matcher_t*
matcher_init_global (matcher_t *matcher, metric_t *metric)
{
    matcher->kind = MATCHER_GLOBAL;
    matcher->metric = *metric;

    return matcher;
}
