/* -*- c -*- */

/*
 * convert.c
 *
 * metapixel
 *
 * Copyright (C) 2003-2004 Mark Probst
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
#include <string.h>
#include <assert.h>

#include "getopt.h"
#include "lispreader.h"

#include "metapixel.h"

int
main (int argc, char *argv[])
{
    int small_width = DEFAULT_WIDTH, small_height = DEFAULT_HEIGHT;

    while (1)
    {
	static struct option long_options[] =
            {
		{ "width", required_argument, 0, 'w' },
		{ "height", required_argument, 0, 'h' },
		{ 0, 0, 0, 0 }
	    };

	int option, option_index;

	option = getopt_long(argc, argv, "w:h:", long_options, &option_index);

	if (option == -1)
	    break;

	switch (option)
	{
	    case 'w' :
		small_width = atoi(optarg);
		break;

	    case 'h' :
		small_height = atoi(optarg);
		break;

	    default :
		assert(0);
	}
    }

    printf("; -*- lisp -*-\n");

    do
    {
	int i, channel;
	char filename[1024];
	lisp_object_t *obj;
	float means[NUM_CHANNELS];

	scanf("%s", filename);
	if (feof(stdin))
	    break;

	assert(strlen(filename) > 0);

	printf("(small-image ");

	obj = lisp_make_string(filename);
	lisp_dump(obj, stdout);
	lisp_free(obj);

	printf(" (size %d %d) ", small_width, small_height);

	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	    scanf("%f", &means[channel]);

	printf("(wavelet (means %f %f %f) (coeffs", means[0], means[1], means[2]);

	for (i = 0; i < NUM_COEFFS; ++i)
	{
	    int index;

	    scanf("%d", &index);
	    printf(" %d", index);
	}

	printf(")) (subpixel");

	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	{
	    static char *channel_names[NUM_CHANNELS] = { "y", "i", "q" };

	    printf(" (%s", channel_names[channel]);

	    for (i = 0; i < NUM_SUBPIXELS; ++i)
	    {
		int val;

		scanf("%d", &val);
		printf(" %d", val);
	    }

	    printf(")");
	}

	printf("))\n");
    } while (!feof(stdin));

    return 0;
}
