/* pic: device-independent picture package */

static char rcsid[] = "$Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/pic.c,v 1.1 2000/01/04 03:35:21 schani Exp $";

#include <simple.h>
#include "pic.h"

void pic_close(p)
Pic *p;
{
    (*p->procs->close)(p->data);
    free(p);
}

void pic_copy(p, q)
register Pic *p, *q;
{
    int nchan, dx, y;
    Window w;
    Pixel1 *buf;
    Pixel1_rgba *buf4;

    nchan = pic_get_nchan(p);
    pic_set_nchan(q, nchan);
    pic_set_window(q, pic_get_window(p, &w));
    dx = w.x1-w.x0+1;
    switch (nchan) {
	case 1:
	    ALLOC(buf, Pixel1, dx);
	    break;
	case 3:
	case 4:
	    ALLOC(buf4, Pixel1_rgba, dx);
	    break;
	default:
	    fprintf(stderr, "pic_copy: can't handle nchan=%d\n", nchan);
	    return;
    }
    for (y=w.y0; y<=w.y1; y++)
	switch (nchan) {
	    case 1:
		pic_read_row(p, y, w.x0, dx, buf);
		pic_write_row(q, y, w.x0, dx, buf);
		break;
	    case 3:
	    case 4:
		pic_read_row_rgba(p, y, w.x0, dx, buf4);
		pic_write_row_rgba(q, y, w.x0, dx, buf4);
		break;
	}
    if (nchan==1) free(buf); else free(buf4);
}

void pic_set_window(p, win)
Pic *p;
Window *win;
{
    pic_set_box(p, win->x0, win->y0, win->x1-win->x0+1, win->y1-win->y0+1);
}

void pic_write_block(p, x0, y0, nx, ny, buf)
Pic *p;
int x0, y0, nx, ny;
Pixel1 *buf;
{
    int y;

    for (y=0; y<ny; y++, buf+=nx)
	pic_write_row(p, y0+y, x0, nx, buf);
}

void pic_write_block_rgba(p, x0, y0, nx, ny, buf)
Pic *p;
int x0, y0, nx, ny;
Pixel1_rgba *buf;
{
    int y;

    for (y=0; y<ny; y++, buf+=nx)
	pic_write_row_rgba(p, y0+y, x0, nx, buf);
}

Window *pic_get_window(p, win)
Pic *p;
Window *win;
{
    int dx, dy;

    if (!win) ALLOC(win, Window, 1);
    pic_get_box(p, &win->x0, &win->y0, &dx, &dy);
    win->x1 = win->x0+dx-1;
    win->y1 = win->y0+dy-1;
    return win;
}

void pic_read_block(p, x0, y0, nx, ny, buf)
Pic *p;
int x0, y0, nx, ny;
Pixel1 *buf;
{
    int y;

    for (y=0; y<ny; y++, buf+=nx)
	pic_read_row(p, y0+y, x0, nx, buf);
}

void pic_read_block_rgba(p, x0, y0, nx, ny, buf)
Pic *p;
int x0, y0, nx, ny;
Pixel1_rgba *buf;
{
    int y;

    for (y=0; y<ny; y++, buf+=nx)
	pic_read_row_rgba(p, y0+y, x0, nx, buf);
}
