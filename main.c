/*
 * main.c
 *
 * metapixel
 *
 * Copyright (C) 1997-2009 Mark Probst
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
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include <glib.h>

#include "getopt.h"

#include "zoom.h"
#include "rwimg/readimage.h"
#include "rwimg/writeimage.h"
#include "lispreader/lispreader.h"

#include "cmdline.h"

static unsigned int num_metapixels = 0;

static library_t **libraries = 0;
static unsigned int num_libraries = 0;

/* default settings */

static char *default_prepare_directory = 0;
static int default_prepare_width = DEFAULT_PREPARE_WIDTH, default_prepare_height = DEFAULT_PREPARE_HEIGHT;
static string_list_t *default_library_directories = 0;
static int default_small_width = DEFAULT_WIDTH, default_small_height = DEFAULT_HEIGHT;
static int default_color_space = COLOR_SPACE_HSV;
static float default_weight_factors[NUM_CHANNELS] = { 1.0, 1.0, 1.0 };
static int default_metric = METRIC_SUBPIXEL;
static int default_search = SEARCH_LOCAL;
static int default_classic_min_distance = DEFAULT_CLASSIC_MIN_DISTANCE;
static int default_collage_min_distance = DEFAULT_COLLAGE_MIN_DISTANCE;
static int default_cheat_amount = 0;
static int default_forbid_reconstruction_radius = 0;
static unsigned int default_metapixel_flip = FLIP_HOR | FLIP_VER, default_prepare_flip = FLIP_HOR;

/* actual settings */

static int small_width, small_height;
static int color_space;
static float weight_factors[NUM_CHANNELS];
static int forbid_reconstruction_radius;

static int benchmark_rendering = 0;

static const char*
strip_path (const char *name)
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
add_library (library_t *library)
{
    ++num_libraries;

    if (num_libraries == 1)
	libraries = (library_t**)malloc(sizeof(library_t*));
    else
	libraries = (library_t**)realloc(libraries, num_libraries * sizeof(library_t*));

    assert(libraries != 0);

    libraries[num_libraries - 1] = library;

    num_metapixels += library->num_metapixels;
}

static void
init_metric (metric_t *metric, int kind)
{
    if (kind == METRIC_SUBPIXEL)
	metric_init(metric, METRIC_SUBPIXEL, color_space, weight_factors);
    else
	assert(0);
}

static void
print_current_time (void)
{
    struct timeval tv;

    gettimeofday(&tv, 0);

    printf("time: %lu %lu\n", (unsigned long)tv.tv_sec, (unsigned long)tv.tv_usec);
}

static void
generate_collage (char *input_name, char *output_name, float scale, int min_distance, int metric_kind, int cheat,
		  unsigned int allowed_flips)
{
    bitmap_t *in_bitmap, *out_bitmap;
    metric_t metric;
    collage_mosaic_t *mosaic;
    unsigned int scaled_small_width = (unsigned int)(small_width / scale);
    unsigned int scaled_small_height = (unsigned int)(small_height / scale);

    in_bitmap = bitmap_read(input_name);
    if (in_bitmap == 0)
    {
	fprintf(stderr, "Error: Could not read image `%s'.\n", input_name);
	exit(1);
    }

    init_metric(&metric, metric_kind);
    mosaic = collage_generate_from_bitmap(num_libraries, libraries, in_bitmap,
					  scaled_small_width, scaled_small_height,
					  scaled_small_width, scaled_small_height,
					  min_distance, &metric, allowed_flips, 0);
    assert(mosaic != 0);

    out_bitmap = collage_paste_to_bitmap(mosaic,
					 (unsigned int)(in_bitmap->width * scale),
					 (unsigned int)(in_bitmap->height * scale),
					 in_bitmap,
					 cheat * 0x10000 / 100,
					 0);
    assert(out_bitmap != 0);

    collage_free(mosaic);

    bitmap_free(in_bitmap);

    bitmap_write(out_bitmap, output_name);

    bitmap_free(out_bitmap);
}

static void
randomize_ptr_array (int length, gpointer *array)
{
    int i;

    for (i = 0; i < length - 1; ++i)
    {
	int index = g_random_int_range(i, length);
	gpointer tmp = array[i];

	array[i] = array[index];
	array[index] = tmp;
    }
}

static bitmap_t*
get_level_1_bitmap_for_metapixel (metapixel_t *metapixel, gpointer data)
{
    static GHashTable *cache = NULL;

    char *filename;
    bitmap_t *bitmap;

    if (cache != NULL)
    {
	bitmap = g_hash_table_lookup(cache, metapixel);
	if (bitmap)
	    return bitmap_copy(bitmap);
    }
    else
	cache = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_assert(metapixel->filename != NULL);
    filename = strrchr(metapixel->filename, '/');
    if (filename != NULL)
	++filename;
    else
	filename = metapixel->filename;

    filename = g_strdup_printf("/home/schani/Work/unix/metapixel/contact-favorites-level1/%s.png", filename);

    bitmap = bitmap_read(filename);
    g_assert(bitmap != NULL);

    g_free(filename);

    g_hash_table_insert(cache, metapixel, bitmap);

    return bitmap_copy(bitmap);
}

#define DUMP_ZOOMED

static void
generate_millions (char *input_name, char *output_name)
{
    bitmap_t *in_bitmap, *zoomed_in_bitmap, *out_bitmap, *zoomed_out_bitmap;
    int tile_width, metawidth, zoomed_width, num_passes, num_pass_pixels, sub_width, i;
    pixel_assignment_t *pixel_assignments;
    pixel_assignment_t **pixel_assignment_ptrs;

    in_bitmap = bitmap_read(input_name);
    if (in_bitmap == NULL)
    {
	fprintf(stderr, "Error: Could not read image `%s'.\n", input_name);
	exit(1);
    }

    tile_width = (int)ceilf(sqrtf(num_metapixels));
    g_assert(tile_width * tile_width >= num_metapixels);

    metawidth = (int)ceilf((float)MAX(in_bitmap->width, in_bitmap->height) / (float)tile_width);
    zoomed_width = tile_width * metawidth;

    num_passes = metawidth * metawidth;
    num_pass_pixels = tile_width * tile_width;

    g_print("doing %d passes of %d pixels each\n", num_passes, num_pass_pixels);

    zoomed_in_bitmap = bitmap_scale(in_bitmap, zoomed_width, zoomed_width, FILTER_MITCHELL);
    g_assert(zoomed_in_bitmap != NULL);

    pixel_assignment_ptrs = millions_generate_pixel_assignments(zoomed_width, zoomed_width, &pixel_assignments);
    randomize_ptr_array(zoomed_width * zoomed_width, (gpointer*)pixel_assignment_ptrs);

    for (i = 0; i < num_passes; ++i)
	millions_generate_from_pixel_assignments(num_libraries, libraries, zoomed_in_bitmap,
						 num_pass_pixels, pixel_assignment_ptrs + i * num_pass_pixels);
    g_free(pixel_assignment_ptrs);

    out_bitmap = millions_paste_image_from_pixel_assignments(zoomed_width, zoomed_width, pixel_assignments);
    g_assert(out_bitmap != NULL);

    zoomed_out_bitmap = bitmap_scale(out_bitmap, in_bitmap->width, in_bitmap->height, FILTER_MITCHELL);
    g_assert(zoomed_out_bitmap != NULL);

    bitmap_write(zoomed_out_bitmap, output_name);

    bitmap_free(out_bitmap);
    bitmap_free(zoomed_out_bitmap);

#ifdef DUMP_ZOOMED
    sub_width = (int)(zoomed_width / 1.5);
    while (sub_width >= 2)
    {
	int zoom_level = (int)ceilf((float)zoomed_width / (float)sub_width);
	char *filename = g_strdup_printf("/tmp/zoomed.%d.png", zoom_level);
	unsigned int cheat = (unsigned int)(logf(zoom_level) / logf(zoomed_width) * (float)0x10000);

	g_print("generating sub zoom %d with pixels of width %d - cheat %d percent\n", sub_width, zoom_level, cheat * 100 / 0x10000);

	out_bitmap = millions_paste_subimage_from_pixel_assignments(zoomed_width, zoomed_width,
								    (zoomed_width - sub_width) / 2, (zoomed_width - sub_width) / 2,
								    sub_width, sub_width,
								    zoom_level, zoom_level,
								    pixel_assignments,
								    cheat, get_level_1_bitmap_for_metapixel, NULL);
	g_assert(out_bitmap != NULL);
	zoomed_out_bitmap = bitmap_scale(out_bitmap, in_bitmap->width, in_bitmap->height, FILTER_MITCHELL);
	g_assert(zoomed_out_bitmap != NULL);

	bitmap_write(zoomed_out_bitmap, filename);

	g_free(filename);

	bitmap_free(out_bitmap);
	bitmap_free(zoomed_out_bitmap);

	sub_width = (int)(sub_width / 1.5);
    }
#endif

    g_free(pixel_assignments);

    bitmap_free(in_bitmap);
    bitmap_free(zoomed_in_bitmap);
}

static int
get_image_size (const char *filename, unsigned int *width, unsigned int *height)
{
    image_reader_t *reader = open_image_reading(filename);

    if (reader == 0)
	return 0;

    *width = reader->width;
    *height = reader->height;

    free_image_reader(reader);

    return 1;
}

static int
calculate_metasize (const char *filename, float scale, unsigned int *metawidth, unsigned int *metaheight)
{
    unsigned int width, height;

    if (!get_image_size(filename, &width, &height))
	return 0;

    width = width * scale;
    height = height * scale;

    *metawidth = (width + small_width - 1) / small_width;
    *metaheight = (height + small_height - 1) / small_height;

    return 1;
}

static classic_reader_t*
make_classic_reader (const char *in_image_name, float in_image_scale)
{
    tiling_t tiling;
    classic_reader_t *reader;
    unsigned int metawidth, metaheight;

    if (!calculate_metasize(in_image_name, in_image_scale, &metawidth, &metaheight))
	return 0;

    tiling_init_rectangular(&tiling, metawidth, metaheight);

    reader = classic_reader_new_from_file(in_image_name, &tiling);
    assert(reader != 0);

    return reader;
}

static int
make_classic_mosaic (char *in_image_name, char *out_image_name,
		     int metric_kind, float scale, int search, int min_distance, int cheat, unsigned int flip,
		     char *in_protocol_name, char *out_protocol_name)
{
    classic_mosaic_t *mosaic;

    if (in_protocol_name != 0)
    {
	int num_new_libraries;
	library_t **new_libraries;

	mosaic = classic_read(num_libraries, libraries, in_protocol_name, &num_new_libraries, &new_libraries);

	if (num_new_libraries > 0)
	{
	    int i;

	    for (i = 0; i < num_new_libraries; ++i)
		add_library(new_libraries[i]);

	    free(new_libraries);
	}

	if (mosaic == 0)
	    return 0;
    }
    else
    {
	classic_reader_t *reader;
	metric_t metric;
	matcher_t matcher;

	reader = make_classic_reader(in_image_name, scale);

	if (reader == 0)
	{
	    fprintf(stderr, "Error: cannot open input image `%s'.\n", in_image_name);
	    return 0;
	}

	init_metric(&metric, metric_kind);

	if (search == SEARCH_LOCAL)
	    matcher_init_local(&matcher, &metric, min_distance);
	else if (search == SEARCH_GLOBAL)
	    matcher_init_global(&matcher, &metric);
	else
	    assert(0);

	mosaic = classic_generate(num_libraries, libraries, reader, &matcher, forbid_reconstruction_radius, flip, 0);

	classic_reader_free(reader);
    }

    assert(mosaic != 0);

    if (benchmark_rendering)
    {
	int i;

	for (i = 0; i < mosaic->tiling.metawidth * mosaic->tiling.metaheight; ++i)
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
	    fprintf(stderr, "Error: Cannot open protocol file `%s' for writing: %s.\n", out_protocol_name, strerror(errno));
	    /* FIXME: free stuff */
	    return 0;
	}
	else
	    classic_write(mosaic, protocol_out);

	fclose(protocol_out);
    }

    {
	classic_reader_t *reader = 0;
	classic_writer_t *writer;
	unsigned int metawidth, metaheight;

	if (!calculate_metasize(in_image_name, scale, &metawidth, &metaheight))
	{
	    fprintf(stderr, "Error: could not read input image `%s'.\n", in_image_name);

	    classic_free(mosaic);
	    return 0;
	}

	if (cheat > 0)
	{
	    reader = make_classic_reader(in_image_name, scale);
	    assert(reader != 0);
	}

	writer = classic_writer_new_for_file(out_image_name, metawidth * small_width, metaheight * small_height);
	assert(writer != 0);

	classic_paste(mosaic, reader, cheat * 0x10000 / 100, writer, 0);

	classic_writer_free(writer);

	if (cheat > 0)
	    classic_reader_free(reader);
    }

    classic_free(mosaic);

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
		    else if (lisp_match_string("(metric #?(or subpixel))", obj, vars))
		    {
			if (strcmp(lisp_symbol(vars[0]), "subpixel") == 0)
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
		    else if (lisp_match_string("(prepare-flip #?(boolean) #?(boolean))", obj, vars))
		    {
			default_prepare_flip = 0;
			if (lisp_boolean(vars[0]))
			    default_prepare_flip |= FLIP_HOR;
			if (lisp_boolean(vars[1]))
			    default_prepare_flip |= FLIP_VER;
		    }
		    else if (lisp_match_string("(metapixel-flip #?(boolean) #?(boolean))", obj, vars))
		    {
			default_metapixel_flip = 0;
			if (lisp_boolean(vars[0]))
			    default_metapixel_flip |= FLIP_HOR;
			if (lisp_boolean(vars[1]))
			    default_metapixel_flip |= FLIP_VER;
		    }
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
	   "  metapixel --new-library <library-dir>\n"
	   "      create a new library\n"
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
	   "  -C, --color-space=SPACE      select color space (hsv, rgb, yiq)\n"
	   "                               default is hsv\n"
	   "  -W, --weights=A,B,C          assign relative weights to color channels\n"
	   "  -s  --scale=SCALE            scale input image by specified factor\n"
	   "  -m, --metric=METRIC          choose metric (only subpixel is valid)\n"
	   "  -e, --search=SEARCH          choose search method (local or global)\n"
	   "  -c, --collage                collage mode\n"
	   "  -M, --millions               millions mode\n"
	   "  -d, --distance=DIST          minimum distance between two instances of\n"
	   "                               the same constituent image\n"
	   "  -a, --cheat=PERC             cheat with specified percentage\n"
	   "  -f, --forbid-reconstruction=DIST\n"
	   "                               forbid placing antimosaic images on their\n"
	   "                               original locations or locations around it\n"
	   "  --flip=DIRECTIONS            specify along which axis images may be\n"
	   "                               flipped (no, x, y, xy)\n"
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
#define OPT_FLIP                       265
#define OPT_NEW_LIBRARY		       266

#define MODE_NONE		0
#define MODE_NEW_LIBRARY	1
#define MODE_PREPARE		2
#define MODE_METAPIXEL		3
#define MODE_BATCH		4

int
main (int argc, char *argv[])
{
    int mode = MODE_NONE;
    int metric;
    int search;
    gboolean collage = FALSE;
    gboolean millions = FALSE;
    int classic_min_distance, collage_min_distance;
    int cheat = 0;
    float scale = 1.0;
    char *out_filename = 0;
    char *in_filename = 0;
    char *antimosaic_filename = 0;
    string_list_t *library_directories = 0;
    int prepare_width = 0, prepare_height = 0;
    unsigned int flip = 0xdeadbeef;

    read_rc_file();

    small_width = default_small_width;
    small_height = default_small_height;
    prepare_width = default_prepare_width;
    prepare_height = default_prepare_height;
    color_space = default_color_space;
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
		{ "new-library", no_argument, 0, OPT_NEW_LIBRARY },
		{ "prepare", no_argument, 0, OPT_PREPARE },
		{ "metapixel", no_argument, 0, OPT_METAPIXEL },
		{ "batch", no_argument, 0, OPT_BATCH },
		{ "out", required_argument, 0, OPT_OUT },
		{ "in", required_argument, 0, OPT_IN },
		{ "benchmark-rendering", no_argument, 0, OPT_BENCHMARK_RENDERING },
		{ "print-prepare-settings", no_argument, 0, OPT_PRINT_PREPARE_SETTINGS },
		{ "flip", required_argument, 0, OPT_FLIP },
		{ "library", required_argument, 0, 'l' },
		{ "width", required_argument, 0, 'w' },
		{ "height", required_argument, 0, 'h' },
		{ "color-space", required_argument, 0, 'C' },
		{ "weights", required_argument, 0, 'W' },
		{ "scale", required_argument, 0, 's' },
		{ "collage", no_argument, 0, 'c' },
		{ "millions", no_argument, 0, 'M' },
		{ "distance", required_argument, 0, 'd' },
		{ "cheat", required_argument, 0, 'a' },
		{ "metric", required_argument, 0, 'm' },
		{ "search", required_argument, 0, 'e' },
		{ "antimosaic", required_argument, 0, 'x' },
		{ "forbid-reconstruction", required_argument, 0, 'f' },
		{ 0, 0, 0, 0 }
	    };

	int option, option_index;

	option = getopt_long(argc, argv, "l:m:e:w:h:C:W:s:cMd:a:x:f:", long_options, &option_index);

	if (option == -1)
	    break;

	switch (option)
	{
	    case OPT_NEW_LIBRARY :
		mode = MODE_NEW_LIBRARY;
		break;

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

	    case OPT_FLIP :
		flip = 0;
		if (strchr(optarg, 'x'))
		    flip |= FLIP_HOR;
		if (strchr(optarg, 'y'))
		    flip |= FLIP_VER;
		break;

	    case 'm' :
		if (strcmp(optarg, "subpixel") == 0)
		    metric = METRIC_SUBPIXEL;
		else
		{
		    fprintf(stderr, "metric must either be subpixel\n");
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
		    fprintf(stderr, "Error: Search method must either be local or global.\n");
		    return 1;
		}
		break;

	    case 'w' :
		small_width = prepare_width = atoi(optarg);
		break;

	    case 'h' :
		small_height = prepare_height = atoi(optarg);
		break;

	    case 'W' :
		{
		    char **strs = g_strsplit(optarg, ",", 3);
		    int i;

		    if (!(strs[0] && strs[1] && strs[2] && !strs[3]))
		    {
			fprintf(stderr, "Error: Weights must be of the form `A,B,C' where A, B and C are numbers.\n");
			return 1;
		    }

		    for (i = 0; i < 3; ++i)
			weight_factors[i] = (float)g_ascii_strtod(strs[i], NULL);

		    g_strfreev(strs);
		}
		break;

	    case 'C' :
		if (strcmp(optarg, "rgb") == 0)
		    color_space = COLOR_SPACE_RGB;
		else if (strcmp(optarg, "hsv") == 0)
		    color_space = COLOR_SPACE_HSV;
		else if (strcmp(optarg, "yiq") == 0)
		    color_space = COLOR_SPACE_YIQ;
		else
		{
		    fprintf(stderr, "Error: Unknown color space `%s'.  Valid choices are `rgb', `hsv' and `yiq'.\n",
			    optarg);
		    return 1;
		}
		break;

	    case 's':
		scale = atof(optarg);
		break;

	    case 'c' :
		collage = TRUE;
		break;

	    case 'M' :
		millions = TRUE;
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
		    fprintf(stderr, "Error: --library and --antimosaic cannot be used together.\n");
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
		       "Copyright (C) 1997-2009 Mark Probst\n"
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

    if (flip == 0xdeadbeef)
    {
	if (mode == MODE_METAPIXEL || mode == MODE_BATCH)
	    flip = default_metapixel_flip;
	else
	    flip = default_prepare_flip;
    }

    /* check settings for soundness */
    if (collage && millions)
    {
	fprintf(stderr, "Error: --collage and --millions cannot be used together.\n");
	return 1;
    }
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
	    fprintf(stderr, "Error: The --in and --out options can only be used in metapixel mode\n");
	    return 1;
	}
	if (collage)
	{
	    fprintf(stderr, "Error: The --in and --out options can only be used for classic mosaics\n");
	    return 1;
	}
    }

    if (mode == MODE_NEW_LIBRARY)
    {
	char *library_name;
	library_t *library;

	if (argc - optind != 1)
	{
	    usage();
	    return 1;
	}

	library_name = argv[optind];

	mkdir(library_name, 0777);

	library = library_new(library_name);
	if (library == 0)
	    return 1;

	library_close(library);

	return 0;
    }
    else if (mode == MODE_PREPARE)
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
	    return 1;

	bitmap = bitmap_read(inimage_name);
	if (bitmap == 0)
	{
	    fprintf(stderr, "Error: could not read image `%s'.\n", inimage_name);
	    return 1;
	}

	pixel = metapixel_new_from_bitmap(bitmap, strip_path(inimage_name), prepare_width, prepare_height);
	assert(pixel != 0);

	bitmap_free(bitmap);

	pixel->flip = flip;

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

	    for (lst = library_directories; lst != 0; lst = lst->next)
	    {
		library_t *library = library_open(lst->str);

		if (library == 0)
		    return 1;

		add_library(library);
	    }

	    forbid_reconstruction_radius = 0;
	}
	else
	{
	    fprintf(stderr, "Error: you must give one of the option --library and --antimosaic.\n");
	    return 1;
	}

	if (mode == MODE_METAPIXEL)
	{
	    if (collage)
		generate_collage(argv[optind], argv[optind + 1], scale, collage_min_distance,
				 metric, cheat, flip);
	    else if (millions)
		generate_millions(argv[optind], argv[optind + 1]);
	    else
		make_classic_mosaic(argv[optind], argv[optind + 1],
				    metric, scale, search, classic_min_distance, cheat, flip,
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
			    else if (lisp_match_string("(metric #?(or subpixel))",
						       lisp_car(lst), &var))
			    {
				if (strcmp(lisp_symbol(var), "subpixel") == 0)
				    this_metric = METRIC_SUBPIXEL;
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
					    this_metric, this_scale, this_search, this_min_distance, this_cheat, 0,
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
