/*
 * error.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "error.h"

static error_handler_func_t error_handler = 0;

static int
error_kind (int error_code)
{
    static struct { int error_code; int kind; } kinds[] =
	{ { ERROR_WRONG_NUM_SUBPIXELS, ERROR_INFO_FILENAME },
	  { ERROR_TABLES_PARSE_ERROR, ERROR_INFO_FILENAME },
	  { ERROR_TABLES_SYNTAX_ERROR, ERROR_INFO_FILENAME },
	  { ERROR_TABLES_FILE_EXISTS, ERROR_INFO_FILENAME },
	  { ERROR_TABLES_FILE_CANNOT_CREATE, ERROR_INFO_FILENAME },
	  { ERROR_TABLES_FILE_CANNOT_OPEN, ERROR_INFO_FILENAME },
	  { ERROR_CANNOT_FIND_SMALL_IMAGE_NAME, ERROR_INFO_FILENAME },
	  { -1, -1 } };

    int i;

    for (i = 0; kinds[i].error_code >= 0; ++i)
	if (kinds[i].error_code == error_code)
	    return kinds[i].kind;

    assert(0);

    return -1;
}

char*
error_format_error (int error_code, error_info_t info)
{
    static struct { int error_code; const char *format; } formats[] =
	{ { ERROR_WRONG_NUM_SUBPIXELS, "Wrong number of subpixels in `%s'" },
	  { ERROR_TABLES_PARSE_ERROR, "Parse error in library `%s'" },
	  { ERROR_TABLES_SYNTAX_ERROR, "Syntax error in library `%s'" },
	  { ERROR_TABLES_FILE_EXISTS, "Tables file `%s' already exists" },
	  { ERROR_TABLES_FILE_CANNOT_CREATE, "Cannot create tables file `%s'" },
	  { ERROR_TABLES_FILE_CANNOT_OPEN, "Cannot open tables file `%s'" },
	  { ERROR_CANNOT_FIND_SMALL_IMAGE_NAME, "Cannot find name for small image `%s'" },
	  { -1, 0 } };

    int kind = error_kind(error_code);
    int i;

    for (i = 0; formats[i].error_code >= 0; ++i)
	if (formats[i].error_code == error_code)
	    switch (kind)
	    {
		case ERROR_INFO_NULL :
		    {
			char *copy = strdup(formats[i].format);

			assert(copy != 0);

			return copy;
		    }

		case ERROR_INFO_FILENAME :
		    {
			char *str = (char*)malloc(strlen(formats[i].format) + strlen(info.filename) + 1);

			assert(str != 0);

			sprintf(str, formats[i].format, info.filename);

			return str;
		    }

		default :
		    assert(0);
	    }

    assert(0);

    return 0;
}

void
error_set_handler (error_handler_func_t handler)
{
    error_handler = handler;
}

void
error_report (int code, error_info_t info)
{
    if (error_handler != 0)
    {
	error_handler(code, info);
	error_free_info(code, info);
    }
    else
    {
	char *str = error_format_error(code, info);

	assert(str != 0);

	fprintf(stderr, "Error: %s.\n", str);

	free(str);

	error_free_info(code, info);
    }
}

error_info_t
error_make_null_info (void)
{
    error_info_t info;

    return info;
}

error_info_t
error_make_filename_info (const char *filename)
{
    error_info_t info;

    info.filename = strdup(filename);
    assert(info.filename != 0);

    return info;
}

void
error_free_info (int error_code, error_info_t info)
{
    switch (error_kind(error_code))
    {
	case ERROR_INFO_NULL :
	    break;

	case ERROR_INFO_FILENAME :
	    free(info.filename);
	    break;

	default:
	    assert(0);
    }
}
