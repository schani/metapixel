/* $Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/zoom.h,v 1.1 2000/01/04 03:35:21 schani Exp $ */

#include "pic.h"
#include "window.h"
#include "filt.h"

typedef struct {	/* SOURCE TO DEST COORDINATE MAPPING */
    double sx, sy;	/* x and y scales */
    double tx, ty;	/* x and y translations */
    double ux, uy;	/* x and y offset used by MAP, private fields */
} Mapping;

/* see explanation in zoom.c */

extern int zoom_debug;
extern int zoom_coerce;	/* simplify filters if possible? */
extern int zoom_xy;	/* filter x before y (1) or vice versa (0)? */
extern int zoom_trimzeros;	/* trim zeros from filter weight table? */

void zoom (Pic *apic, Window_box *a, Pic *bpic, Window_box *b, Filt *xfilt, Filt *yfilt);
