/*
 * error.h
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

#ifndef __METAPIXEL_ERROR_H__
#define __METAPIXEL_ERROR_H__

#define ERROR_WRONG_NUM_SUBPIXELS                 1
#define ERROR_TABLES_PARSE_ERROR                  2
#define ERROR_TABLES_SYNTAX_ERROR                 3
#define ERROR_TABLES_FILE_EXISTS                  4
#define ERROR_TABLES_FILE_CANNOT_CREATE           5
#define ERROR_TABLES_FILE_CANNOT_OPEN             6
#define ERROR_CANNOT_FIND_METAPIXEL_IMAGE_NAME    7
#define ERROR_CANNOT_READ_METAPIXEL_IMAGE         8
#define ERROR_CANNOT_READ_INPUT_IMAGE             9
#define ERROR_CANNOT_WRITE_OUTPUT_IMAGE          10
#define ERROR_CANNOT_FIND_LOCAL_MATCH            11
#define ERROR_NOT_ENOUGH_GLOBAL_METAPIXELS       12
#define ERROR_CANNOT_FIND_COLLAGE_MATCH          13
#define ERROR_IMAGE_TOO_SMALL                    14

#define ERROR_INFO_NULL             0
#define ERROR_INFO_FILENAME         1

typedef union
{
    char *filename;
} error_info_t;

typedef void (*error_handler_func_t) (int error_code, error_info_t info);

/* If the error handler does a nonlocal exit (i.e., a longjmp or an
   exception throw), it is the error handler's responsibility to free
   the error info via error_free_info. */
void error_set_handler (error_handler_func_t handler);

void error_free_info (int error_code, error_info_t info);

/* The returned string must be freed by the caller. */
char* error_format_error (int error_code, error_info_t info);

/* Only used by the API: */
error_info_t error_make_null_info (void);
error_info_t error_make_filename_info (const char *filename);

void error_report (int code, error_info_t info);

#endif
