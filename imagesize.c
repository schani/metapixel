/* -*- c -*- */

/*
 * imagesize.c
 *
 * metapixel
 *
 * Copyright (C) 2003 Mark Probst
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
#include <assert.h>

#include "readimage.h"

int
main (int argc, char *argv[])
{
    image_reader_t *reader;

    assert(argc == 2);

    reader = open_image_reading(argv[1]);

    if (reader == 0)
	printf("0 0\n");
    else
	printf("%d %d\n", reader->width, reader->height);

    return 0;
}
