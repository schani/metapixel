#ifndef WINDOW_HDR
#define WINDOW_HDR

/* $Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/window.h,v 1.1 2000/01/04 03:35:21 schani Exp $ */

typedef struct {	/* WINDOW: A DISCRETE 2-D RECTANGLE */
    int x0, y0;		/* xmin and ymin */
    int x1, y1;		/* xmax and ymax (inclusive) */
} Window;

typedef struct {	/* WINDOW_BOX: A DISCRETE 2-D RECTANGLE */
    int x0, y0;		/* xmin and ymin */
    int x1, y1;		/* xmax and ymax (inclusive) */
    int nx, ny;		/* xsize=x1-x0+1 and ysize=y1-y0+1 */
} Window_box;

/*
 * note: because of the redundancy in the above structure, nx and ny should
 * be recomputed with window_box_set_size() when they cannot be trusted
 */

/* caution: we exploit positional coincidences in the following: */
#define window_box_overlap(a, b) \
    window_overlap((Window_box *)(a), (Window_box *)(b))

#endif
