/*
 * main.c
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

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "getopt.h"

#include "vector.h"
#include "zoom.h"
#include "readimage.h"
#include "writeimage.h"
#include "lispreader.h"

#include "cmdline.h"

#define FOR_EACH_METAPIXEL(p)   { unsigned int library_index; \
                                  for (library_index = 0; library_index < num_libraries; ++library_index) { \
                                      metapixel_t *p; \
                                      for (p = libraries[library_index]->metapixels; p != 0; p = p->next)
#define END_FOR_EACH_METAPIXEL  } }


static unsigned int num_metapixels = 0;

static library_t **libraries = 0;
static unsigned int num_libraries = 0;

/* default settings */

static char *default_prepare_directory = 0;
static int default_prepare_width = DEFAULT_PREPARE_WIDTH, default_prepare_height = DEFAULT_PREPARE_HEIGHT;
static string_list_t *default_library_directories = 0;
static int default_small_width = DEFAULT_WIDTH, default_small_height = DEFAULT_HEIGHT;
static float default_weight_factors[NUM_CHANNELS] = { 1.0, 1.0, 1.0 };
static int default_metric = METRIC_SUBPIXEL;
static int default_search = SEARCH_LOCAL;
static int default_classic_min_distance = DEFAULT_CLASSIC_MIN_DISTANCE;
static int default_collage_min_distance = DEFAULT_COLLAGE_MIN_DISTANCE;
static int default_cheat_amount = 0;
static int default_forbid_reconstruction_radius = 0;

/* actual settings */

static int small_width, small_height;
static float weight_factors[NUM_CHANNELS];
static int forbid_reconstruction_radius;

static int benchmark_rendering = 0;

static char*
strip_path (char *name)
{
    char *p = strrchr(name, '/');

    if (p == 0)
        return name;
    return p + 1;
}

static string_list_t*
string_list_prepend_copy (string_list_t *lst, const char *str)
{
    string_list_t *new = (string_list_t*)malloc(sizeof(string_list_t));

    new->str = strdup(str);
    new->next = lst;

    return new;
}

static unsigned int
string_list_length (string_list_t *lst)
{
    unsigned int l = 0;

    while (lst != 0)
    {
	++l;
	lst = lst->next;
    }

    return l;
}

static void
assert_client_data (metapixel_t *pixel)
{
    if (pixel->client_data == 0)
    {
	pixel->client_data = (client_metapixel_data_t*)malloc(sizeof(client_metapixel_data_t));
	assert(pixel->client_data != 0);
	memset(pixel->client_data, 0, sizeof(client_metapixel_data_t));
    }
}

/*
static float
wavelet_compare (coeffs_t *coeffs, metapixel_t *pixel, float best_score)
{
    search_coefficients_t *query = &coeffs->wavelet.coeffs;
    float *query_means = coeffs->wavelet.means;
    float *sums = coeffs->wavelet.sums;
    search_coefficients_t *target = &pixel->coeffs;
    float *target_means = pixel->means;
    float score = 0.0;
    int i;
    int j;
    int channel;

    for (channel = 0; channel < NUM_CHANNELS; ++channel)
	score += index_weights[compute_index(0, channel, 0)]
	    * fabs(query_means[channel] - target_means[channel]) * 0.05;

    j = 0;
    for (i = 0; i < NUM_COEFFS; ++i)
    {
	if (score - sums[i] > best_score)
	    return 1e99;

	while (target->coeffs[j] < query->coeffs[i] && j < NUM_COEFFS)
	    ++j;

	if (j >= NUM_COEFFS)
	    break;

	if (target->coeffs[j] > query->coeffs[i])
	    continue;

	score -= index_weights[weight_ordered_index_to_index[target->coeffs[j]]];
    }

    return score;
}
*/

static float
subpixel_compare (coeffs_t *coeffs, metapixel_t *pixel, float best_score)
{
    int channel;
    float score = 0.0;

    for (channel = 0; channel < NUM_CHANNELS; ++channel)
    {
	int i;

	for (i = 0; i < NUM_SUBPIXELS; ++i)
	{
	    float dist = (int)coeffs->subpixel.subpixels[channel * NUM_SUBPIXELS + i]
		- (int)pixel->subpixels[channel * NUM_SUBPIXELS + i];

	    score += dist * dist * weight_factors[channel];

	    if (score >= best_score)
		return 1e99;
	}
    }

    return score;
}

void
generate_search_coeffs_for_subimage (coeffs_t *coeffs, bitmap_t *bitmap,
				     int x, int y, int width, int height, int metric)
{
    /* FIXME: not reentrant */
    static float *float_image = 0;

    if (metric == METRIC_WAVELET)
    {
	/*
	coefficient_with_index_t raw_coeffs[NUM_COEFFS];
	int i;

	if (float_image == 0)
	    float_image = (float*)malloc(sizeof(float) * IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS);

	if (width != IMAGE_SIZE || height != IMAGE_SIZE)
	{
	    unsigned char *scaled_data;

	    scaled_data = scale_image(image_data, image_width, image_height, x, y, width, height, IMAGE_SIZE, IMAGE_SIZE);
	    assert(scaled_data != 0);

	    for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE * NUM_CHANNELS; ++i)
		float_image[i] = scaled_data[i];

	    free(scaled_data);
	}
	else
	{
	    int j, channel;

	    for (j = 0; j < IMAGE_SIZE; ++j)
		for (i = 0; i < IMAGE_SIZE; ++i)
		    for (channel = 0; channel < NUM_CHANNELS; ++channel)
			float_image[(j * IMAGE_SIZE + i) * NUM_CHANNELS + channel] =
			    image_data[((y + j) * image_width + (x + i)) * NUM_CHANNELS + channel];
	}

	transform_rgb_to_yiq(float_image, IMAGE_SIZE * IMAGE_SIZE);
	decompose_image(float_image);
	find_highest_coefficients(float_image, raw_coeffs);

	generate_search_coeffs(&coeffs->wavelet.coeffs, coeffs->wavelet.sums, raw_coeffs);

	for (i = 0; i < NUM_CHANNELS; ++i)
	    coeffs->wavelet.means[i] = float_image[i];
	*/
    }
    else if (metric == METRIC_SUBPIXEL)
    {
	bitmap_t *sub_bitmap, *scaled_bitmap;
	int i;
	int channel;

	if (float_image == 0)
	    float_image = (float*)malloc(sizeof(float) * NUM_SUBPIXELS * NUM_CHANNELS);
	assert(float_image != 0);

	sub_bitmap = bitmap_sub(bitmap, x, y, width, height);
	assert(sub_bitmap != 0);

	//bitmap_write(sub_bitmap, "/tmp/sub.png");

	scaled_bitmap = bitmap_scale(sub_bitmap, NUM_SUBPIXEL_ROWS_COLS, NUM_SUBPIXEL_ROWS_COLS, FILTER_MITCHELL);
	assert(scaled_bitmap != 0);

	bitmap_free(sub_bitmap);

	//bitmap_write(scaled_bitmap, "/tmp/scaled.png");

	assert(scaled_bitmap->color == COLOR_RGB_8);
	assert(scaled_bitmap->pixel_stride == NUM_CHANNELS);
	assert(scaled_bitmap->row_stride == NUM_SUBPIXEL_ROWS_COLS * NUM_CHANNELS);

	for (i = 0; i < NUM_SUBPIXELS * NUM_CHANNELS; ++i)
	    float_image[i] = scaled_bitmap->data[i];

	bitmap_free(scaled_bitmap);

	transform_rgb_to_yiq(float_image, NUM_SUBPIXELS);

	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    for (i = 0; i < NUM_SUBPIXELS; ++i)
		coeffs->subpixel.subpixels[channel * NUM_SUBPIXELS + i] = float_image[i * NUM_CHANNELS + channel];
    }
    else
	assert(0);
}

static int
metapixel_in_array (metapixel_t *pixel, metapixel_t **array, int size)
{
    int i;

    for (i = 0; i < size; ++i)
	if (array[i] == pixel)
	    return 1;
    return 0;
}

compare_func_t
compare_func_for_metric (int metric)
{
    if (metric == METRIC_WAVELET)
	//return wavelet_compare;
	assert(0);
    else if (metric == METRIC_SUBPIXEL)
	return subpixel_compare;
    else
	assert(0);
    return 0;
}

static int
manhattan_distance (int x1, int y1, int x2, int y2)
{
    return abs(x1 - x2) + abs(y1 - y2);
}

static match_t
metapixel_nearest_to (coeffs_t *coeffs, int metric, int x, int y,
		      metapixel_t **forbidden, int num_forbidden,
		      int (*validity_func) (void*, metapixel_t*, int, int),
		      void *validity_func_data)
{
    float best_score = 1e99;
    metapixel_t *best_fit = 0;
    compare_func_t compare_func = compare_func_for_metric(metric);
    match_t match;

    FOR_EACH_METAPIXEL(pixel)
    {
	float score;

	if (pixel->client_data != 0
	    && (manhattan_distance(x, y, pixel->client_data->anti_x, pixel->client_data->anti_y)
		< forbid_reconstruction_radius))
	    continue;

	score = compare_func(coeffs, pixel, best_score);

	if (score < best_score && !metapixel_in_array(pixel, forbidden, num_forbidden)
	    && (validity_func == 0 || validity_func(validity_func_data, pixel, x, y)))
	{
	    best_score = score;
	    best_fit = pixel;
	}
    }
    END_FOR_EACH_METAPIXEL

    match.pixel = best_fit;
    match.score = best_score;

    return match;
}

static void
get_n_metapixel_nearest_to (int n, global_match_t *matches, coeffs_t *coeffs, int metric)
{
    compare_func_t compare_func = compare_func_for_metric(metric);
    int i;

    assert(num_metapixels >= n);

    i = 0;
    FOR_EACH_METAPIXEL(pixel)
    {
	float score = compare_func(coeffs, pixel, (i < n) ? 1e99 : matches[n - 1].score);

	if (i < n || score < matches[n - 1].score)
	{
	    int j, m;

	    m = MIN(i, n);

	    for (j = 0; j < m; ++j)
		if (matches[j].score > score)
		    break;

	    assert(j <= m && j < n);

	    memmove(matches + j + 1, matches + j, sizeof(global_match_t) * (MIN(n, m + 1) - (j + 1)));

	    matches[j].pixel = pixel;
	    matches[j].score = score;
	}

	++i;
    }
    END_FOR_EACH_METAPIXEL

    assert(i >= n);
}

static void
paste_metapixel (metapixel_t *pixel, bitmap_t *image, int x, int y)
{
    bitmap_t *bitmap;

    bitmap = metapixel_get_bitmap(pixel);
    if (bitmap == 0)
    {
	fprintf(stderr, "Error: cannot read metapixel image `%s'.\n", pixel->filename);
	exit(1);
    }

    if (bitmap->width != small_width || bitmap->height != small_height)
    {
	bitmap_t *scaled_bitmap = bitmap_scale(bitmap, small_width, small_height, FILTER_MITCHELL);

	assert(scaled_bitmap != 0);

	bitmap_free(bitmap);
	bitmap = scaled_bitmap;
    }

    bitmap_paste(image, bitmap, x, y);

    bitmap_free(bitmap);
}

static classic_reader_t*
init_classic_reader (char *input_name, float scale)
{
    classic_reader_t *reader = (classic_reader_t*)malloc(sizeof(classic_reader_t));

    assert(reader != 0);

    reader->image_reader = open_image_reading(input_name);
    if (reader->image_reader == 0)
    {
	fprintf(stderr, "cannot read image `%s'\n", input_name);
	exit(1);
    }

    reader->in_image_width = reader->image_reader->width;
    reader->in_image_height = reader->image_reader->height;

    reader->out_image_width = (((int)((float)reader->in_image_width * scale) - 1) / small_width + 1) * small_width;
    reader->out_image_height = (((int)((float)reader->in_image_height * scale) - 1) / small_height + 1) * small_height;

    assert(reader->out_image_width % small_width == 0);
    assert(reader->out_image_height % small_height == 0);

    reader->metawidth = reader->out_image_width / small_width;
    reader->metaheight = reader->out_image_height / small_height;

    reader->in_image = 0;
    reader->y = 0;

    return reader;
}

static void
read_classic_row (classic_reader_t *reader)
{
    unsigned char *image_data;

    if (reader->in_image != 0)
    {
	assert(reader->y > 0);
	bitmap_free(reader->in_image);
    }
    else
	assert(reader->y == 0);

    reader->num_lines = (reader->y + 1) * reader->in_image_height / reader->metaheight
	- reader->y * reader->in_image_height / reader->metaheight;

    image_data = (unsigned char*)malloc(reader->num_lines * reader->in_image_width * NUM_CHANNELS);
    assert(image_data != 0);

    read_lines(reader->image_reader, image_data, reader->num_lines);

    ++reader->y;

    reader->in_image = bitmap_new_packed(COLOR_RGB_8, reader->in_image_width, reader->num_lines, image_data);
    assert(reader->in_image != 0);
}

static void
compute_classic_column_coords (classic_reader_t *reader, int x, int *left_x, int *width)
{
    *left_x = x * reader->in_image_width / reader->metawidth;
    *width = (x + 1) * reader->in_image_width / reader->metawidth - *left_x;
}

static void
generate_search_coeffs_for_classic_subimage (classic_reader_t *reader, int x, coeffs_t *coeffs, int metric)
{
    int left_x, width;

    compute_classic_column_coords(reader, x, &left_x, &width);
    generate_search_coeffs_for_subimage(coeffs, reader->in_image,
					left_x, 0, width, reader->num_lines, metric);
}

static void
free_classic_reader (classic_reader_t *reader)
{
    if (reader->in_image != 0)
	bitmap_free(reader->in_image);
    free_image_reader(reader->image_reader);
    free(reader);
}

static mosaic_t*
init_mosaic_from_reader (classic_reader_t *reader)
{
    mosaic_t *mosaic = (mosaic_t*)malloc(sizeof(mosaic_t));
    int metawidth = reader->metawidth, metaheight = reader->metaheight;
    int i;

    assert(mosaic != 0);

    mosaic->metawidth = metawidth;
    mosaic->metaheight = metaheight;
    mosaic->matches = (match_t*)malloc(sizeof(match_t) * metawidth * metaheight);

    for (i = 0; i < metawidth * metaheight; ++i)
	mosaic->matches[i].pixel = 0;

    return mosaic;
}

static mosaic_t*
generate_local_classic (classic_reader_t *reader, int min_distance, int metric)
{
    mosaic_t *mosaic = init_mosaic_from_reader(reader);
    int metawidth = reader->metawidth, metaheight = reader->metaheight;
    int x, y;
    metapixel_t **neighborhood = 0;
    int neighborhood_diameter = min_distance * 2 + 1;
    int neighborhood_size = (neighborhood_diameter * neighborhood_diameter - 1) / 2;

    if (min_distance > 0)
	neighborhood = (metapixel_t**)malloc(sizeof(metapixel_t*) * neighborhood_size);

    for (y = 0; y < metaheight; ++y)
    {
	read_classic_row(reader);

	// bitmap_write(reader->in_image, "/tmp/metarow.png");

	for (x = 0; x < metawidth; ++x)
	{
	    match_t match;
	    int i;
	    coeffs_t coeffs;

	    for (i = 0; i < neighborhood_size; ++i)
	    {
		int nx = x + i % neighborhood_diameter - min_distance;
		int ny = y + i / neighborhood_diameter - min_distance;

		if (nx < 0 || nx >= metawidth || ny < 0 || ny >= metaheight)
		    neighborhood[i] = 0;
		else
		    neighborhood[i] = mosaic->matches[ny * metawidth + nx].pixel;
	    }

	    generate_search_coeffs_for_classic_subimage(reader, x, &coeffs, metric);

	    match = metapixel_nearest_to(&coeffs, metric, x, y, neighborhood, neighborhood_size, 0, 0);

	    if (match.pixel == 0)
	    {
		fprintf(stderr, "Error: cannot find a matching image - try using a shorter distance.\n");
		exit(1);
	    }

	    mosaic->matches[y * metawidth + x] = match;

	    printf(".");
	    fflush(stdout);
	}
    }

    free(neighborhood);

    printf("\n");

    return mosaic;
}

static int
compare_global_matches (const void *_m1, const void *_m2)
{
    global_match_t *m1 = (global_match_t*)_m1;
    global_match_t *m2 = (global_match_t*)_m2;

    if (m1->score < m2->score)
	return -1;
    if (m1->score > m2->score)
	return 1;
    return 0;
}

static mosaic_t*
generate_global_classic (classic_reader_t *reader, int metric)
{
    mosaic_t *mosaic = init_mosaic_from_reader(reader);
    int metawidth = reader->metawidth, metaheight = reader->metaheight;
    int x, y;
    global_match_t *matches, *m;
    /* FIXME: this will overflow if metawidth and/or metaheight are large! */
    int num_matches = (metawidth * metaheight) * (metawidth * metaheight);
    int i, ignore_forbidden;
    int num_locations_filled;

    if (num_metapixels < metawidth * metaheight)
    {
	fprintf(stderr,
		"global search method needs at least as much\n"
		"metapixels as there are locations\n");
	exit(1);
    }

    matches = (global_match_t*)malloc(sizeof(global_match_t) * num_matches);
    assert(matches != 0);

    m = matches;
    for (y = 0; y < metaheight; ++y)
    {
	read_classic_row(reader);

	for (x = 0; x < metawidth; ++x)
	{
	    coeffs_t coeffs;

	    generate_search_coeffs_for_classic_subimage(reader, x, &coeffs, metric);
	    
	    get_n_metapixel_nearest_to(metawidth * metaheight, m, &coeffs, metric);
	    for (i = 0; i < metawidth * metaheight; ++i)
	    {
		int j;

		for (j = i + 1; j < metawidth * metaheight; ++j)
		    assert(m[i].pixel != m[j].pixel);

		m[i].x = x;
		m[i].y = y;
	    }

	    m += metawidth * metaheight;

	    printf(".");
	    fflush(stdout);
	}
    }

    qsort(matches, num_matches, sizeof(global_match_t), compare_global_matches);

    FOR_EACH_METAPIXEL(pixel)
	assert_client_data(pixel);
	pixel->client_data->flag = 0;
    END_FOR_EACH_METAPIXEL

    num_locations_filled = 0;
    for (ignore_forbidden = 0; ignore_forbidden < 2; ++ignore_forbidden)
    {
	for (i = 0; i < num_matches; ++i)
	{
	    int index = matches[i].y * metawidth + matches[i].x;

	    if (!ignore_forbidden && matches[i].pixel->client_data != 0
		&& (manhattan_distance(matches[i].x, matches[i].y,
				       matches[i].pixel->client_data->anti_x,
				       matches[i].pixel->client_data->anti_y)
		    < forbid_reconstruction_radius))
		continue;

	    if (matches[i].pixel->client_data->flag)
		continue;

	    if (num_locations_filled >= metawidth * metaheight)
		break;

	    if (mosaic->matches[index].pixel == 0)
	    {
		if (forbid_reconstruction_radius > 0 && ignore_forbidden)
		{
		    printf("!");
		    fflush(stdout);
		}
		mosaic->matches[index].pixel = matches[i].pixel;
		mosaic->matches[index].score = matches[i].score;

		matches[i].pixel->client_data->flag = 1;

		++num_locations_filled;
	    }
	}
	if (forbid_reconstruction_radius == 0)
	    break;
    }
    assert(num_locations_filled == metawidth * metaheight);

    free(matches);

    printf("\n");

    return mosaic;
}

static void
print_current_time (void)
{
    struct timeval tv;

    gettimeofday(&tv, 0);

    printf("time: %lu %lu\n", (unsigned long)tv.tv_sec, (unsigned long)tv.tv_usec);
}

static void
paste_classic (mosaic_t *mosaic, char *input_name, char *output_name, int cheat)
{
    image_reader_t *reader;
    image_writer_t *writer = 0;
    int out_image_width, out_image_height;
    int x, y;
    unsigned char *out_image_data;
    int in_image_width, in_image_height;
    int metawidth = mosaic->metawidth, metaheight = mosaic->metaheight;
    bitmap_t *out_bitmap;

    if (cheat > 0)
    {
	reader = open_image_reading(input_name);
	if (reader == 0)
	{
	    fprintf(stderr, "cannot read image `%s'\n", input_name);
	    exit(1);
	}

	in_image_width = reader->width;
	in_image_height = reader->height;
    }
    else
    {
	reader = 0;
	in_image_width = in_image_height = 0;
    }

    out_image_width = mosaic->metawidth * small_width;
    out_image_height = mosaic->metaheight * small_height;

    if (!benchmark_rendering)
    {
	writer = open_image_writing(output_name, out_image_width, out_image_height,
				    out_image_width * 3, IMAGE_FORMAT_PNG);
	if (writer == 0)
	{
	    fprintf(stderr, "cannot write image `%s'\n", output_name);
	    exit(1);
	}
    }

    out_bitmap = bitmap_new_empty(COLOR_RGB_8, out_image_width, small_height);
    assert(out_bitmap != 0);

    out_image_data = out_bitmap->data;

    if (benchmark_rendering)
	print_current_time();

    for (y = 0; y < metaheight; ++y)
    {
	for (x = 0; x < metawidth; ++x)
	{
	    if (benchmark_rendering)
		assert(mosaic->matches[y * metawidth + x].pixel->bitmap != 0);

	    paste_metapixel(mosaic->matches[y * metawidth + x].pixel, out_bitmap, x * small_width, 0);
	    if (!benchmark_rendering)
	    {
		printf("X");
		fflush(stdout);
	    }
	}

	if (cheat > 0)
	{
	    int num_lines = (y + 1) * in_image_height / metaheight - y * in_image_height / metaheight;
	    unsigned char *in_image_data = (unsigned char*)malloc(num_lines * in_image_width * NUM_CHANNELS);
	    bitmap_t *in_bitmap;
	    bitmap_t *source_bitmap;

	    assert(in_image_data != 0);

	    read_lines(reader, in_image_data, num_lines);

	    in_bitmap = bitmap_new_packed(COLOR_RGB_8, in_image_width, num_lines, in_image_data);
	    assert(in_bitmap != 0);

	    if (in_image_width != out_image_width || num_lines != small_height)
		source_bitmap = bitmap_scale(in_bitmap, out_image_width, small_height, FILTER_MITCHELL);
	    else
		source_bitmap = in_bitmap;

	    bitmap_alpha_compose(out_bitmap, source_bitmap, cheat * 0x10000 / 100);

	    if (source_bitmap != in_bitmap)
		bitmap_free(source_bitmap);

	    bitmap_free(in_bitmap);
	}

	if (!benchmark_rendering)
	    write_lines(writer, out_image_data, small_height);
    }

    if (benchmark_rendering)
	print_current_time();

    bitmap_free(out_bitmap);

    if (!benchmark_rendering)
	free_image_writer(writer);
    if (cheat > 0)
	free_image_reader(reader);

    printf("\n");
}

static void
pixel_add_collage_position (metapixel_t *pixel, int x, int y)
{
    position_t *position = (position_t*)malloc(sizeof(position_t));

    assert(position != 0);

    position->x = x;
    position->y = y;

    assert_client_data(pixel);
    position->next = pixel->client_data->collage_positions;
    pixel->client_data->collage_positions = position;
}

static int
pixel_valid_for_collage_position (void *data, metapixel_t *pixel, int x, int y)
{
    int min_distance = (int)data;
    position_t *position;

    if (min_distance <= 0)
	return 1;

    if (pixel->client_data != 0)
	for (position = pixel->client_data->collage_positions; position != 0; position = position->next)
	    if (manhattan_distance(x, y, position->x, position->y) < min_distance)
		return 0;

    return 1;
}

static void
generate_collage (char *input_name, char *output_name, float scale, int min_distance, int metric, int cheat)
{
    bitmap_t *in_bitmap, *out_bitmap;
    char *bitmap;
    int num_pixels_done = 0;

    in_bitmap = bitmap_read(input_name);
    if (in_bitmap == 0)
    {
	fprintf(stderr, "Error: Could not read image `%s'.\n", input_name);
	exit(1);
    }

    if (scale != 1.0)
    {
	int new_width = (float)in_bitmap->width * scale;
	int new_height = (float)in_bitmap->height * scale;
	bitmap_t *scaled_bitmap;

	if (new_width < small_width || new_height < small_height)
	{
	    fprintf(stderr, "Error: Source image or scaling factor too small.\n");
	    exit(1);
	}

	printf("Scaling source image to %dx%d\n", new_width, new_height);

	scaled_bitmap = bitmap_scale(in_bitmap, new_width, new_height, FILTER_MITCHELL);
	assert(scaled_bitmap != 0);

	bitmap_free(in_bitmap);
	in_bitmap = scaled_bitmap;
    }

    out_bitmap = bitmap_new_empty(COLOR_RGB_8, in_bitmap->width, in_bitmap->height);
    assert(out_bitmap != 0);

    bitmap = (char*)malloc(in_bitmap->width * in_bitmap->height);
    memset(bitmap, 0, in_bitmap->width * in_bitmap->height);

    while (num_pixels_done < in_bitmap->width * in_bitmap->height)
    {
	int i, j;
	int x, y;
	coeffs_t coeffs;
	match_t match;

	while (1)
	{
	    x = random() % in_bitmap->width - small_width / 2;
	    y = random() % in_bitmap->height - small_height / 2;

	    if (x < 0)
		x = 0;
	    if (x + small_width > in_bitmap->width)
		x = in_bitmap->width - small_width;

	    if (y < 0)
		y = 0;
	    if (y + small_height > in_bitmap->height)
		y = in_bitmap->height - small_height;

	    for (j = 0; j < small_height; ++j)
		for (i = 0; i < small_width; ++i)
		    if (!bitmap[(y + j) * in_bitmap->width + x + i])
			goto out;
	}

    out:
	generate_search_coeffs_for_subimage(&coeffs, in_bitmap, x, y, small_width, small_height, metric);

	match = metapixel_nearest_to(&coeffs, metric, x, y, 0, 0,
				     pixel_valid_for_collage_position, (void*)min_distance);
	paste_metapixel(match.pixel, out_bitmap, x, y);

	if (min_distance > 0)
	    pixel_add_collage_position(match.pixel, x, y);

	for (j = 0; j < small_height; ++j)
	    for (i = 0; i < small_width; ++i)
		if (!bitmap[(y + j) * in_bitmap->width + x + i])
		{
		    bitmap[(y + j) * in_bitmap->width + x + i] = 1;
		    ++num_pixels_done;
		}

	printf(".");
	fflush(stdout);
    }

    bitmap_write(out_bitmap, output_name);

    free(bitmap);
    bitmap_free(out_bitmap);
    bitmap_free(in_bitmap);
}

static metapixel_t*
find_metapixel (char *filename)
{
    FOR_EACH_METAPIXEL(pixel)
	if (strcmp(pixel->filename, filename) == 0)
	    return pixel;
    END_FOR_EACH_METAPIXEL

    return 0;
}

static mosaic_t*
read_protocol (FILE *in)
{
    lisp_object_t *obj;
    lisp_stream_t stream;
    mosaic_t *mosaic = (mosaic_t*)malloc(sizeof(mosaic_t));
    int type;

    lisp_stream_init_file(&stream, in);

    obj = lisp_read(&stream);
    type = lisp_type(obj);
    if (type != LISP_TYPE_EOF && type != LISP_TYPE_PARSE_ERROR)
    {
	lisp_object_t *vars[3];

	if (lisp_match_string("(mosaic (size #?(integer) #?(integer)) (metapixels . #?(list)))",
			      obj, vars))
	{
	    int i;
	    int num_pixels;
	    lisp_object_t *lst;

	    mosaic->metawidth = lisp_integer(vars[0]);
	    mosaic->metaheight = lisp_integer(vars[1]);
	    num_pixels = mosaic->metawidth * mosaic->metaheight;
	    mosaic->matches = (match_t*)malloc(sizeof(match_t) * num_pixels);

	    for (i = 0; i < num_pixels; ++i)
		mosaic->matches[i].pixel = 0;

	    if (lisp_list_length(vars[2]) != num_pixels)
	    {
		fprintf(stderr, "mosaic should have %d metapixels, not %d\n", num_pixels, lisp_list_length(vars[2]));
		exit(1);
	    }

	    lst = vars[2];
	    for (i = 0; i < num_pixels; ++i)
	    {
		lisp_object_t *vars[5];

		if (lisp_match_string("(#?(integer) #?(integer) #?(integer) #?(integer) #?(string))",
				      lisp_car(lst), vars))
		{
		    int x = lisp_integer(vars[0]);
		    int y = lisp_integer(vars[1]);
		    int width = lisp_integer(vars[2]);
		    int height = lisp_integer(vars[3]);
		    metapixel_t *pixel;

		    if (width != 1 || height != 1)
		    {
			fprintf(stderr, "width and height in metapixel must both be 1\n");
			exit(1);
		    }

		    if (mosaic->matches[y * mosaic->metawidth + x].pixel != 0)
		    {
			fprintf(stderr, "location (%d,%d) is assigned to twice\n", x, y);
			exit(1);
		    }

		    pixel = find_metapixel(lisp_string(vars[4]));

		    if (pixel == 0)
		    {
			fprintf(stderr, "could not find metapixel `%s'\n", lisp_string(vars[4]));
			exit(1);
		    }

		    mosaic->matches[y * mosaic->metawidth + x].pixel = pixel;
		}
		else
		{
		    fprintf(stderr, "metapixel ");
		    lisp_dump(lisp_car(lst), stderr);
		    fprintf(stderr, " has wrong format\n");
		    exit(1);
		}

		lst = lisp_cdr(lst);
	    }
	}
	else
	{
	    fprintf(stderr, "malformed expression in protocol file\n");
	    exit(1);
	}
    }
    else
    {
	fprintf(stderr, "error in protocol file\n");
	exit(1);
    }
    lisp_free(obj);

    return mosaic;
}

void
free_mosaic (mosaic_t *mosaic)
{
    free(mosaic->matches);
    free(mosaic);
}

int
make_classic_mosaic (char *in_image_name, char *out_image_name,
		     int metric, float scale, int search, int min_distance, int cheat,
		     char *in_protocol_name, char *out_protocol_name)
{
    mosaic_t *mosaic;
    int x, y;

    if (in_protocol_name != 0)
    {
	FILE *protocol_in = fopen(in_protocol_name, "r");

	if (protocol_in == 0)
	{
	    fprintf(stderr, "cannot open protocol file for reading `%s': %s\n", in_protocol_name, strerror(errno));
	    return 0;
	}

	mosaic = read_protocol(protocol_in);

	fclose(protocol_in);
    }
    else
    {
	classic_reader_t *reader = init_classic_reader(in_image_name, scale);

	if (search == SEARCH_LOCAL)
	    mosaic = generate_local_classic(reader, min_distance, metric);
	else if (search == SEARCH_GLOBAL)
	    mosaic = generate_global_classic(reader, metric);
	else
	    assert(0);

	free_classic_reader(reader);
    }

    assert(mosaic != 0);

    if (benchmark_rendering)
    {
	int i;

	for (i = 0; i < mosaic->metawidth * mosaic->metaheight; ++i)
	{
	    metapixel_t *pixel = mosaic->matches[i].pixel;

	    if (pixel->bitmap == 0)
	    {
		pixel->bitmap = metapixel_get_bitmap(pixel);
		assert(pixel->bitmap != 0);
	    }
	}
    }

    if (out_protocol_name != 0)
    {
	FILE *protocol_out = fopen(out_protocol_name, "w");

	if (protocol_out == 0)
	{
	    fprintf(stderr, "cannot open protocol file `%s' for writing: %s\n", out_protocol_name, strerror(errno));
	    /* FIXME: free stuff */
	    return 0;
	}
	else
	{
	    fprintf(protocol_out, "(mosaic (size %d %d)\n(metapixels\n", mosaic->metawidth, mosaic->metaheight);
	    for (y = 0; y < mosaic->metaheight; ++y)
	    {
		for (x = 0; x < mosaic->metawidth; ++x)
		{
		    match_t *match = &mosaic->matches[y * mosaic->metawidth + x];
		    lisp_object_t *obj = lisp_make_string(match->pixel->filename);

		    fprintf(protocol_out, "(%d %d 1 1 ", x, y);
		    lisp_dump(obj, protocol_out);
		    fprintf(protocol_out, ") ; %f\n", match->score);

		    lisp_free(obj);
		}
	    }
	    fprintf(protocol_out, "))\n");
	}

	fclose(protocol_out);
    }

    paste_classic(mosaic, in_image_name, out_image_name, cheat);

    free_mosaic(mosaic);

    return 1;
}

#define RC_FILE_NAME      ".metapixelrc"

static void
read_rc_file (void)
{
    lisp_stream_t stream;
    char *homedir;
    char *filename;

    homedir = getenv("HOME");

    if (homedir == 0)
    {
	fprintf(stderr, "Warning: HOME is not in environment - cannot find rc file.\n");
	return;
    }

    filename = (char*)malloc(strlen(homedir) + 1 + strlen(RC_FILE_NAME) + 1);
    strcpy(filename, homedir);
    strcat(filename, "/");
    strcat(filename, RC_FILE_NAME);

    if (access(filename, R_OK) == 0)
    {
	if (lisp_stream_init_path(&stream, filename) == 0)
	    fprintf(stderr, "Warning: could not open rc file `%s'.", filename);
	else
	{
	    for (;;)
	    {
		lisp_object_t *obj;
		int type;

		obj = lisp_read(&stream);
		type = lisp_type(obj);

		if (type != LISP_TYPE_EOF && type != LISP_TYPE_PARSE_ERROR)
		{
		    lisp_object_t *vars[3];

		    if (lisp_match_string("(prepare-directory #?(string))", obj, vars))
			default_prepare_directory = strdup(lisp_string(vars[0]));
		    else if (lisp_match_string("(prepare-dimensions #?(integer) #?(integer))", obj, vars))
		    {
			default_prepare_width = lisp_integer(vars[0]);
			default_prepare_height = lisp_integer(vars[1]);
		    }
		    else if (lisp_match_string("(library-directory #?(string))", obj, vars))
			default_library_directories = string_list_prepend_copy(default_library_directories,
									       lisp_string(vars[0]));
		    else if (lisp_match_string("(small-image-dimensions #?(integer) #?(integer))", obj, vars))
		    {
			default_small_width = lisp_integer(vars[0]);
			default_small_height = lisp_integer(vars[1]);
		    }
		    else if (lisp_match_string("(yiq-weights #?(number) #?(number) #?(number))", obj, vars))
		    {
			int i;

			for (i = 0; i < 3; ++i)
			    default_weight_factors[i] = lisp_real(vars[i]);
		    }
		    else if (lisp_match_string("(metric #?(or wavelet subpixel))", obj, vars))
		    {
			if (strcmp(lisp_symbol(vars[0]), "wavelet") == 0)
			    default_metric = METRIC_WAVELET;
			else
			    default_metric = METRIC_SUBPIXEL;
		    }
		    else if (lisp_match_string("(search-method #?(or local global))", obj, vars))
		    {
			if (strcmp(lisp_symbol(vars[0]), "local") == 0)
			    default_search = SEARCH_LOCAL;
			else
			    default_search = SEARCH_GLOBAL;
		    }
		    else if (lisp_match_string("(minimum-classic-distance #?(integer))", obj, vars))
			default_classic_min_distance = lisp_integer(vars[0]);
		    else if (lisp_match_string("(minimum-collage-distance #?(integer))", obj, vars))
			default_collage_min_distance = lisp_integer(vars[0]);
		    else if (lisp_match_string("(cheat-amount #?(integer))", obj, vars))
			default_cheat_amount = lisp_integer(vars[0]);
		    else if (lisp_match_string("(forbid-reconstruction-distance #?(integer))", obj, vars))
			default_forbid_reconstruction_radius = lisp_integer(vars[0]);
		    else
		    {
			fprintf(stderr, "Warning: unknown rc file option ");
			lisp_dump(obj, stderr);
			fprintf(stderr, "\n");
		    }
		}
		else if (type == LISP_TYPE_PARSE_ERROR)
		    fprintf(stderr, "Error: parse error in rc file.\n");

		lisp_free(obj);

		if (type == LISP_TYPE_EOF || type == LISP_TYPE_PARSE_ERROR)
		    break;
	    }

	    lisp_stream_free_path(&stream);
	}
    }

    free(filename);
}

static void
usage (void)
{
    printf("Usage:\n"
	   "  metapixel --version\n"
	   "      print out version number\n"
	   "  metapixel --help\n"
	   "      print this help text\n"
	   "  metapixel [option ...] --prepare <library-dir> <image>\n"
	   "      add <image> to the library in <library-dir>\n"
	   "  metapixel [option ...] --metapixel <in> <out>\n"
	   "      transform <in> to <out>\n"
	   "  metapixel [option ...] --batch <batchfile>\n"
	   "      perform all the tasks in <batchfile>\n"
	   "Options:\n"
	   "  -l, --library=DIR            add the library in DIR\n"
	   "  -x, --antimosaic=PIC         use PIC as an antimosaic\n"
	   "  -w, --width=WIDTH            set width for small images\n"
	   "  -h, --height=HEIGHT          set height for small images\n"
	   "  -y, --y-weight=WEIGHT        assign relative weight for the Y-channel\n"
	   "  -i, --i-weight=WEIGHT        assign relative weight for the I-channel\n"
	   "  -q, --q-weight=WEIGHT        assign relative weight for the Q-channel\n"
	   "  -s  --scale=SCALE            scale input image by specified factor\n"
	   "  -m, --metric=METRIC          choose metric (subpixel or wavelet)\n"
	   "  -e, --search=SEARCH          choose search method (local or global)\n"
	   "  -c, --collage                collage mode\n"
	   "  -d, --distance=DIST          minimum distance between two instances of\n"
	   "                               the same constituent image\n"
	   "  -a, --cheat=PERC             cheat with specified percentage\n"
	   "  -f, --forbid-reconstruction=DIST\n"
	   "                               forbid placing antimosaic images on their\n"
	   "                               original locations or locations around it\n"
	   "  --out=FILE                   write protocol to file\n"
	   "  --in=FILE                    read protocol from file and use it\n"
	   "\n"
	   "Report bugs and suggestions to schani@complang.tuwien.ac.at\n");
}

#define OPT_VERSION                    256
#define OPT_HELP                       257
#define OPT_PREPARE                    258
#define OPT_METAPIXEL                  259
#define OPT_BATCH                      260
#define OPT_OUT                        261
#define OPT_IN                         262
#define OPT_BENCHMARK_RENDERING        263
#define OPT_PRINT_PREPARE_SETTINGS     264

#define MODE_NONE       0
#define MODE_PREPARE    1
#define MODE_METAPIXEL  2
#define MODE_BATCH      3

int
main (int argc, char *argv[])
{
    int mode = MODE_NONE;
    int metric;
    int search;
    int collage = 0;
    int classic_min_distance, collage_min_distance;
    int cheat = 0;
    float scale = 1.0;
    char *out_filename = 0;
    char *in_filename = 0;
    char *antimosaic_filename = 0;
    string_list_t *library_directories = 0;
    int prepare_width = 0, prepare_height = 0;

    read_rc_file();

    small_width = default_small_width;
    small_height = default_small_height;
    prepare_width = default_prepare_width;
    prepare_height = default_prepare_height;
    memcpy(weight_factors, default_weight_factors, sizeof(weight_factors));
    metric = default_metric;
    search = default_search;
    classic_min_distance = default_classic_min_distance;
    collage_min_distance = default_collage_min_distance;
    cheat = default_cheat_amount;
    forbid_reconstruction_radius = default_forbid_reconstruction_radius + 1;

    while (1)
    {
	static struct option long_options[] =
            {
		{ "version", no_argument, 0, OPT_VERSION },
		{ "help", no_argument, 0, OPT_HELP },
		{ "prepare", no_argument, 0, OPT_PREPARE },
		{ "metapixel", no_argument, 0, OPT_METAPIXEL },
		{ "batch", no_argument, 0, OPT_BATCH },
		{ "out", required_argument, 0, OPT_OUT },
		{ "in", required_argument, 0, OPT_IN },
		{ "benchmark-rendering", no_argument, 0, OPT_BENCHMARK_RENDERING },
		{ "print-prepare-settings", no_argument, 0, OPT_PRINT_PREPARE_SETTINGS },
		{ "library", required_argument, 0, 'l' },
		{ "width", required_argument, 0, 'w' },
		{ "height", required_argument, 0, 'h' },
		{ "y-weight", required_argument, 0, 'y' },
		{ "i-weight", required_argument, 0, 'i' },
		{ "q-weight", required_argument, 0, 'q' },
		{ "scale", required_argument, 0, 's' },
		{ "collage", no_argument, 0, 'c' },
		{ "distance", required_argument, 0, 'd' },
		{ "cheat", required_argument, 0, 'a' },
		{ "metric", required_argument, 0, 'm' },
		{ "search", required_argument, 0, 'e' },
		{ "antimosaic", required_argument, 0, 'x' },
		{ "forbid-reconstruction", required_argument, 0, 'f' },
		{ 0, 0, 0, 0 }
	    };

	int option, option_index;

	option = getopt_long(argc, argv, "l:m:e:w:h:y:i:q:s:cd:a:x:f:", long_options, &option_index);

	if (option == -1)
	    break;

	switch (option)
	{
	    case OPT_PREPARE :
		mode = MODE_PREPARE;
		break;

	    case OPT_METAPIXEL :
		mode = MODE_METAPIXEL;
		break;

	    case OPT_BATCH :
		mode = MODE_BATCH;
		break;

	    case OPT_OUT :
		if (out_filename != 0)
		{
		    fprintf(stderr, "the --out option can be used at most once\n");
		    return 1;
		}
		out_filename = strdup(optarg);
		assert(out_filename != 0);
		break;

	    case OPT_IN :
		if (in_filename != 0)
		{
		    fprintf(stderr, "the --in option can be used at most once\n");
		    return 1;
		}
		in_filename = strdup(optarg);
		assert(in_filename != 0);
		break;

	    case OPT_BENCHMARK_RENDERING :
		benchmark_rendering = 1;
		break;

	    case OPT_PRINT_PREPARE_SETTINGS :
		printf("%d %d %s\n", default_prepare_width, default_prepare_height,
		       default_prepare_directory != 0 ? default_prepare_directory : "");
		exit(0);
		break;

	    case 'm' :
		if (strcmp(optarg, "wavelet") == 0)
		    metric = METRIC_WAVELET;
		else if (strcmp(optarg, "subpixel") == 0)
		    metric = METRIC_SUBPIXEL;
		else
		{
		    fprintf(stderr, "metric must either be subpixel or wavelet\n");
		    return 1;
		}
		break;

	    case 'e' :
		if (strcmp(optarg, "local") == 0)
		    search = SEARCH_LOCAL;
		else if (strcmp(optarg, "global") == 0)
		    search = SEARCH_GLOBAL;
		else
		{
		    fprintf(stderr, "search method must either be local or global\n");
		    return 1;
		}
		break;

	    case 'w' :
		small_width = prepare_width = atoi(optarg);
		break;

	    case 'h' :
		small_height = prepare_height = atoi(optarg);
		break;

	    case 'y' :
		weight_factors[0] = atof(optarg);
		break;

	    case 'i' :
		weight_factors[1] = atof(optarg);
		break;

	    case 'q' :
		weight_factors[2] = atof(optarg);
		break;

	    case 's':
		scale = atof(optarg);
		break;

	    case 'c' :
		collage = 1;
		break;

	    case 'd' :
		classic_min_distance = collage_min_distance = atoi(optarg);
		break;

	    case 'a' :
		cheat = atoi(optarg);
		break;

	    case 'l' :
		if (antimosaic_filename != 0)
		{
		    fprintf(stderr, "Error: --tables and --antimosaic cannot be used together.\n");
		    return 1;
		}

		library_directories = string_list_prepend_copy(library_directories, optarg);
		break;

	    case 'x' :
		if (antimosaic_filename != 0)
		{
		    fprintf(stderr, "Error: at most one antimosaic picture can be specified.\n");
		    return 1;
		}
		else if (library_directories != 0)
		{
		    fprintf(stderr, "Error: --library and --antimosaic cannot be used together.\n");
		    return 1;
		}
		antimosaic_filename = strdup(optarg);
		break;

	    case 'f' :
		forbid_reconstruction_radius = atoi(optarg) + 1;
		break;

	    case OPT_VERSION :
		printf("metapixel " METAPIXEL_VERSION "\n"
		       "\n"
		       "Copyright (C) 1997-2004 Mark Probst\n"
		       "\n"
		       "This program is free software; you can redistribute it and/or modify\n"
		       "it under the terms of the GNU General Public License as published by\n"
		       "the Free Software Foundation; either version 2 of the License, or\n"
		       "(at your option) any later version.\n"
		       "\n"
		       "This program is distributed in the hope that it will be useful,\n"
		       "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		       "GNU General Public License for more details.\n"
		       "\n"
		       "You should have received a copy of the GNU General Public License\n"
		       "along with this program; if not, write to the Free Software\n"
		       "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.\n");
		exit(0);

	    case OPT_HELP :
		usage();
		return 0;

	    default :
		assert(0);
	}
    }

    /* check settings for soundness */
    if (small_height <= 0)
    {
	fprintf(stderr, "Error: height of small images must be positive.\n");
	return 1;
    }
    if (small_width <= 0)
    {
	fprintf(stderr, "Error: width of small images must be positive.\n");
	return 1;
    }
    if (scale <= 0.0)
    {
	fprintf(stderr, "Error: scale factor must be positive.\n");
	return 1;
    }
    if (classic_min_distance < 0)
    {
	fprintf(stderr, "Error: classic minimum distance must be non-negative.\n");
	return 1;
    }
    if (collage_min_distance < 0)
    {
	fprintf(stderr, "Error: collage minimum distance must be non-negative.\n");
	return 1;
    }
    if (cheat < 0 || cheat > 100)
    {
	fprintf(stderr, "Error: cheat amount must be in the range from 0 to 100.\n");
	return 1;
    }
    /* the value in forbid_reconstruction_radius is always one more
       than the user specified */
    if (forbid_reconstruction_radius <= 0)
    {
	fprintf(stderr, "Error: forbid reconstruction distance must be non-negative.\n");
	return 1;
    }

    if (in_filename != 0 || out_filename != 0)
    {
	if (mode != MODE_METAPIXEL)
	{
	    fprintf(stderr, "the --in and --out options can only be used in metapixel mode\n");
	    return 1;
	}
	if (collage)
	{
	    fprintf(stderr, "the --in and --out options can only be used for classic mosaics\n");
	    return 1;
	}
    }

    if (mode == MODE_PREPARE)
    {
	char *inimage_name, *library_name;
	metapixel_t *pixel;
	bitmap_t *bitmap;
	library_t *library;

	if (argc - optind != 2)
	{
	    usage();
	    return 1;
	}

	assert(prepare_width > 0 && prepare_height > 0);

	library_name = argv[optind + 0];
	inimage_name = argv[optind + 1];

	library = library_open_without_reading(library_name);
	if (library == 0)
	{
	    fprintf(stderr, "Error: could not open library `%s'.\n", library_name);
	    return 1;
	}

	bitmap = bitmap_read(inimage_name);
	if (bitmap == 0)
	{
	    fprintf(stderr, "Error: could not read image `%s'.\n", inimage_name);
	    return 1;
	}

	pixel = metapixel_new_from_bitmap(bitmap, strip_path(inimage_name), prepare_width, prepare_height);
	assert(pixel != 0);

	bitmap_free(bitmap);

	if (!library_add_metapixel(library, pixel))
	{
	    fprintf(stderr, "Error: could not add metapixel to library.\n");
	    return 1;
	}

	metapixel_free(pixel);

	library_close(library);
    }
    else if (mode == MODE_METAPIXEL
	     || mode == MODE_BATCH)
    {
	if ((mode == MODE_METAPIXEL && argc - optind != 2)
	    || (mode == MODE_BATCH && argc - optind != 1))
	{
	    usage();
	    return 1;
	}

	if (antimosaic_filename == 0 && library_directories == 0)
	    library_directories = default_library_directories;

	if (antimosaic_filename != 0)
	{
	    /*
	    classic_reader_t *reader = init_classic_reader(antimosaic_filename, scale);
	    int x, y;

	    for (y = 0; y < reader->metaheight; ++y)
	    {
		read_classic_row(reader);

		for (x = 0; x < reader->metawidth; ++x)
		{
		    static coefficient_with_index_t highest_coeffs[NUM_COEFFS];

		    unsigned char *scaled_data;
		    metapixel_t *pixel = new_metapixel();
		    int left_x, width;

		    compute_classic_column_coords(reader, x, &left_x, &width);
		    scaled_data = scale_image(reader->in_image_data, reader->in_image_width, reader->num_lines,
					      left_x, 0, width, reader->num_lines, small_width, small_height);
		    assert(scaled_data != 0);

		    generate_metapixel_coefficients(pixel, scaled_data, highest_coeffs);

		    pixel->data = scaled_data;
		    pixel->width = small_width;
		    pixel->height = small_height;
		    pixel->anti_x = x;
		    pixel->anti_y = y;
		    pixel->collage_positions = 0;

		    pixel->filename = (char*)malloc(64);
		    sprintf(pixel->filename, "(%d,%d)", x, y);

		    add_metapixel(pixel);

		    printf(":");
		    fflush(stdout);
		}
	    }

	    printf("\n");

	    free_classic_reader(reader);
	    */
	    assert(0);
	}
	else if (library_directories != 0)
	{
	    string_list_t *lst;
	    unsigned int i;

	    num_libraries = string_list_length(library_directories);
	    libraries = (library_t**)malloc(num_libraries * sizeof(library_t*));

	    for (i = 0, lst = library_directories; lst != 0; ++i, lst = lst->next)
	    {
		libraries[i] = library_open(lst->str);

		if (libraries[i] == 0)
		{
		    fprintf(stderr, "Error: cannot open library `%s'.\n", lst->str);
		    return 1;
		}

		num_metapixels += libraries[i]->num_metapixels;
	    }

	    forbid_reconstruction_radius = 0;
	}
	else
	{
	    fprintf(stderr, "Error: you must give one of the option --tables and --antimosaic.\n");
	    return 1;
	}

	if (mode == MODE_METAPIXEL)
	{
	    if (collage)
		generate_collage(argv[optind], argv[optind + 1], scale, collage_min_distance,
				 metric, cheat);
	    else
		make_classic_mosaic(argv[optind], argv[optind + 1],
				    metric, scale, search, classic_min_distance, cheat,
				    in_filename, out_filename);
	}
	else if (mode == MODE_BATCH)
	{
	    FILE *in = fopen(argv[optind], "r");
	    lisp_stream_t stream;

	    if (in == 0)
	    {
		fprintf(stderr, "cannot open batch file `%s': %s\n", argv[optind], strerror(errno));
		return 1;
	    }

	    lisp_stream_init_file(&stream, in);

	    for (;;)
	    {
		lisp_object_t *obj = lisp_read(&stream);
		int type = lisp_type(obj);

		if (type != LISP_TYPE_EOF && type != LISP_TYPE_PARSE_ERROR)
		{
		    lisp_object_t *vars[4];

		    if (lisp_match_string("(classic (#?(or image protocol) #?(string)) #?(string) . #?(list))",
					  obj, vars))
		    {
			float this_scale = scale;
			int this_search = search;
			int this_min_distance = classic_min_distance;
			int this_cheat = cheat;
			int this_metric = metric;
			char *this_prot_in_filename = in_filename;
			char *this_prot_out_filename = out_filename;
			char *this_image_out_filename = lisp_string(vars[2]);
			char *this_image_in_filename = 0;
			lisp_object_t *lst;
			lisp_object_t *var;

			if (strcmp(lisp_symbol(vars[0]), "image") == 0)
			    this_image_in_filename = lisp_string(vars[1]);
			else
			    this_prot_in_filename = lisp_string(vars[1]);

			for (lst = vars[3]; lisp_type(lst) != LISP_TYPE_NIL; lst = lisp_cdr(lst))
			{
			    if (lisp_match_string("(scale #?(real))",
						  lisp_car(lst), &var))
			    {
				float val = lisp_real(var);

				if (val <= 0.0)
				    fprintf(stderr, "scale must be larger than 0\n");
				else
				    this_scale = val;
			    }
			    else if (lisp_match_string("(search #?(or local global))",
						       lisp_car(lst), &var))
			    {
				if (strcmp(lisp_symbol(var), "local") == 0)
				    this_search = SEARCH_LOCAL;
				else
				    this_search = SEARCH_GLOBAL;
			    }
			    else if (lisp_match_string("(min-distance #?(integer))",
						       lisp_car(lst), &var))
			    {
				int val = lisp_integer(var);

				if (val < 0)
				    fprintf(stderr, "min-distance cannot be negative\n");
				else
				    this_min_distance = val;
			    }
			    else if (lisp_match_string("(cheat #?(integer))",
						       lisp_car(lst), &var))
			    {
				int val = lisp_integer(var);

				if (val < 0 || val > 100)
				    fprintf(stderr, "cheat must be between 0 and 100, inclusively\n");
				else
				    this_cheat = val;
				    
			    }
			    else if (lisp_match_string("(metric #?(or subpixel wavelet))",
						       lisp_car(lst), &var))
			    {
				if (strcmp(lisp_symbol(var), "subpixel") == 0)
				    this_metric = METRIC_SUBPIXEL;
				else
				    this_metric = METRIC_WAVELET;
			    }
			    else if (lisp_match_string("(protocol #?(string))",
						       lisp_car(lst), &var))
				this_prot_out_filename = lisp_string(var);
			    else
			    {
				fprintf(stderr, "unknown expression ");
				lisp_dump(lisp_car(lst), stderr);
				fprintf(stderr, "\n");
			    }
			}

			make_classic_mosaic(this_image_in_filename, this_image_out_filename,
					    this_metric, this_scale, this_search, this_min_distance, this_cheat,
					    this_prot_in_filename, this_prot_out_filename);
		    }
		    else
		    {
			fprintf(stderr, "unknown expression ");
			lisp_dump(obj, stderr);
			fprintf(stderr, "\n");
		    }
		}
		else if (type == LISP_TYPE_PARSE_ERROR)
		    fprintf(stderr, "parse error in batch file\n");
		lisp_free(obj);

		if (type == LISP_TYPE_EOF)
		    break;
	    }
	}
	else
	    assert(0);
    }
    else
    {
	usage();
	return 1;
    }

    return 0;
}
