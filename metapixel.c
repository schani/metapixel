/* -*- c -*- */

/*
 * metapixel.c
 *
 * metapixel
 *
 * Copyright (C) 1997-1999 Mark Probst
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <magick/config.h>
#include <magick/magick.h>

#include "getopt.h"

#include "vector.h"
#include "rwpng.h"

#ifndef MIN
#define MIN(a,b)           ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)           ((a)>(b)?(a):(b))
#endif

#define DEFAULT_WIDTH       64
#define DEFAULT_HEIGHT      64

#define IMAGE_SIZE          64
#define ROW_LENGTH          (IMAGE_SIZE * 3)
#define SIGNIFICANT_COEFFS  40

typedef struct
{
    int index;
    float coeff;
    int sign;
} coefficient_with_index_t;

typedef struct
{
    int index;
    float weight;
} index_with_weight_t;

typedef struct
{
    int num_positives;
    index_with_weight_t positives[SIGNIFICANT_COEFFS];

    int num_negatives;
    index_with_weight_t negatives[SIGNIFICANT_COEFFS];
} search_coefficients_t;

int index_order[IMAGE_SIZE * IMAGE_SIZE];

typedef struct _metapixel_t
{
    char filename[1024];
    search_coefficients_t coeffs[3];
    float means[3];
    struct _metapixel_t *next;
} metapixel_t;

static metapixel_t *first_pixel = 0;

float sqrt_of_two, sqrt_of_image_size;

float weight_factors[3] = { 1.0, 1.0, 1.0 };

int small_width = DEFAULT_WIDTH, small_height = DEFAULT_HEIGHT;

void
generate_index_order (void)
{
    int index = 0,
	begin = 0,
	row = 0,
	col = 0,
	first_half = 1;

    do
    {
	index_order[index++] = 3 * (col + IMAGE_SIZE * row);

	if (first_half)
	{
	    if (row == 0)
	    {
		if (begin == IMAGE_SIZE - 1)
		{
		    first_half = 0;
		    col = begin = 1;
		    row = IMAGE_SIZE - 1;
		}
		else
		{
		    ++begin;
		    row = begin;
		    col = 0;
		}
	    }
	    else
	    {
		++col;
		--row;
	    }
	}
	else
	{
	    if (col == IMAGE_SIZE - 1)
	    {
		++begin;
		col = begin;
		row = IMAGE_SIZE - 1;
	    }
	    else
	    {
		++col;
		--row;
	    }
	}
    } while (index < IMAGE_SIZE * IMAGE_SIZE);
}

void
cut_last_coefficients (float *image, int channel, int howmany)
{
    int i;

    for (i = IMAGE_SIZE * IMAGE_SIZE - howmany; i < IMAGE_SIZE * IMAGE_SIZE; ++i)
	image[channel + index_order[i]] = 0.0;
}

void
transform_rgb_to_yiq (float *image)
{
    static Matrix3D conversion_matrix =
    {
	{ 0.299, 0.587, 0.114 },
	{ 0.596, -0.275, -0.321 },
	{ 0.212, -0.528, 0.311 }
    };

    int i;

    for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE; ++i)
    {
	Vector3D rgb_vec,
	    yiq_vec;

	InitVector3D(&rgb_vec, image[3 * i + 0], image[3 * i + 1], image[3 * i + 2]);
	MultMatrixVector3D(&yiq_vec, (const Matrix3D*)&conversion_matrix, &rgb_vec);
	image[3 * i + 0] = yiq_vec.x;
	image[3 * i + 1] = yiq_vec.y + 127.5;
	image[3 * i + 2] = yiq_vec.z + 127.5;
    }
}

void
transform_yiq_to_rgb (float *image)
{
    static Matrix3D conversion_matrix =
    {
	{ 1.00308929854,  0.954849063112,   0.61785970812 },
	{ 0.996776058337, -0.270706233074, -0.644788332692 },
	{ 1.00849783766,   -1.1104851847,   1.69956753125 }
    };

    int i;

    for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE; ++i)
    {
	Vector3D rgb_vec,
	    yiq_vec;

	InitVector3D(&yiq_vec, image[3 * i + 0], image[3 * i + 1] - 127.5, image[3 * i + 2] - 127.5);
	MultMatrixVector3D(&rgb_vec, (const Matrix3D*)&conversion_matrix, &yiq_vec);
	image[3 * i + 0] = rgb_vec.x;
	image[3 * i + 1] = rgb_vec.y;
	image[3 * i + 2] = rgb_vec.z;
    }
}

void
transpose_image (float *old_image, float *new_image)
{
    int i,
	j,
	color;

    for (i = 0; i < IMAGE_SIZE; ++i)
	for (j = 0; j < IMAGE_SIZE; ++j)
	    for (color = 0; color < 3; ++color)
		new_image[color + i * 3 + j * 3 * IMAGE_SIZE] =
		    old_image[color + j * 3 + i * 3 * IMAGE_SIZE];
}

void
output_row (float *row)
{
    int i;

    for (i = 0; i < IMAGE_SIZE; ++i)
	printf("%d ", (int)row[i * 3 + 2]);
    printf("\n");
}

void
output_image (float *image)
{
    int i;

    for (i = 0; i < IMAGE_SIZE; ++i)
	output_row(image + 3 * IMAGE_SIZE * i);
    printf("\n");
}

void
decompose_row (float *row)
{
    int h = IMAGE_SIZE,
	i;
    float new_row[3 * IMAGE_SIZE];

    for (i = 0; i < 3 * IMAGE_SIZE; ++i)
	row[i] = row[i] / sqrt_of_image_size;

    while (h > 1)
    {
	h = h / 2;
	for (i = 0; i < h; ++i)
	{
	    int color;

	    for (color = 0; color < 3; ++color)
	    {
		float val1 = row[color + 2 * i * 3],
		    val2 = row[color + (2 * i + 1) * 3];

		new_row[color + i * 3] = (val1 + val2) / sqrt_of_two;
		new_row[color + (h + i) * 3] = (val1 - val2) / sqrt_of_two;
	    }
	}
	memcpy(row, new_row, sizeof(float) * 3 * h * 2);
    }
}

void
decompose_column (float *column)
{
    int h = IMAGE_SIZE,
	i,
	channel;
    float new_column[ROW_LENGTH];

    for (i = 0; i < IMAGE_SIZE; ++i)
	for (channel = 0; channel < 3; ++channel)
	    column[channel + i * ROW_LENGTH] =
		column[channel + i * ROW_LENGTH] / sqrt_of_image_size;

    while (h > 1)
    {
	h = h / 2;
	for (i = 0; i < h; ++i)
	{
	    for (channel = 0; channel < 3; ++channel)
	    {
		float val1 = column[channel + (2 * i) * ROW_LENGTH],
		    val2 = column[channel + (2 * i + 1) * ROW_LENGTH];

		new_column[channel + i * 3] = (val1 + val2) / sqrt_of_two;
		new_column[channel + (h + i) * 3] = (val1 - val2) / sqrt_of_two;
	    }
	}

	for (i = 0; i < h * 2; ++i)
	    for (channel = 0; channel < 3; ++channel)
		column[channel + i * ROW_LENGTH] = new_column[channel + i * 3];
    }
}

void
decompose_image (float *image)
{
    int row;

    for (row = 0; row < IMAGE_SIZE; ++row)
	decompose_row(image + 3 * IMAGE_SIZE * row);
    for (row = 0; row < IMAGE_SIZE; ++row)
	decompose_column(image + 3 * row);
}

void
compose_row (float *row)
{
    int h = 1,
	i;
    float new_row[3 * IMAGE_SIZE];

    memcpy(new_row, row, sizeof(float) * 3 * IMAGE_SIZE);
    while (h < IMAGE_SIZE)
    {
	for (i = 0; i < h; ++i)
	{
	    int color;

	    for (color = 0; color < 3; ++color)
	    {
		float val1 = row[color + i * 3],
		    val2 = row[color + (h + i) * 3];

		new_row[color + 2 * i * 3] = (val1 + val2) / sqrt_of_two;
		new_row[color + (2 * i + 1) * 3] = (val1 - val2) / sqrt_of_two;
	    }
	}
	memcpy(row, new_row, sizeof(float) * 3 * IMAGE_SIZE);
	h = h * 2;
    }

    for (i = 0; i < 3 * IMAGE_SIZE; ++i)
	row[i] = row[i] * sqrt_of_image_size;
}

void
compose_image (float *image)
{
    int row;
    float *transposed_image = (float*)malloc(sizeof(float) * 3 * IMAGE_SIZE * IMAGE_SIZE);

    transpose_image(image, transposed_image);
    for (row = 0; row < IMAGE_SIZE; ++row)
	compose_row(transposed_image + 3 * IMAGE_SIZE * row);
    transpose_image(transposed_image, image);
    for (row = 0; row < IMAGE_SIZE; ++row)
	compose_row(image + 3 * IMAGE_SIZE * row);

    free(transposed_image);
}

int
compare_coeffs_with_index (const void *p1, const void *p2)
{
    const coefficient_with_index_t *coeff1 = (const coefficient_with_index_t*)p1,
	*coeff2 = (const coefficient_with_index_t*)p2;

    if (fabs(coeff1->coeff) < fabs(coeff2->coeff))
	return 1;
    else if (fabs(coeff1->coeff) == fabs(coeff2->coeff))
	return 0;
    return -1;
}

void
find_highest_coefficients (float *image, coefficient_with_index_t highest_coeffs[3][SIGNIFICANT_COEFFS])
{
    int index,
	channel;

    for (index = 1; index < SIGNIFICANT_COEFFS + 1; ++index)
	for (channel = 0; channel < 3; ++channel)
	{
	    highest_coeffs[channel][index - 1].index = index;
	    highest_coeffs[channel][index - 1].coeff = image[channel + 3 * index];
	}

    for (channel = 0; channel < 3; ++channel)
	qsort(highest_coeffs[channel], SIGNIFICANT_COEFFS, sizeof(coefficient_with_index_t),
	      compare_coeffs_with_index);

    for (; index < IMAGE_SIZE * IMAGE_SIZE; ++index)
    {
	for (channel = 0; channel < 3; ++channel)
	{
	    if (fabs(image[channel + 3 * index])
		> fabs(highest_coeffs[channel][SIGNIFICANT_COEFFS - 1].coeff))
	    {
		int significance;

		for (significance = SIGNIFICANT_COEFFS - 2; significance >= 0; --significance)
		    if (fabs(image[channel + 3 * index])
			<= fabs(highest_coeffs[channel][significance].coeff))
			break;
		++significance;
		memmove(highest_coeffs[channel] + significance + 1,
			highest_coeffs[channel] + significance,
			sizeof(coefficient_with_index_t) * (SIGNIFICANT_COEFFS - 1 - significance));
		highest_coeffs[channel][significance].index = index;
		highest_coeffs[channel][significance].coeff = image[channel + 3 * index];
	    }
	}
    }

    for (index = 0; index < SIGNIFICANT_COEFFS; ++index)
	for (channel = 0; channel < 3; ++channel)
	    if (highest_coeffs[channel][index].coeff > 0.0)
		highest_coeffs[channel][index].sign = 1;
	    else
		highest_coeffs[channel][index].sign = -1;
}

float
weight_function (int channel, int index)
{
    static float weight_table[3][6] =
    {
	{ 5.00, 0.83, 1.01, 0.52, 0.47, 0.30 },
	{ 19.21, 1.26, 0.44, 0.53, 0.28, 0.14 },
	{ 34, 0.36, 0.45, 0.14, 0.18, 0.27 }
    };

    int i = index % IMAGE_SIZE,
	j = index / IMAGE_SIZE,
	bin = MIN(MAX(i, j), 5);

    return weight_table[channel][bin] * weight_factors[channel];
}

float
compare_images (search_coefficients_t query[3], float *query_means,
		search_coefficients_t target[3], float *target_means)
{
    float score = 0.0;
    int channel;

    for (channel = 0; channel < 3; ++channel)
    {
	int i;
	int j;

	score += weight_function(channel, 0)
	    * fabs(query_means[channel] - target_means[channel]) * 0.05;

	j = 0;
	for (i = 0; i < query[channel].num_positives; ++i)
	{
	    while (target[channel].positives[j].index < query[channel].positives[i].index
		   && j < target[channel].num_positives)
		++j;

	    if (j >= target[channel].num_positives)
		break;

	    if (target[channel].positives[j].index > query[channel].positives[i].index)
		continue;

	    score -= target[channel].positives[j].weight;
	}

	j = 0;
	for (i = 0; i < query[channel].num_negatives; ++i)
	{
	    while (target[channel].negatives[j].index < query[channel].negatives[i].index
		   && j < target[channel].num_negatives)
		++j;

	    if (j >= target[channel].num_negatives)
		break;

	    if (target[channel].negatives[j].index > query[channel].negatives[i].index)
		continue;

	    score -= target[channel].negatives[j].weight;
	}
    }

    return score;
}

void
add_metapixel (metapixel_t *pixel)
{
    pixel->next = first_pixel;
    first_pixel = pixel;
}

int
compare_indexes_with_weight (const void *p1, const void *p2)
{
    index_with_weight_t *i1 = (index_with_weight_t*)p1;
    index_with_weight_t *i2 = (index_with_weight_t*)p2;

    return i1->index - i2->index;
}

void
generate_search_coeffs (search_coefficients_t search_coeffs[3], coefficient_with_index_t raw_coeffs[3][SIGNIFICANT_COEFFS])
{
    int channel;

    for (channel = 0; channel < 3; ++channel)
    {
	int i;

	search_coeffs[channel].num_positives = search_coeffs[channel].num_negatives = 0;

	for (i = 0; i < SIGNIFICANT_COEFFS; ++i)
	{
	    if (raw_coeffs[channel][i].sign > 0)
	    {
		int j = search_coeffs[channel].num_positives++;

		search_coeffs[channel].positives[j].index = raw_coeffs[channel][i].index;
		search_coeffs[channel].positives[j].weight = weight_function(channel, raw_coeffs[channel][i].index);
	    }
	    else
	    {
		int j = search_coeffs[channel].num_negatives++;

		search_coeffs[channel].negatives[j].index = raw_coeffs[channel][i].index;
		search_coeffs[channel].negatives[j].weight = weight_function(channel, raw_coeffs[channel][i].index);
	    }
	}

	assert(search_coeffs[channel].num_positives + search_coeffs[channel].num_negatives == SIGNIFICANT_COEFFS);

	qsort(search_coeffs[channel].positives, search_coeffs[channel].num_positives,
	      sizeof(index_with_weight_t), compare_indexes_with_weight);
	qsort(search_coeffs[channel].negatives, search_coeffs[channel].num_negatives,
	      sizeof(index_with_weight_t), compare_indexes_with_weight);
    }
}

void
generate_search_coeffs_for_subimage (search_coefficients_t search_coeffs[3], float means[3], Image *image, int x, int y, int use_crop)
{
    static float *float_image = 0;

    RectangleInfo rect;
    Image *subimage, *scaled_image;
    int i;
    coefficient_with_index_t raw_coeffs[3][SIGNIFICANT_COEFFS];

    if (float_image == 0)
	float_image = (float*)malloc(sizeof(float) * IMAGE_SIZE * IMAGE_SIZE * 3);

    if (use_crop)
    {
	rect.x = x;
	rect.y = y;
	rect.width = small_width;
	rect.height = small_height;

	subimage = CropImage(image, &rect);
	assert(subimage != 0);

	if (small_width != IMAGE_SIZE || small_height != IMAGE_SIZE)
	{
	    scaled_image = ScaleImage(subimage, IMAGE_SIZE, IMAGE_SIZE);
	    assert(scaled_image != 0);
	    DestroyImage(subimage);
	}
	else
	    scaled_image = subimage;

	UncondenseImage(scaled_image);

	for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE; ++i)
	{
	    float_image[i * 3 + 0] = DownScale(scaled_image->pixels[i].red);
	    float_image[i * 3 + 1] = DownScale(scaled_image->pixels[i].green);
	    float_image[i * 3 + 2] = DownScale(scaled_image->pixels[i].blue);
	}
    }
    else
    {
	int j;

	x /= small_width;
	y /= small_height;

	for (j = 0; j < IMAGE_SIZE; ++j)
	    for (i = 0; i < IMAGE_SIZE; ++i)
	    {
		float_image[(j * IMAGE_SIZE + i) * 3 + 0] = DownScale(image->pixels[(y * IMAGE_SIZE + j) * image->columns + x * IMAGE_SIZE + i].red);
		float_image[(j * IMAGE_SIZE + i) * 3 + 1] = DownScale(image->pixels[(y * IMAGE_SIZE + j) * image->columns + x * IMAGE_SIZE + i].green);
		float_image[(j * IMAGE_SIZE + i) * 3 + 2] = DownScale(image->pixels[(y * IMAGE_SIZE + j) * image->columns + x * IMAGE_SIZE + i].blue);
	    }
    }

    transform_rgb_to_yiq(float_image);
    decompose_image(float_image);
    find_highest_coefficients(float_image, raw_coeffs);

    generate_search_coeffs(search_coeffs, raw_coeffs);

    for (i = 0; i < 3; ++i)
	means[i] = float_image[i];
}

metapixel_t*
metapixel_nearest_to (search_coefficients_t search_coeffs[3], float *query_means)
{
    float best_score = 1e99;
    metapixel_t *best_fit = 0, *pixel;

    for (pixel = first_pixel; pixel != 0; pixel = pixel->next)
    {
	float score = compare_images(search_coeffs, query_means, pixel->coeffs, pixel->means);

	if (best_fit == 0 || score < best_score)
	{
	    best_score = score;
	    best_fit = pixel;
	}
    }

    return best_fit;
}

void
paste_metapixel (metapixel_t *pixel, unsigned char *data, int width, int height, int x, int y)
{
    int i;
    int pixel_width, pixel_height;
    unsigned char *pixel_data = read_png_file(pixel->filename, &pixel_width, &pixel_height);

    for (i = 0; i < small_height; ++i)
	memcpy(data + 3 * (x + (y + i) * width), pixel_data + 3 * i * small_width, 3 * small_width);

    free(pixel_data);
}

void
usage (void)
{
    printf("Usage:\n"
	   "  metapixel --version\n"
	   "      print out version number\n"
	   "  metapixel --help\n"
	   "      print this help text\n"
	   "  metapixel [option ...] --prepare <inimage> <outimage> <tablefile>\n"
	   "      calculate and output tables for <file>\n"
	   "  metapixel [option ...] --metapixel <in> <out>\n"
	   "      transform <in> to <out>\n"
	   "Options:\n"
	   "  -w, --width=WIDTH           set width for small images\n"
	   "  -h, --height=HEIGHT         set height for small images\n"
	   "  -y, --y-weight=WEIGHT       assign relative weight for the Y-channel\n"
	   "  -i, --i-weight=WEIGHT       assign relative weight for the I-channel\n"
	   "  -q, --q-weight=WEIGHT       assign relative weight for the Q-channel\n"
	   "  -c, --collage               collage mode\n"
	   "\n"
	   "Report bugs to schani@complang.tuwien.ac.at\n");
}

#define MODE_NONE       0
#define MODE_PREPARE    1
#define MODE_METAPIXEL  2

int
main (int argc, char *argv[])
{
    int mode = MODE_NONE;
    int collage = 0;

    while (1)
    {
	static struct option long_options[] =
            {
		{ "version", no_argument, 0, 256 },
		{ "help", no_argument, 0, 257 },
		{ "prepare", no_argument, 0, 'p' },
		{ "metapixel", no_argument, 0, 'm' },
		{ "width", required_argument, 0, 'w' },
		{ "height", required_argument, 0, 'h' },
		{ "y-weight", required_argument, 0, 'y' },
		{ "i-weight", required_argument, 0, 'i' },
		{ "q-weight", required_argument, 0, 'q' },
		{ "collage", no_argument, 0, 'c' },
		{ 0, 0, 0, 0 }
	    };

	int option,
	    option_index;

	option = getopt_long(argc, argv, "pmy:i:q:", long_options, &option_index);

	if (option == -1)
	    break;

	switch (option)
	{
	    case 'p' :
		mode = MODE_PREPARE;
		break;

	    case 'm' :
		mode = MODE_METAPIXEL;
		break;

	    case 'w' :
		small_width = atoi(optarg);
		break;

	    case 'h' :
		small_height = atoi(optarg);
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

	    case 'c' :
		collage = 1;
		break;

	    case 256 :
		printf("metapixel 0.1\n"
		       "\n"
		       "Copyright (C) 1997-1999 Mark Probst\n"
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

	    case 257 :
		usage();
		return 0;
	}
    }

    sqrt_of_two = sqrt(2);
    sqrt_of_image_size = sqrt(IMAGE_SIZE);

    if (mode == MODE_PREPARE)
    {
	float *float_image = (float*)malloc(sizeof(float) * 3 * IMAGE_SIZE * IMAGE_SIZE);
	int i, channel;
	coefficient_with_index_t highest_coeffs[3][SIGNIFICANT_COEFFS];
	unsigned char *image_data;
	char *inimage_name, *outimage_name, *tables_name;
	Image *image, *scaled;
	ImageInfo image_info;
	FILE *tables_file;

	if (argc - optind != 3)
	{
	    usage();
	    return 1;
	}

	inimage_name = argv[optind + 0];
	outimage_name = argv[optind + 1];
	tables_name = argv[optind + 2];

	GetImageInfo(&image_info);
	strcpy(image_info.filename, inimage_name);
	image = ReadImage(&image_info);
	if (image == 0)
	{
	    fprintf(stderr, "could not read image %s\n", inimage_name);
	    return 1;
	}

	tables_file = fopen(tables_name, "a");
	if (tables_file == 0)
	{
	    fprintf(stderr, "could not open file %s for writing\n", tables_name);
	    return 1;
	}

	image->filter = MitchellFilter;
	scaled = ZoomImage(image, 64, 64);
	assert(scaled != 0);

	TransformRGBImage(scaled, RGBColorspace);
	UncondenseImage(scaled);

	image_data = (unsigned char*)malloc(3 * IMAGE_SIZE * IMAGE_SIZE);
	for (i = 0; i < IMAGE_SIZE * IMAGE_SIZE; ++i)
	{
	    image_data[i * 3 + 0] = DownScale(scaled->pixels[i].red);
	    image_data[i * 3 + 1] = DownScale(scaled->pixels[i].green);
	    image_data[i * 3 + 2] = DownScale(scaled->pixels[i].blue);

	    float_image[i * 3 + 0] = DownScale(scaled->pixels[i].red);
	    float_image[i * 3 + 1] = DownScale(scaled->pixels[i].green);
	    float_image[i * 3 + 2] = DownScale(scaled->pixels[i].blue);
	}

	write_png_file(outimage_name, IMAGE_SIZE, IMAGE_SIZE, image_data);
	free(image_data);

	transform_rgb_to_yiq(float_image);
	decompose_image(float_image);
	
	find_highest_coefficients(float_image, highest_coeffs);

	fprintf(tables_file, "%s\n", outimage_name);
	for (channel = 0; channel < 3; ++channel)
	    fprintf(tables_file, "%f ", float_image[channel]);
	fprintf(tables_file, "\n");
	for (channel = 0; channel < 3; ++channel)
	{
	    for (i = 0; i < SIGNIFICANT_COEFFS; ++i)
		fprintf(tables_file, "%d %d ",
			highest_coeffs[channel][i].index,
			highest_coeffs[channel][i].sign);
	    fprintf(tables_file, "\n");
	}
	fprintf(tables_file, "\n");

	fclose(tables_file);
    }
    else if (mode == MODE_METAPIXEL)
    {
	unsigned char *out_image_data;
	int in_image_width, in_image_height;
	Image *image;
	ImageInfo image_info;
	int use_crop = 1;

	if (argc - optind != 2)
	{
	    usage();
	    return 1;
	}

	do
	{
	    coefficient_with_index_t coeffs[3][SIGNIFICANT_COEFFS];
	    metapixel_t *pixel = (metapixel_t*)malloc(sizeof(metapixel_t));
	    int channel;
	    int i;

	    scanf("%s", pixel->filename);
	    if (feof(stdin))
		break;
	    for (channel = 0; channel < 3; ++channel)
		scanf("%f", &pixel->means[channel]);
	    for (channel = 0; channel < 3; ++channel)
	    {
		for (i = 0; i < SIGNIFICANT_COEFFS; ++i)
		    scanf("%d %d",
			  &coeffs[channel][i].index,
			  &coeffs[channel][i].sign);
	    }

	    generate_search_coeffs(pixel->coeffs, coeffs);

	    add_metapixel(pixel);
	} while (!feof(stdin));
	
	GetImageInfo(&image_info);
	strcpy(image_info.filename, argv[optind]);
	image = ReadImage(&image_info);
	if (image == 0)
	{
	    fprintf(stderr, "could not read image %s\n", argv[optind]);
	    return 1;
	}

	in_image_width = image->columns;
	in_image_height = image->rows;

	if (!collage && (small_width != IMAGE_SIZE || small_height != IMAGE_SIZE))
	{
	    Image *scaled_image;

	    scaled_image = ScaleImage(image, in_image_width / small_width * IMAGE_SIZE, in_image_height / small_height * IMAGE_SIZE);
	    assert(scaled_image != 0);
	    DestroyImage(image);
	    image = scaled_image;
	}

	if (!collage || (small_width == IMAGE_SIZE || small_height == IMAGE_SIZE))
	{
	    UncondenseImage(image);

	    use_crop = 0;
	}

	out_image_data = (unsigned char*)malloc(in_image_width * in_image_height * 3);

	if (!collage)
	{
	    int x, y;

	    assert(in_image_width % small_width == 0);
	    assert(in_image_height % small_height == 0);

	    for (y = 0; y < in_image_height / small_height; ++y)
		for (x = 0; x < in_image_width / small_width; ++x)
		{
		    search_coefficients_t search_coeffs[3];
		    metapixel_t *pixel;
		    float means[3];

		    generate_search_coeffs_for_subimage(search_coeffs, means, image,
							x * small_width, y * small_height, use_crop);

		    pixel = metapixel_nearest_to(search_coeffs, means);
		    paste_metapixel(pixel, out_image_data, in_image_width, in_image_height, x * small_width, y * small_height);

		    printf(".");
		    fflush(stdout);
		}
	}
	else
	{
	    char *bitmap;
	    int num_pixels_done = 0;

	    bitmap = (char*)malloc(in_image_width * in_image_height);
	    memset(bitmap, 0, in_image_width * in_image_height);

	    while (num_pixels_done < in_image_width * in_image_height)
	    {
		int i, j;
		int x, y;
		search_coefficients_t search_coeffs[3];
		metapixel_t *pixel;
		float means[3];

		while (1)
		{
		    x = random() % in_image_width - small_width / 2;
		    y = random() % in_image_height - small_height / 2;

		    if (x < 0)
			x = 0;
		    if (x + small_width > in_image_width)
			x = in_image_width - small_width;

		    if (y < 0)
			y = 0;
		    if (y + small_height > in_image_height)
			y = in_image_height - small_height;

		    for (j = 0; j < small_height; ++j)
			for (i = 0; i < small_width; ++i)
			    if (!bitmap[(y + j) * in_image_width + x + i])
				goto out;
		}

	    out:
		generate_search_coeffs_for_subimage(search_coeffs, means, image,
						    x * small_width, y * small_height, use_crop);

		pixel = metapixel_nearest_to(search_coeffs, means);
		paste_metapixel(pixel, out_image_data, in_image_width, in_image_height, x, y);

		for (j = 0; j < small_height; ++j)
		    for (i = 0; i < small_width; ++i)
			if (!bitmap[(y + j) * in_image_width + x + i])
			{
			    bitmap[(y + j) * in_image_width + x + i] = 1;
			    ++num_pixels_done;
			}

		printf(".");
		fflush(stdout);
	    }
	}

	printf("done\n");

	write_png_file(argv[optind + 1], in_image_width, in_image_height, out_image_data);
    }
    else
    {
	usage();
	return 1;
    }

    return 0;
}
