/*
 * lispreader.c
 *
 * Copyright (C) 1998-2004 Mark Probst
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#include <lispreader.h>

#define TOKEN_ERROR                   -1
#define TOKEN_EOF                     0
#define TOKEN_OPEN_PAREN              1
#define TOKEN_CLOSE_PAREN             2
#define TOKEN_SYMBOL                  3
#define TOKEN_STRING                  4
#define TOKEN_INTEGER                 5
#define TOKEN_REAL                    6
#define TOKEN_PATTERN_OPEN_PAREN      7
#define TOKEN_DOT                     8
#define TOKEN_TRUE                    9
#define TOKEN_FALSE                   10

#define MAX_TOKEN_LENGTH           8192

static char token_string[MAX_TOKEN_LENGTH + 1] = "";
static int token_length = 0;

static char *mmap_token_start, *mmap_token_stop;

static lisp_object_t end_marker = { LISP_TYPE_EOF };
static lisp_object_t error_object = { LISP_TYPE_PARSE_ERROR };
static lisp_object_t close_paren_marker = { LISP_TYPE_PARSE_ERROR };
static lisp_object_t dot_marker = { LISP_TYPE_PARSE_ERROR };

static void
_token_clear (void)
{
    token_string[0] = '\0';
    token_length = 0;
}

static void
_token_append (char c)
{
    assert(token_length < MAX_TOKEN_LENGTH);

    token_string[token_length++] = c;
    token_string[token_length] = '\0';
}

static void
copy_mmapped_token (void)
{
    token_length = mmap_token_stop - mmap_token_start;

    assert(token_length < MAX_TOKEN_LENGTH);

    memcpy(token_string, mmap_token_start, token_length);
    token_string[token_length] = '\0';
}

static int
_next_char (lisp_stream_t *stream)
{
    switch (stream->type)
    {
	case LISP_STREAM_MMAP_FILE :
	case LISP_STREAM_STRING :
	    assert(0);
	    return EOF;

	case LISP_STREAM_FILE :
	    return getc(stream->v.file);

        case LISP_STREAM_ANY:
	    return stream->v.any.next_char(stream->v.any.data);
    }
    assert(0);
    return EOF;
}

static void
_unget_char (char c, lisp_stream_t *stream)
{
    switch (stream->type)
    {
	case LISP_STREAM_MMAP_FILE :
	case LISP_STREAM_STRING :
	    assert(0);
	    break;

	case LISP_STREAM_FILE :
	    ungetc(c, stream->v.file);
	    break;

       case LISP_STREAM_ANY:
	    stream->v.any.unget_char(c, stream->v.any.data);
	    break;
	 
	default :
	    assert(0);
    }
}

static int
my_atoi (const char *start, const char *stop)
{
    int value = 0;

    while (start < stop)
    {
	value = value * 10 + (*start - '0');
	++start;
    }

    return value;
}

#define SCAN_FUNC_NAME _scan_mmap
#define SCAN_DECLS     char *pos = stream->v.mmap.pos, *end = stream->v.mmap.end;
#define NEXT_CHAR      (pos == end ? EOF : *pos++)
#define UNGET_CHAR(c)  (--pos)
#define TOKEN_START(o) (mmap_token_start = pos - (o))
#define TOKEN_APPEND(c)
#define TOKEN_STOP     (mmap_token_stop = pos)
#define RETURN(t)      ({ stream->v.mmap.pos = pos ; return (t); })

#include "lispscan.h"

#undef SCAN_FUNC_NAME
#undef SCAN_DECLS
#undef NEXT_CHAR
#undef UNGET_CHAR
#undef TOKEN_START
#undef TOKEN_APPEND
#undef TOKEN_STOP
#undef RETURN

#define SCAN_FUNC_NAME  _scan
#define SCAN_DECLS
#define NEXT_CHAR       _next_char(stream)
#define UNGET_CHAR(c)   _unget_char((c), stream)
#define TOKEN_START(o)  _token_clear()
#define TOKEN_APPEND(c) _token_append((c))
#define TOKEN_STOP
#define RETURN(t)       return (t)

#include "lispscan.h"

#undef SCAN_FUNC_NAME
#undef SCAN_DECLS
#undef NEXT_CHAR
#undef UNGET_CHAR
#undef TOKEN_START
#undef TOKEN_APPEND
#undef TOKEN_STOP
#undef RETURN

#define IS_STREAM_MMAPPED(s)   ((s)->type <= LISP_LAST_MMAPPED_STREAM)
#define SCAN(s)                (IS_STREAM_MMAPPED((s)) ? _scan_mmap((s)) : _scan((s)))

static lisp_object_t*
lisp_object_alloc (allocator_t *allocator, int type)
{
    lisp_object_t *obj = (lisp_object_t*)allocator_alloc(allocator, sizeof(lisp_object_t));

    obj->type = type;

    return obj;
}

lisp_stream_t*
lisp_stream_init_path (lisp_stream_t *stream, const char *path)
{
    int fd;
    struct stat sb;
    size_t len;
    void *buf;

    fd = open(path, O_RDONLY, 0);

    if (fd == -1)
	return 0;

    if (fstat(fd, &sb) == -1)
    {
	close(fd);
	return 0;
    }

    len = sb.st_size;

    buf = mmap(0, len, PROT_READ, MAP_SHARED, fd, 0);

    if (buf == (void*)-1)
    {
	FILE *file = fdopen(fd, "r");

	if (file == 0)
	{
	    close(fd);

	    return 0;
	}
	else
	    return lisp_stream_init_file(stream, file);
    }
    else
    {
	close(fd);

	stream->type = LISP_STREAM_MMAP_FILE;
	stream->v.mmap.buf = buf;
	stream->v.mmap.pos = buf;
	stream->v.mmap.end = buf + len;
    }

    return stream;
}

lisp_stream_t*
lisp_stream_init_file (lisp_stream_t *stream, FILE *file)
{
    stream->type = LISP_STREAM_FILE;
    stream->v.file = file;

    return stream;
}

lisp_stream_t*
lisp_stream_init_string (lisp_stream_t *stream, char *buf)
{
    stream->type = LISP_STREAM_STRING;
    stream->v.mmap.buf = buf;
    stream->v.mmap.end = buf + strlen(buf);
    stream->v.mmap.pos = buf;

    return stream;
}

lisp_stream_t* 
lisp_stream_init_any (lisp_stream_t *stream, void *data, 
		      int (*next_char) (void *data),
		      void (*unget_char) (char c, void *data))
{
    assert(next_char != 0 && unget_char != 0);
    
    stream->type = LISP_STREAM_ANY;
    stream->v.any.data = data;
    stream->v.any.next_char= next_char;
    stream->v.any.unget_char = unget_char;

    return stream;
}

void
lisp_stream_free_path  (lisp_stream_t *stream)
{
    assert(stream->type == LISP_STREAM_MMAP_FILE
	   || stream->type == LISP_STREAM_FILE);

    if (stream->type == LISP_STREAM_MMAP_FILE)
	munmap(stream->v.mmap.buf, stream->v.mmap.end - stream->v.mmap.buf);
    else
	fclose(stream->v.file);
}

lisp_object_t*
lisp_make_integer_with_allocator (allocator_t *allocator, int value)
{
    lisp_object_t *obj = lisp_object_alloc(allocator, LISP_TYPE_INTEGER);

    obj->v.integer = value;

    return obj;
}

lisp_object_t*
lisp_make_real_with_allocator (allocator_t *allocator, float value)
{
    lisp_object_t *obj = lisp_object_alloc(allocator, LISP_TYPE_REAL);

    obj->v.real = value;

    return obj;
}

static lisp_object_t*
lisp_make_symbol_with_allocator_internal (allocator_t *allocator, const char *str, size_t len)
{
    lisp_object_t *obj = lisp_object_alloc(allocator, LISP_TYPE_SYMBOL);

    obj->v.string = allocator_alloc(allocator, len + 1);
    memcpy(obj->v.string, str, len + 1);
    obj->v.string[len] = '\0';

    return obj;
}

lisp_object_t*
lisp_make_symbol_with_allocator (allocator_t *allocator, const char *value)
{
    return lisp_make_symbol_with_allocator_internal(allocator, value, strlen(value));
}

lisp_object_t*
lisp_make_string_with_allocator (allocator_t *allocator, const char *value)
{
    lisp_object_t *obj = lisp_object_alloc(allocator, LISP_TYPE_STRING);

    obj->v.string = allocator_strdup(allocator, value);

    return obj;
}

lisp_object_t*
lisp_make_cons_with_allocator (allocator_t *allocator, lisp_object_t *car, lisp_object_t *cdr)
{
    lisp_object_t *obj = lisp_object_alloc(allocator, LISP_TYPE_CONS);

    obj->v.cons.car = car;
    obj->v.cons.cdr = cdr;

    return obj;
}

lisp_object_t*
lisp_make_boolean_with_allocator (allocator_t *allocator, int value)
{
    lisp_object_t *obj = lisp_object_alloc(allocator, LISP_TYPE_BOOLEAN);

    obj->v.integer = value ? 1 : 0;

    return obj;
}

lisp_object_t*
lisp_make_integer (int value)
{
    return lisp_make_integer_with_allocator(&malloc_allocator, value);
}

lisp_object_t*
lisp_make_real (float value)
{
    return lisp_make_real_with_allocator(&malloc_allocator, value);
}

lisp_object_t*
lisp_make_symbol (const char *value)
{
    return lisp_make_symbol_with_allocator(&malloc_allocator, value);
}

lisp_object_t*
lisp_make_string (const char *value)
{
    return lisp_make_string_with_allocator(&malloc_allocator, value);
}

lisp_object_t*
lisp_make_cons (lisp_object_t *car, lisp_object_t *cdr)
{
    return lisp_make_cons_with_allocator(&malloc_allocator, car, cdr);
}

lisp_object_t*
lisp_make_boolean (int value)
{
    return lisp_make_boolean_with_allocator(&malloc_allocator, value);
}

static lisp_object_t*
lisp_make_pattern_cons_with_allocator (allocator_t *allocator, lisp_object_t *car, lisp_object_t *cdr)
{
    lisp_object_t *obj = lisp_object_alloc(allocator, LISP_TYPE_PATTERN_CONS);

    obj->v.cons.car = car;
    obj->v.cons.cdr = cdr;

    return obj;
}

static lisp_object_t*
lisp_make_pattern_var_with_allocator (allocator_t *allocator, int type, int index, lisp_object_t *sub)
{
    lisp_object_t *obj = lisp_object_alloc(allocator, LISP_TYPE_PATTERN_VAR);

    obj->v.pattern.type = type;
    obj->v.pattern.index = index;
    obj->v.pattern.sub = sub;

    return obj;
}

lisp_object_t*
lisp_read_with_allocator (allocator_t *allocator, lisp_stream_t *in)
{
    int token = SCAN(in);
    lisp_object_t *obj = lisp_nil();

    if (token == TOKEN_EOF)
	return &end_marker;

    switch (token)
    {
	case TOKEN_ERROR :
	    return &error_object;

	case TOKEN_EOF :
	    return &end_marker;

	case TOKEN_OPEN_PAREN :
	case TOKEN_PATTERN_OPEN_PAREN :
	    {
		lisp_object_t *last = lisp_nil(), *car;

		do
		{
		    car = lisp_read_with_allocator(allocator, in);
		    if (car == &error_object || car == &end_marker)
		    {
			lisp_free_with_allocator(allocator, obj);
			return &error_object;
		    }
		    else if (car == &dot_marker)
		    {
			if (lisp_nil_p(last))
			{
			    lisp_free_with_allocator(allocator, obj);
			    return &error_object;
			}

			car = lisp_read_with_allocator(allocator, in);
			if (car == &error_object || car == &end_marker)
			{
			    lisp_free_with_allocator(allocator, obj);
			    return car;
			}
			else
			{
			    last->v.cons.cdr = car;

			    if (SCAN(in) != TOKEN_CLOSE_PAREN)
			    {
				lisp_free_with_allocator(allocator, obj);
				return &error_object;
			    }

			    car = &close_paren_marker;
			}
		    }
		    else if (car != &close_paren_marker)
		    {
			if (lisp_nil_p(last))
			    obj = last = (token == TOKEN_OPEN_PAREN
					  ? lisp_make_cons_with_allocator(allocator, car, lisp_nil())
					  : lisp_make_pattern_cons_with_allocator(allocator, car, lisp_nil()));
			else
			    last = last->v.cons.cdr = lisp_make_cons_with_allocator(allocator, car, lisp_nil());
		    }
		} while (car != &close_paren_marker);
	    }
	    return obj;

	case TOKEN_CLOSE_PAREN :
	    return &close_paren_marker;

	case TOKEN_SYMBOL :
	    if (IS_STREAM_MMAPPED(in))
		return lisp_make_symbol_with_allocator_internal(allocator, mmap_token_start,
								mmap_token_stop - mmap_token_start);
	    else
		return lisp_make_symbol_with_allocator(allocator, token_string);

	case TOKEN_STRING :
	    return lisp_make_string_with_allocator(allocator, token_string);

	case TOKEN_INTEGER :
	    if (IS_STREAM_MMAPPED(in))
		return lisp_make_integer_with_allocator(allocator, my_atoi(mmap_token_start, mmap_token_stop));
	    else
		return lisp_make_integer_with_allocator(allocator, atoi(token_string));
	
        case TOKEN_REAL :
	    if (IS_STREAM_MMAPPED(in))
		copy_mmapped_token();
	    return lisp_make_real_with_allocator(allocator, (float)atof(token_string));

	case TOKEN_DOT :
	    return &dot_marker;

	case TOKEN_TRUE :
	    return lisp_make_boolean_with_allocator(allocator, 1);

	case TOKEN_FALSE :
	    return lisp_make_boolean_with_allocator(allocator, 0);
    }

    assert(0);
    return &error_object;
}

lisp_object_t*
lisp_read (lisp_stream_t *in)
{
    return lisp_read_with_allocator(&malloc_allocator, in);
}

void
lisp_free_with_allocator (allocator_t *allocator, lisp_object_t *obj)
{
 restart:

    if (obj == 0)
	return;

    switch (obj->type)
    {
	case LISP_TYPE_INTERNAL :
	case LISP_TYPE_PARSE_ERROR :
	case LISP_TYPE_EOF :
	    return;

	case LISP_TYPE_SYMBOL :
	case LISP_TYPE_STRING :
	    allocator_free(allocator, obj->v.string);
	    break;

	case LISP_TYPE_CONS :
	case LISP_TYPE_PATTERN_CONS :
	    /* If we just recursively free car and cdr we risk a stack
	       overflow because lists may be nested arbitrarily deep.

	       We can get rid of one recursive call with a tail call,
	       but there's still one remaining.

	       The solution is to flatten a recursive list until we
	       can free the car without recursion.  Then we free the
	       cdr with a tail call.

	       The transformation we perform on the list is this:

	         ((a . b) . c) -> (a . (b . c))
	    */
	    if (!lisp_nil_p(obj->v.cons.car)
		&& (lisp_type(obj->v.cons.car) == LISP_TYPE_CONS
		    || lisp_type(obj->v.cons.car) == LISP_TYPE_PATTERN_CONS))
	    {
		/* this is the transformation */

		lisp_object_t *car, *cdar;

		car = obj->v.cons.car;
		cdar = car->v.cons.cdr;

		car->v.cons.cdr = obj;

		obj->v.cons.car = cdar;

		obj = car;

		goto restart;
	    }
	    else
	    {
		/* here we just free the car (which is not recursive),
		   the cons itself and the cdr via a tail call.  */

		lisp_object_t *tmp;

		lisp_free_with_allocator(allocator, obj->v.cons.car);

		tmp = obj;
		obj = obj->v.cons.cdr;

		allocator_free(allocator, tmp);

		goto restart;
	    }

	case LISP_TYPE_PATTERN_VAR :
	    lisp_free_with_allocator(allocator, obj->v.pattern.sub);
	    break;
    }

    allocator_free(allocator, obj);
}

void
lisp_free (lisp_object_t *obj)
{
    lisp_free_with_allocator(&malloc_allocator, obj);
}

lisp_object_t*
lisp_read_from_string_with_allocator (allocator_t *allocator, const char *buf)
{
    lisp_stream_t stream;

    lisp_stream_init_string(&stream, (char*)buf);
    return lisp_read_with_allocator(allocator, &stream);
}

lisp_object_t*
lisp_read_from_string (const char *buf)
{
    return lisp_read_from_string_with_allocator(&malloc_allocator, buf);
}

static int
_compile_pattern (lisp_object_t **obj, int *index)
{
    if (*obj == 0)
	return 1;

    switch (lisp_type(*obj))
    {
	case LISP_TYPE_PATTERN_CONS :
	    {
		struct { char *name; int type; } types[] =
						 {
						     { "any", LISP_PATTERN_ANY },
						     { "symbol", LISP_PATTERN_SYMBOL },
						     { "string", LISP_PATTERN_STRING },
						     { "integer", LISP_PATTERN_INTEGER },
						     { "real", LISP_PATTERN_REAL },
						     { "boolean", LISP_PATTERN_BOOLEAN },
						     { "list", LISP_PATTERN_LIST },
						     { "or", LISP_PATTERN_OR },
						     { "number", LISP_PATTERN_NUMBER },
						     { 0, 0 }
						 };
		char *type_name;
		int type = 0;	/* makes gcc happy */
		int i;
		lisp_object_t *pattern;

		if (lisp_type(lisp_car(*obj)) != LISP_TYPE_SYMBOL)
		    return 0;

		type_name = lisp_symbol(lisp_car(*obj));
		for (i = 0; types[i].name != 0; ++i)
		{
		    if (strcmp(types[i].name, type_name) == 0)
		    {
			type = types[i].type;
			break;
		    }
		}

		if (types[i].name == 0)
		    return 0;

		if (type != LISP_PATTERN_OR && lisp_cdr(*obj) != 0)
		    return 0;

		pattern = lisp_make_pattern_var_with_allocator(&malloc_allocator, type, (*index)++, lisp_nil());

		if (type == LISP_PATTERN_OR)
		{
		    lisp_object_t *cdr = lisp_cdr(*obj);

		    if (!_compile_pattern(&cdr, index))
		    {
			lisp_free(pattern);
			return 0;
		    }

		    pattern->v.pattern.sub = cdr;

		    (*obj)->v.cons.cdr = lisp_nil();
		}

		lisp_free(*obj);

		*obj = pattern;
	    }
	    break;

	case LISP_TYPE_CONS :
	    if (!_compile_pattern(&(*obj)->v.cons.car, index))
		return 0;
	    if (!_compile_pattern(&(*obj)->v.cons.cdr, index))
		return 0;
	    break;
    }

    return 1;
}

int
lisp_compile_pattern (lisp_object_t **obj, int *num_subs)
{
    int index = 0;
    int result;

    result = _compile_pattern(obj, &index);

    if (result && num_subs != 0)
	*num_subs = index;

    return result;
}

static int _match_pattern (lisp_object_t *pattern, lisp_object_t *obj, lisp_object_t **vars);

static int
_match_pattern_var (lisp_object_t *pattern, lisp_object_t *obj, lisp_object_t **vars)
{
    assert(lisp_type(pattern) == LISP_TYPE_PATTERN_VAR);

    switch (pattern->v.pattern.type)
    {
	case LISP_PATTERN_ANY :
	    break;

	case LISP_PATTERN_SYMBOL :
	    if (obj == 0 || lisp_type(obj) != LISP_TYPE_SYMBOL)
		return 0;
	    break;

	case LISP_PATTERN_STRING :
	    if (obj == 0 || lisp_type(obj) != LISP_TYPE_STRING)
		return 0;
	    break;

	case LISP_PATTERN_INTEGER :
	    if (obj == 0 || lisp_type(obj) != LISP_TYPE_INTEGER)
		return 0;
	    break;

        case LISP_PATTERN_REAL :
	    if (obj == 0 || lisp_type(obj) != LISP_TYPE_REAL)
		return 0;
	    break;
	  
	case LISP_PATTERN_BOOLEAN :
	    if (obj == 0 || lisp_type(obj) != LISP_TYPE_BOOLEAN)
		return 0;
	    break;

	case LISP_PATTERN_LIST :
	    if (obj == 0 || lisp_type(obj) != LISP_TYPE_CONS)
		return 0;
	    break;

	case LISP_PATTERN_OR :
	    {
		lisp_object_t *sub;
		int matched = 0;

		for (sub = pattern->v.pattern.sub; sub != 0; sub = lisp_cdr(sub))
		{
		    assert(lisp_type(sub) == LISP_TYPE_CONS);

		    if (_match_pattern(lisp_car(sub), obj, vars))
			matched = 1;
		}

		if (!matched)
		    return 0;
	    }
	    break;

	case LISP_PATTERN_NUMBER :
	    if (obj == 0 || (lisp_type(obj) != LISP_TYPE_INTEGER
			     && lisp_type(obj) != LISP_TYPE_REAL))
		return 0;
	    break;

	default :
	    assert(0);
    }

    if (vars != 0)
	vars[pattern->v.pattern.index] = obj;

    return 1;
}

static int
_match_pattern (lisp_object_t *pattern, lisp_object_t *obj, lisp_object_t **vars)
{
    if (pattern == 0)
	return obj == 0;

    if (obj == 0)
	return 0;

    if (lisp_type(pattern) == LISP_TYPE_PATTERN_VAR)
	return _match_pattern_var(pattern, obj, vars);

    if (lisp_type(pattern) != lisp_type(obj))
	return 0;

    switch (lisp_type(pattern))
    {
	case LISP_TYPE_SYMBOL :
	    return strcmp(lisp_symbol(pattern), lisp_symbol(obj)) == 0;

	case LISP_TYPE_STRING :
	    return strcmp(lisp_string(pattern), lisp_string(obj)) == 0;

	case LISP_TYPE_INTEGER :
	    return lisp_integer(pattern) == lisp_integer(obj);

        case LISP_TYPE_REAL :
            return lisp_real(pattern) == lisp_real(obj);

	case LISP_TYPE_CONS :
	    {
		int result1, result2;

		result1 = _match_pattern(lisp_car(pattern), lisp_car(obj), vars);
		result2 = _match_pattern(lisp_cdr(pattern), lisp_cdr(obj), vars);

		return result1 && result2;
	    }
	    break;

	default :
	    assert(0);
    }

    return 0;
}

int
lisp_match_pattern (lisp_object_t *pattern, lisp_object_t *obj, lisp_object_t **vars, int num_subs)
{
    int i;

    if (vars != 0)
	for (i = 0; i < num_subs; ++i)
	    vars[i] = &error_object;

    return _match_pattern(pattern, obj, vars);
}

int
lisp_match_string (const char *pattern_string, lisp_object_t *obj, lisp_object_t **vars)
{
    lisp_object_t *pattern;
    int result;
    int num_subs;

    pattern = lisp_read_from_string(pattern_string);

    if (pattern != 0 && (lisp_type(pattern) == LISP_TYPE_EOF
			 || lisp_type(pattern) == LISP_TYPE_PARSE_ERROR))
	return 0;

    if (!lisp_compile_pattern(&pattern, &num_subs))
    {
	lisp_free(pattern);
	return 0;
    }

    result = lisp_match_pattern(pattern, obj, vars, num_subs);

    lisp_free(pattern);

    return result;
}

int
lisp_type (lisp_object_t *obj)
{
    if (obj == 0)
	return LISP_TYPE_NIL;
    return obj->type;
}

int
lisp_integer (lisp_object_t *obj)
{
    assert(obj->type == LISP_TYPE_INTEGER);

    return obj->v.integer;
}

char*
lisp_symbol (lisp_object_t *obj)
{
    assert(obj->type == LISP_TYPE_SYMBOL);

    return obj->v.string;
}

char*
lisp_string (lisp_object_t *obj)
{
    assert(obj->type == LISP_TYPE_STRING);

    return obj->v.string;
}

int
lisp_boolean (lisp_object_t *obj)
{
    assert(obj->type == LISP_TYPE_BOOLEAN);

    return obj->v.integer;
}

float
lisp_real (lisp_object_t *obj)
{
    assert(obj->type == LISP_TYPE_REAL || obj->type == LISP_TYPE_INTEGER);

    if (obj->type == LISP_TYPE_INTEGER)
	return obj->v.integer;
    return obj->v.real;
}
	   
lisp_object_t*
lisp_car (lisp_object_t *obj)
{
    assert(obj->type == LISP_TYPE_CONS || obj->type == LISP_TYPE_PATTERN_CONS);

    return obj->v.cons.car;
}

lisp_object_t*
lisp_cdr (lisp_object_t *obj)
{
    assert(obj->type == LISP_TYPE_CONS || obj->type == LISP_TYPE_PATTERN_CONS);

    return obj->v.cons.cdr;
}

lisp_object_t*
lisp_cxr (lisp_object_t *obj, const char *x)
{
    int i;

    for (i = strlen(x) - 1; i >= 0; --i)
	if (x[i] == 'a')
	    obj = lisp_car(obj);
	else if (x[i] == 'd')
	    obj = lisp_cdr(obj);
	else
	    assert(0);

    return obj;
}

int
lisp_list_length (lisp_object_t *obj)
{
    int length = 0;

    while (obj != 0)
    {
	assert(obj->type == LISP_TYPE_CONS || obj->type == LISP_TYPE_PATTERN_CONS);

	++length;
	obj = obj->v.cons.cdr;
    }

    return length;
}

lisp_object_t*
lisp_list_nth_cdr (lisp_object_t *obj, int index)
{
    while (index > 0)
    {
	assert(obj != 0);
	assert(obj->type == LISP_TYPE_CONS || obj->type == LISP_TYPE_PATTERN_CONS);

	--index;
	obj = obj->v.cons.cdr;
    }

    return obj;
}

lisp_object_t*
lisp_list_nth (lisp_object_t *obj, int index)
{
    obj = lisp_list_nth_cdr(obj, index);

    assert(obj != 0);

    return obj->v.cons.car;
}

void
lisp_dump (lisp_object_t *obj, FILE *out)
{
    if (obj == 0)
    {
	fprintf(out, "()");
	return;
    }

    switch (lisp_type(obj))
    {
	case LISP_TYPE_EOF :
	    fputs("#<eof>", out);
	    break;

	case LISP_TYPE_PARSE_ERROR :
	    fputs("#<error>", out);
	    break;

	case LISP_TYPE_INTEGER :
	    fprintf(out, "%d", lisp_integer(obj));
	    break;

        case LISP_TYPE_REAL :
	    fprintf(out, "%f", lisp_real(obj));
	    break;

	case LISP_TYPE_SYMBOL :
	    fputs(lisp_symbol(obj), out);
	    break;

	case LISP_TYPE_STRING :
	    {
		char *p;

		fputc('"', out);
		for (p = lisp_string(obj); *p != 0; ++p)
		{
		    if (*p == '"' || *p == '\\')
			fputc('\\', out);
		    fputc(*p, out);
		}
		fputc('"', out);
	    }
	    break;

	case LISP_TYPE_CONS :
	case LISP_TYPE_PATTERN_CONS :
	    fputs(lisp_type(obj) == LISP_TYPE_CONS ? "(" : "#?(", out);
	    while (obj != 0)
	    {
		lisp_dump(lisp_car(obj), out);
		obj = lisp_cdr(obj);
		if (obj != 0)
		{
		    if (lisp_type(obj) != LISP_TYPE_CONS
			&& lisp_type(obj) != LISP_TYPE_PATTERN_CONS)
		    {
			fputs(" . ", out);
			lisp_dump(obj, out);
			break;
		    }
		    else
			fputc(' ', out);
		}
	    }
	    fputc(')', out);
	    break;

	case LISP_TYPE_BOOLEAN :
	    if (lisp_boolean(obj))
		fputs("#t", out);
	    else
		fputs("#f", out);
	    break;

	default :
	    assert(0);
    }
}
