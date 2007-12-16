/*
 * search.c
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

#include <assert.h>
#include <string.h>

#include "api.h"

static int
metapixel_in_array (metapixel_t *pixel, metapixel_t **array, int size)
{
    int i;

    for (i = 0; i < size; ++i)
	if (array[i] == pixel)
	    return 1;
    return 0;
}

metapixel_match_t
search_metapixel_nearest_to (int num_libraries, library_t **libraries,
			     coeffs_union_t *coeffs, metric_t *metric, int x, int y,
			     metapixel_t **forbidden, int num_forbidden,
			     unsigned int forbid_reconstruction_radius, unsigned int allowed_flips,
			     int (*validity_func) (void*, metapixel_t*, unsigned int, int, int),
			     void *validity_func_data)
{
    float best_score = 1e99;
    metapixel_t *best_fit = 0;
    unsigned int best_index = (unsigned int)-1;
    unsigned int best_orientation = 0;
    compare_func_set_t *compare_func_set = metric_compare_func_set_for_metric(metric);
    metapixel_match_t match;
    /* allowed < 0 means we don't know.  0 means not allowed, >0 means allowed.  */
    int allowed;

    void check_orientation (metapixel_t *pixel, unsigned int pixel_index,
			    compare_func_t compare_func, unsigned int orientation)
	{
	    float score;

	    if (allowed == 0)
		return;

	    score = compare_func(coeffs, pixel, best_score, metric->color_space, metric->weights);

	    if (score < best_score)
	    {
		if (allowed < 0)
		    allowed = (!metapixel_in_array(pixel, forbidden, num_forbidden)
			       && (validity_func == 0
				   || validity_func(validity_func_data, pixel, pixel_index, x, y)))
			? 1 : 0;

		assert(allowed >= 0);

		if (allowed)
		{
		    best_score = score;
		    best_fit = pixel;
		    best_index = pixel_index;
		    best_orientation = orientation;
		}
	    }
	}

    FOR_EACH_METAPIXEL(pixel, pixel_index)
    {
	allowed = -1;

	if (pixel->anti_x >= 0 && pixel->anti_y >= 0
	    && (utils_manhattan_distance(x, y, pixel->anti_x, pixel->anti_y)
		< forbid_reconstruction_radius))
	    continue;

	check_orientation(pixel, pixel_index, compare_func_set->compare_no_flip, 0);

	if (pixel->flip & FLIP_HOR & allowed_flips)
	{
	    check_orientation(pixel, pixel_index, compare_func_set->compare_hor_flip, FLIP_HOR);
	    if (pixel->flip & FLIP_VER & allowed_flips)
		check_orientation(pixel, pixel_index, compare_func_set->compare_hor_ver_flip, FLIP_HOR | FLIP_VER);
	}
	if (pixel->flip & FLIP_VER & allowed_flips)
	    check_orientation(pixel, pixel_index, compare_func_set->compare_ver_flip, FLIP_VER);
    }
    END_FOR_EACH_METAPIXEL

    match.pixel = best_fit;
    match.pixel_index = best_index;
    match.orientation = best_orientation;
    match.score = best_score;

    return match;
}

void
search_n_metapixel_nearest_to (int num_libraries, library_t **libraries,
			       int n, global_match_t *matches, coeffs_union_t *coeffs, metric_t *metric,
			       unsigned int allowed_flips)
{
    compare_func_set_t* compare_func_set = metric_compare_func_set_for_metric(metric);
    int i;

    void check_orientation (metapixel_t *pixel, unsigned int pixel_index,
			    compare_func_t compare_func, unsigned int orientation)
	{
	    float score = compare_func(coeffs, pixel, (i < n) ? 1e99 : matches[n - 1].match.score,
				       metric->color_space, metric->weights);

	    if (i < n || score < matches[n - 1].match.score)
	    {
		int j, m;

		m = MIN(i, n);

		for (j = 0; j < m; ++j)
		    if (matches[j].match.score > score)
			break;

		assert(j <= m && j < n);

		memmove(matches + j + 1, matches + j, sizeof(global_match_t) * (MIN(n, m + 1) - (j + 1)));

		matches[j].match.pixel = pixel;
		matches[j].match.orientation = orientation;
		matches[j].match.pixel_index = pixel_index;
		matches[j].match.score = score;
	    }
	}

    i = 0;
    FOR_EACH_METAPIXEL(pixel, pixel_index)
    {
	check_orientation(pixel, pixel_index, compare_func_set->compare_no_flip, 0);
	++i;

	if (pixel->flip & FLIP_HOR & allowed_flips)
	{
	    check_orientation(pixel, pixel_index, compare_func_set->compare_hor_flip, FLIP_HOR);
	    ++i;
	    if (pixel->flip & FLIP_VER & allowed_flips)
	    {
		check_orientation(pixel, pixel_index, compare_func_set->compare_hor_ver_flip, FLIP_HOR | FLIP_VER);
		++i;
	    }
	}
	if (pixel->flip & FLIP_VER & allowed_flips)
	{
	    check_orientation(pixel, pixel_index, compare_func_set->compare_ver_flip, FLIP_VER);
	    ++i;
	}
    }
    END_FOR_EACH_METAPIXEL

    assert(i >= n);
}
