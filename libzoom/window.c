#include <simple.h>
#include "window.h"

static char rcsid[] = "$Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/window.c,v 1.1 2000/01/04 03:35:21 schani Exp $";

window_set(x0, y0, x1, y1, a)
int x0, y0, x1, y1;
register Window *a;
{
    a->x0 = x0;
    a->y0 = y0;
    a->x1 = x1;
    a->y1 = y1;
}

window_clip(a, b)		/* a=intersect(a,b), return overlap bit */
register Window *a, *b;
{
    int overlap;

    overlap = window_overlap(a, b);
    window_intersect(a, b, a);
    return overlap;
}

window_intersect(a, b, c)	/* c = intersect(a,b) */
register Window *a, *b, *c;
{
    c->x0 = MAX(a->x0, b->x0);
    c->y0 = MAX(a->y0, b->y0);
    c->x1 = MIN(a->x1, b->x1);
    c->y1 = MIN(a->y1, b->y1);
}

window_overlap(a, b)
register Window *a, *b;
{
    return a->x0<=b->x1 && a->x1>=b->x0 && a->y0<=b->y1 && a->y1>=b->y0;
}

window_print(str, a)
char *str;
Window *a;
{
    printf("%s{%d,%d,%d,%d}%dx%d",
	str, a->x0, a->y0, a->x1, a->y1, a->x1-a->x0+1, a->y1-a->y0+1);
}

/*----------------------------------------------------------------------*/

window_box_intersect(a, b, c)
register Window_box *a, *b, *c;
{
    c->x0 = MAX(a->x0, b->x0);
    c->y0 = MAX(a->y0, b->y0);
    c->x1 = MIN(a->x1, b->x1);
    c->y1 = MIN(a->y1, b->y1);
    window_box_set_size(c);
}

window_box_print(str, a)
char *str;
Window_box *a;
{
    printf("%s{%d,%d,%d,%d}%dx%d",
	str, a->x0, a->y0, a->x1, a->y1, a->nx, a->ny);
}

window_box_set_max(a)
register Window_box *a;
{
    a->x1 = a->x0+a->nx-1;
    a->y1 = a->y0+a->ny-1;
}

window_box_set_size(a)
register Window_box *a;
{
    a->nx = a->x1-a->x0+1;
    a->ny = a->y1-a->y0+1;
}
