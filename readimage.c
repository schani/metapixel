/* -*- c -*- */

/*
 * readimage.c
 *
 * metapixel
 *
 * Copyright (C) 2000 Mark Probst
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
#include <string.h>

#include "rwjpeg.h"
#include "rwpng.h"

unsigned char*
read_image (char *filename, int *width, int *height)
{
    unsigned char magic[4];
    FILE *file;

    file = fopen(filename, "r");
    if (file == 0)
	return 0;

    if (fread(magic, 4, 1, file) != 1)
    {
	fclose(file);
	return 0;
    }

    fclose(file);

    if (memcmp(magic, "\xff\xd8", 2) == 0)
	return read_jpeg_file(filename, width, height);
    else if (memcmp(magic, "\x89PNG", 4) == 0)
	return read_png_file(filename, width, height);
    return 0;
}
