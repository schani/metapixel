/*
 * pic.h: definitions for device-independent picture package
 *
 * Paul Heckbert, ph@cs.cmu.edu	Sept 1988
 *
 * Copyright (c) 1989  Paul S. Heckbert
 * This source may be used for peaceful, nonprofit purposes only, unless
 * under licence from the author. This notice should remain in the source.
 */

#ifndef PIC_HDR
#define PIC_HDR

/* $Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/pic.h,v 1.1 2000/01/04 03:35:21 schani Exp $ */
#include "pixel.h"
#include "window.h"

typedef struct {		/* PICTURE PROCEDURE POINTERS */
    char *(*open)(/* name, mode */);
    void (*close)(/* p */);

    char *(*get_name)(/* p */);
    void (*clear)(/* p, pv */);
    void (*clear_rgba)(/* p, r, g, b, a */);

    void (*set_nchan)(/* p, nchan */);
    void (*set_box)(/* p, ox, oy, dx, dy */);
    void (*write_pixel)(/* p, x, y, v */);
    void (*write_pixel_rgba)(/* p, x, y, r, g, b, a */);
    void (*write_row)(/* p, y, x0, nx, buf */);
    void (*write_row_rgba)(/* p, y, x0, nx, buf */);

    int (*get_nchan)(/* p */);
    void (*get_box)(/* p, ox, oy, dx, dy */);
    Pixel1 (*read_pixel)(/* p, x, y */);
    void (*read_pixel_rgba)(/* p, x, y, pv */);
    void (*read_row)(/* p, y, x0, nx, buf */);
    void (*read_row_rgba)(/* p, y, x0, nx, buf */);
} Pic_procs;

typedef struct {	/* PICTURE INFO */
    char *dev;		/* device/filetype name */
    Pic_procs *procs;	/* structure of generic procedure pointers */
    char *data;		/* device-dependent data (usually ptr to structure) */
} Pic;

#define PIC_LISTMAX 20
extern Pic *pic_list[PIC_LISTMAX];	/* list of known picture devices */
extern int pic_npic;			/* #pics in pic_list, set by pic_init */

#define PIC_UNDEFINED PIXEL_UNDEFINED	/* used for unknown nchan */

Pic	*pic_open(/* name, mode */);
Pic	*pic_open_dev(/* dev, name, mode */);
void	pic_close(/* p */);

#define     pic_get_name(p) \
    (*(p)->procs->get_name)((p)->data)
#define     pic_clear(p, pv) \
    (*(p)->procs->clear)((p)->data, pv)
#define     pic_clear_rgba(p, r, g, b, a) \
    (*(p)->procs->clear_rgba)((p)->data, r, g, b, a)

#define     pic_set_nchan(p, nchan) \
    (*(p)->procs->set_nchan)((p)->data, nchan)
#define     pic_set_box(p, ox, oy, dx, dy) \
    (*(p)->procs->set_box)((p)->data, ox, oy, dx, dy)

#define     pic_write_pixel(p, x, y, pv) \
    (*(p)->procs->write_pixel)((p)->data, x, y, pv)
#define     pic_write_pixel_rgba(p, x, y, r, g, b, a) \
    (*(p)->procs->write_pixel_rgba)((p)->data, x, y, r, g, b, a)
#define     pic_write_row(p, y, x0, nx, buf) \
    (*(p)->procs->write_row)((p)->data, y, x0, nx, buf)
#define     pic_write_row_rgba(p, y, x0, nx, buf) \
    (*(p)->procs->write_row_rgba)((p)->data, y, x0, nx, buf)

#define     pic_get_nchan(p) \
    (*(p)->procs->get_nchan)((p)->data)
#define     pic_get_box(p, ox, oy, dx, dy) \
    (*(p)->procs->get_box)((p)->data, ox, oy, dx, dy)

#define     pic_read_pixel(p, x, y) \
    (*(p)->procs->read_pixel)((p)->data, x, y)
#define     pic_read_pixel_rgba(p, x, y, pv) \
    (*(p)->procs->read_pixel_rgba)((p)->data, x, y, pv)
#define     pic_read_row(p, y, x0, nx, buf) \
    (*(p)->procs->read_row)((p)->data, y, x0, nx, buf)
#define     pic_read_row_rgba(p, y, x0, nx, buf) \
    (*(p)->procs->read_row_rgba)((p)->data, y, x0, nx, buf)


void	pic_init(/* p */);
void	pic_catalog();
#define pic_get_dev(p) (p)->dev
Pic	*pic_load(/* name1, name2 */);
void	pic_save(/* p, name */);
void	pic_copy(/* p, q */);
void	pic_set_window(/* p, win */);
void	pic_write_block(/* p, x0, y0, nx, ny, buf */);
void	pic_write_block_rgba(/* p, x0, y0, nx, ny, buf */);
Window	*pic_get_window(/* p, win */);
void	pic_read_block(/* p, x0, y0, nx, ny, buf */);
void	pic_read_block_rgba(/* p, x0, y0, nx, ny, buf */);

char	*pic_file_dev(/* file */);

#endif
