/*
 * color.c
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

#include <assert.h>
#include <string.h>

#include "api.h"

void
color_rgb_to_yiq (unsigned char *yiq, unsigned char *rgb)
{
    float rgbf[3];
    float yiqf[3];

    rgbf[0] = (float)rgb[0];
    rgbf[1] = (float)rgb[1];
    rgbf[2] = (float)rgb[2];

    yiqf[0] = rgbf[0] * 0.299 + rgbf[1] * 0.596 + rgbf[2] * 0.212;
    yiqf[1] = rgbf[0] * 0.587 + rgbf[1] * -0.275 + rgbf[2] * -0.528;
    yiqf[2] = rgbf[0] * 0.114 + rgbf[1] * -0.321 + rgbf[2] * 0.311;

    yiq[0] = (unsigned char)yiqf[0];
    yiq[1] = (unsigned char)MAX(0.0, MIN(yiqf[1], 255.0));
    yiq[2] = (unsigned char)MAX(0.0, MIN(yiqf[2], 255.0));
}

void
color_rgb_to_hsv (unsigned char *_hsv, unsigned char *_rgb)
{
    float hsv[3], rgb[3];
    float max, min;
    int i;

    for (i = 0; i < 3; ++i)
	rgb[i] = (float)_rgb[i] / 255.0;

    max = MAX(rgb[0], MAX(rgb[1], rgb[2]));
    min = MIN(rgb[0], MIN(rgb[1], rgb[2]));
    hsv[2] = max;

    if (max != 0)
	hsv[1] = (max - min) / max;
    else
	hsv[1] = 0.0;

    if (hsv[1] == 0.0)
	hsv[0] = 0.0;		/* actually undefined */
    else
    {
	float delta = max - min;

	if (rgb[0] == max)
	    hsv[0] = (rgb[1] - rgb[2]) / delta;
	else if (rgb[1] == max)
	    hsv[0] = 2 + (rgb[2] - rgb[0]) / delta;
	else
	    hsv[0] = 4 + (rgb[0] - rgb[1]) / delta;

	hsv[0] /= 6.0;

	if (hsv[0] < 0.0)
	    hsv[0] += 1.0;
    }

    for (i = 0; i < 3; ++i)
	_hsv[i] = hsv[i] * 255.0;
}

void
color_convert_rgb_pixels (unsigned char *dst, unsigned char *rgb, unsigned int num_pixels, int color_space)
{
    unsigned int i;

    switch (color_space)
    {
	case COLOR_SPACE_RGB :
	    memcpy(dst, rgb, num_pixels * 3);
	    break;

	case COLOR_SPACE_HSV :
	    for (i = 0; i < num_pixels; ++i)
	    {
		color_rgb_to_hsv(dst, rgb);
		dst += 3;
		rgb += 3;
	    }
	    break;

	case COLOR_SPACE_YIQ :
	    for (i = 0; i < num_pixels; ++i)
	    {
		color_rgb_to_yiq(dst, rgb);
		dst += 3;
		rgb += 3;
	    }
	    break;

	default :
	    assert(0);
    }
}
