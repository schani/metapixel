#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <values.h>
#include <math.h>

#define str_eq(a, b)    (strcmp(a, b) == 0)
#define MIN(a, b)       ((a)<(b) ? (a) : (b))
#define MAX(a, b)       ((a)>(b) ? (a) : (b))
#define SWAP(a, b, t)   {t = a; a = b; b = t;}

#define ALLOC(ptr, type, n)  assert(ptr = (type *)malloc((n)*sizeof(type)))
#define ALLOC_ZERO(ptr, type, n)  assert(ptr = (type *)calloc(n, sizeof(type)))

#define PI M_PI
