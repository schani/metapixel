/*
 * zoom: subroutines for in-place one-pass filtered image zooming
 *
 * Zooms (resizes) one image into another with independent control of
 * scale, translation, and filtering in x and y.  Magnifies or minifies.
 * Filters with an arbitrary separable finite impulse response filter.
 * (A filter h(x,y) is called separable if h(x,y)=f(x)*g(y)).
 * See the filt package regarding filtering.
 * The code is graphics device independent, using only generic scanline
 * i/o operations.
 *
 * The program makes one pass over the image using a moving window of
 * scanlines, so memory usage is proportional to imagewidth*filterheight,
 * not imagewidth*imageheight, as you'd get if the entire image were
 * buffered.  The separability of the filter is also exploited, to get
 * a cpu time proportional to npixels*(filterwidth+filterheight),
 * not npixels*filterwidth*filterheight as with direct 2-D convolution.
 *
 * The source and destination images can be identical, with overlapping
 * windows, in which case the algorithm finds the magic split point and
 * uses the appropriate scanning directions to avoid feedback (recursion)
 * in the filtering.
 *
 * Paul Heckbert	12 Sept 1988
 * ph@cs.cmu.edu
 *
 * Copyright (c) 1989  Paul S. Heckbert
 * This source may be used for peaceful, nonprofit purposes only, unless
 * under licence from the author. This notice should remain in the source.
 */

/*
 * routines in this source file:
 *
 * PUBLIC:
 * zoom			most common entry point; window-to-window zoom
 * zoom_continuous	fancier: explicit, continuous scale&tran control
 *
 * SORTA PRIVATE:
 * zoom_unfiltered_mono	point-sampled zooming, called by zoom_continuous
 * zoom_unfiltered_rgb	point-sampled zooming, called by zoom_continuous
 * zoom_filtered	filtered zooming, called by zoom_continuous
 * zoom_filtered_xy	zoom, filtering x before y, called by zoom_filtered
 * zoom_filtered_yx	zoom, filtering y before x, called by zoom_filtered
 *
 * make_weighttab	make lookup table of filter coefficients
 * make_map_table	in-place tricks; order scanlines to avoid feedback
 */

static char rcsid[] = "$Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/zoom.c,v 1.1 2000/01/04 03:35:21 schani Exp $";

#include <math.h>

#include "simple.h"
#include "pic.h"
#include "filt.h"
#include "scanline.h"
#include "zoom.h"

#define EPSILON 1e-7			/* error tolerance */
#define UNDEF PIC_UNDEFINED

#define WEIGHTBITS  14			/* # bits in filter coefficients */
#define FINALSHIFT  (2*WEIGHTBITS-CHANBITS) /* shift after x&y filter passes */
#define WEIGHTONE  (1<<WEIGHTBITS)	/* filter weight of one */

#define INTEGER(x) (fabs((x)-floor((x)+.5)) < EPSILON)	/* is x an integer? */
#define FRAC(x) fabs((x)-floor((x)+.5))		/* diff from closest integer */

typedef struct {	/* ZOOM-SPECIFIC FILTER PARAMETERS */
    double scale;	/* filter scale (spacing between centers in a space) */
    double supp;	/* scaled filter support radius */
    int wid;		/* filter width: max number of nonzero samples */
} Filtpar;

int zoom_debug = 0;	/* debug level: 0=none, 1=scanline i/o, 2=filters */
int zoom_coerce = 1;	/* simplify filters if possible? */
int zoom_xy = UNDEF;	/* filter x before y (1) or vice versa (0)? */
int zoom_trimzeros = 1;	/* trim zeros in x filter weight tables? */

static int nzero = 0, ntot = 0, nzmax = 0;

static Filt *simplify_filter();

/*----------------------------------------------------------------------
 * The mapping from source coordinates to dest coordinates:
 * (Notation: prefix 'a' denotes source coords, 'b' denotes destination coords)
 *
 *	bx = sx*ax+tx
 *	by = sy*ay+ty
 *
 * where (ax,ay) and (bx,by) are DISCRETE source and dest coords, respectively.
 *
 * An important fine point on terminology: there are two kinds of pixel coords:
 *	DISCRETE COORDINATES take on integer values at pixel centers
 *	CONTINUOUS COORDINATES take on integer values halfway between pixels
 * For example, an image with discrete coordinate range [0..511] has a
 * continuous coordinate range of [0..512].  The pixel with
 * discrete coords (x,y) has its center at continuous coords (x+.5,y+.5)
 * and its continuous coord domain is [x..x+1] in X and [y..y+1] in Y.
 * Note: discrete coords are not always stored in ints and continuous coords
 * are not always stored in floats.
 *
 * conversion:
 * if c = continuous coord and d = discrete coord, then
 *	c = d+.5
 *	d = floor(c)
 *
 * To map a discrete src interval [a0..a1] to a discrete dst interval [b0..b1]:
 *
 *	b-b0+.5 = bn/an*(a-a0+.5)
 *  or	b = (bn/an)*a + (b0-.5)-(bn/an)*(a0-.5)
 *	  = scale*a+tran
 *
 * where a and b are the discrete source and dest coords (either x or y)
 * and an and bn are the interval lengths: an=a1-a0+1, bn=b1-b0+1.
 * a0, an, b0, bn are the mapping parameters used by the zoom routine below.
 * In general, however, for sub-pixel scale and translate control, we allow
 * any real numbers for scale and tran (although there should usually be some
 * correspondence between the mapping function and the src and dst windows).
 *
 * We'll often want the inverse mapping, from discrete dst coord b to
 * continuous src coord ac, relative to the interval origins a0 and b0:
 *
 *	b+b0 = s*(ac+a0-.5)+t
 *  so	ac = (b+b0-s*(a0-.5)-t)/s
 *	   = (b+offset)/scale = MAP(b, scale, offset)
 *
 * The mapping fields ux and uy in the Mapping structure
 * are these offsets in x & y respectively.
 */

/* the mapping from discrete dest coord b to continuous source coord: */
#define MAP(b, scale, offset)  ((b)+(offset))/(scale)

/*----------------------------------------------------------------------
 * zoom: simplest entry point; window-to-window zoom.
 * Zoom window a of apic to window b of bpic using the specified filters.
 * Window a represents the pixels with x in [a->x0..a->x1]
 * and y in [a->y0..a->y1], (ranges are inclusive).  Likewise for b.
 * apic and bpic may be equal and a and b may overlap, no sweat.
 */

void
zoom(apic, a, bpic, b, xfilt, yfilt)
Pic *apic, *bpic;	/* source and dest pictures */
Window_box *a, *b;	/* source and dest windows */
Filt *xfilt, *yfilt;	/* filters for x and y */
{
    Mapping map;

    if (b->x0==UNDEF) {
	printf("zoom: undefined window for dest %s: ",
	    pic_get_name(bpic));
	window_print("", &b);
	printf("\n");
	exit(1);
    }
    window_box_set_size(a);
    window_box_set_size(b);
    if (a->nx<=0 || a->ny<=0) return;
    map.sx = (double)b->nx/a->nx;
    map.sy = (double)b->ny/a->ny;
    map.tx = b->x0-.5 - map.sx*(a->x0-.5);
    map.ty = b->y0-.5 - map.sy*(a->y0-.5);
    zoom_continuous(apic, a, bpic, b, &map, xfilt, yfilt);
}

/*
 * zoom_opt: window-to-window zoom with options.
 * Like zoom() but has options to make mapping square (preserve pixel shape)
 * and round to the next lowest integer scale factor.
 */

zoom_opt(apic, a, bpic, b, xfilt, yfilt, square, intscale)
Pic *apic, *bpic;	/* source and dest pictures */
Window_box *a, *b;	/* source and dest windows */
Filt *xfilt, *yfilt;	/* filters for x and y */
int square, intscale;	/* square mapping? integer scales? */
{
    Mapping map;

    if (b->x0==UNDEF) {
	printf("zoom_opt: undefined window for dest %s: ",
	    pic_get_name(bpic));
	window_print("", &b);
	printf("\n");
	exit(1);
    }
    window_box_set_size(a);
    window_box_set_size(b);
    if (a->nx<=0 || a->ny<=0) return;
    map.sx = (double)b->nx/a->nx;
    map.sy = (double)b->ny/a->ny;
    if (square) {
	if (map.sx>map.sy) {	/* use the smaller scale for both sx and sy */
	    map.sx = map.sy;
	    b->nx = ceil(a->nx*map.sx);
	}
	else {
	    map.sy = map.sx;
	    b->ny = ceil(a->ny*map.sy);
	}
    }
    if (intscale) {
	if (map.sx>1.) {
	    map.sx = floor(map.sx);
	    b->nx = ceil(a->nx*map.sx);
	}
	if (map.sy>1.) {
	    map.sy = floor(map.sy);
	    b->ny = ceil(a->ny*map.sy);
	}
    }
    window_box_set_max(b);
    map.tx = b->x0-.5 - map.sx*(a->x0-.5);
    map.ty = b->y0-.5 - map.sy*(a->y0-.5);
    zoom_continuous(apic, a, bpic, b, &map, xfilt, yfilt);
}

/*
 * zoom_continuous: zoom with explicit, continuous scale&tran control.
 * Zoom according to the mapping defined in m,
 * reading from window awin of apic and writing to window bwin of bpic.
 * Like zoom but allows continuous, subpixel scales and translates.
 * Meaning of m is explained above.  Pixels in the dest window bwin outside
 * of the support of the mapped source window will be left unchanged.
 * Scales in m should be positive.
 */

zoom_continuous(apic, awin, bpic, bwin, m, xfilt, yfilt)
Pic *apic, *bpic;
Window_box *awin, *bwin;
Mapping *m;
Filt *xfilt, *yfilt;
{
    int xy;
    Window_box a, b, bc, t;
    Filtpar ax, ay;

    /* we consider 3 channel and 4 channel to be the same */
    if ((pic_get_nchan(apic)==1) != (pic_get_nchan(bpic)==1)) {
	fprintf(stderr, "zoom_continuous: source has %d channels, dest has %d channels\n",
	    pic_get_nchan(apic), pic_get_nchan(bpic));
	return;
    }
    if (m->sx<=0. || m->sy<=0.) {
	fprintf(stderr, "zoom_continuous: negative scales not allowed\n");
	return;
    }

    a = *awin;
    b = *bwin;
    window_box_set_size(&a);
    window_box_set_size(&b);

    /*
     * find scale of filter in a space (source space)
     * when minifying, ascale=1/scale, but when magnifying, ascale=1
     */
    ax.scale = xfilt->blur*MAX(1., 1./m->sx);
    ay.scale = yfilt->blur*MAX(1., 1./m->sy);
    /*
     * find support radius of scaled filter
     * if ax.supp and ay.supp are both <=.5 then we've got point sampling.
     * Point sampling is essentially a special filter whose width is fixed
     * at one source pixel.
     */
    ax.supp = MAX(.5, ax.scale*xfilt->supp);
    ay.supp = MAX(.5, ay.scale*yfilt->supp);
    ax.wid = ceil(2.*ax.supp);
    ay.wid = ceil(2.*ay.supp);

    /* clip source and dest windows against their respective pictures */
    window_box_intersect(&a, (Window_box *)pic_get_window(apic, &t), &a);
    if (pic_get_window(bpic, &t)->x0 != UNDEF)
	window_box_intersect(&b, &t, &b);

    /* find valid dest window (transformed source + support margin) */
    bc.x0 =  ceil(m->sx*(a.x0-ax.supp)+m->tx+EPSILON);
    bc.y0 =  ceil(m->sy*(a.y0-ay.supp)+m->ty+EPSILON);
    bc.x1 = floor(m->sx*(a.x1+ax.supp)+m->tx-EPSILON);
    bc.y1 = floor(m->sy*(a.y1+ay.supp)+m->ty-EPSILON);
    window_box_set_size(&bc);

    if (b.x0<bc.x0 || b.x1>bc.x1 || b.y0<bc.y0 || b.y1>bc.y1) {
	/* requested dest window lies outside the valid dest, so clip dest */
	/*
	window_box_print("    clipping odst=", &b);
	window_box_print(" to ", &bc);
	printf("\n");
	*/
	window_box_intersect(&b, &bc, &b);
    }
    pic_set_window(bpic, &b);

    /* compute offsets for MAP (these will be .5 if zoom() routine was called)*/
    m->ux = b.x0-m->sx*(a.x0-.5)-m->tx;
    m->uy = b.y0-m->sy*(a.y0-.5)-m->ty;

    /*
    printf("src=%s:%s", pic_get_dev(apic), pic_get_name(apic));
    window_box_print("", &a);
    printf("\ndst=%s:%s", pic_get_dev(bpic), pic_get_name(bpic));
    window_box_print("", &b);
    printf("\n");
    */

    if (a.nx<=0 || a.ny<=0 || b.nx<=0 || b.ny<=0) return;

    /* check for high-level simplifications of filter */
    xfilt = simplify_filter("x", zoom_coerce, xfilt, &ax, m->sx, m->ux);
    yfilt = simplify_filter("y", zoom_coerce, yfilt, &ay, m->sy, m->uy);

    /*
     * decide which filtering order (xy or yx) is faster for this mapping by
     * counting convolution multiplies
     */
    xy = zoom_xy!=UNDEF ? zoom_xy :
	b.nx*(a.ny*ax.wid+b.ny*ay.wid) < b.ny*(b.nx*ax.wid+a.nx*ay.wid);

    /* choose the appropriate filtering routine */
    if (str_eq(xfilt->name, "point") && str_eq(yfilt->name, "point"))
	zoom_unfiltered(apic, &a, bpic, &b, m);
    else if (xy)
	zoom_filtered_xy(apic, &a, bpic, &b, m, xfilt, &ax, yfilt, &ay);
    else
	zoom_filtered_yx(apic, &a, bpic, &b, m, xfilt, &ax, yfilt, &ay);
}

/*
 * filter_simplify: check if our discrete sampling of an arbitrary continuous
 * filter, parameterized by the filter spacing (a->scale), its radius (a->supp),
 * and the scale and offset of the coordinate mapping (s and u), causes the
 * filter to reduce to point sampling.
 *
 * It reduces if support is less than 1 pixel or
 * if integer scale and translation, and filter is cardinal
 */

static Filt *simplify_filter(dim, coerce, filter, a, s, u)
char *dim;
int coerce;
Filt *filter;
Filtpar *a;
double s, u;	/* scale and offset */
{
    if (coerce && (a->supp<=.5 ||
	filter->cardinal && INTEGER(1./a->scale) && INTEGER(1./(s*a->scale))
	&& INTEGER((u/s-.5)/a->scale))) {
	    if (!str_eq(filter->name, "point"))
		printf("coercing %sfilter=%s to point\n", dim, filter->name);
	    filter = filt_find("point");
	    a->scale = 1.;
	    a->supp = .5;
	    a->wid = 1;
	}
    return filter;
}

/*----------------------------------------------------------------------*/

/* zoom_unfiltered: do unfiltered zoom (point sampling) */

zoom_unfiltered(apic, a, bpic, b, m)
Pic *apic, *bpic;
Window_box *a, *b;
Mapping *m;
{
    int overlap;

    /* do source and dest windows overlap? */
    overlap = apic==bpic && window_box_overlap(a, b);

    printf("-map %g %g %g %g\n", m->sx, m->sy, m->tx, m->ty);
    /*
     * note: We have only x-resample before y-resample versions of the
     * unfiltered zoom case; we could optimize further by creating
     * y-resample before x-resample versions.
     */
    if (pic_get_nchan(apic)==1)
	zoom_unfiltered_mono(apic, a, bpic, b, m, overlap);
    else
	zoom_unfiltered_rgb (apic, a, bpic, b, m, overlap);
}

/* zoom_unfiltered_mono: monochrome unfiltered zoom */

zoom_unfiltered_mono(apic, a, bpic, b, m, overlap)
Pic *apic, *bpic;
Window_box *a, *b;
Mapping *m;
int overlap;
{
    int byi, by, ay, ayold, bx, ax;
    Pixel1 *abuf, *bbuf, *bp;	/* source and dest scanline buffers */
    Pixel1 **xmap, **xp;	/* mapping from abuf to bbuf (optimization) */
    short *ymap;		/* order of dst y coords that avoids feedback */
    char *linewritten;		/* has scanline y been written? (debugging) */

    ALLOC(abuf, Pixel1, a->nx);
    ALLOC(bbuf, Pixel1, b->nx);
    ALLOC(xmap, Pixel1 *, b->nx);
    ALLOC(ymap, short, b->ny);
    ALLOC_ZERO(linewritten, char, MAX(a->y1, b->y1)+1);

    /* if overlapping src & dst, find magic y ordering that avoids feedback */
    make_map_table(m->sy, m->ty, .5, a->y0, b->y0, b->ny, overlap, ymap);

    for (bx=0; bx<b->nx; bx++) {
	ax = MAP(bx, m->sx, m->ux);
	xmap[bx] = &abuf[ax];
    }

    ayold = -1;		/* impossible value for ay */
    for (byi=0; byi<b->ny; byi++) {
	by = ymap[byi];
	ay = MAP(by, m->sy, m->uy);
	/* scan line a.y0+ay goes to b.y0+by */
	if (ay!=ayold) {		/* new scan line; read it in */
	    ayold = ay;
	    if (overlap && linewritten[a->y0+ay])
		fprintf(stderr, "FEEDBACK ON LINE %d\n", a->y0+ay);
	    /* read scan line into abuf */
	    pic_read_row(apic, a->y0+ay, a->x0, a->nx, abuf);
	    /* resample in x */
	    for (bp=bbuf, xp=xmap, bx=0; bx<b->nx; bx++)
		*bp++ = **xp++;
	}
	pic_write_row(bpic, b->y0+by, b->x0, b->nx, bbuf);
	linewritten[b->y0+by] = 1;
    }
    free(abuf);
    free(bbuf);
    free(xmap);
    free(ymap);
}

/* zoom_unfiltered_rgb: color unfiltered zoom */

zoom_unfiltered_rgb(apic, a, bpic, b, m, overlap)
Pic *apic, *bpic;
Window_box *a, *b;
Mapping *m;
int overlap;
{
    int byi, by, ay, ayold, bx, ax;
    Pixel1_rgba *abuf, *bbuf, *bp;	/* source and dest scanline buffers */
    Pixel1_rgba **xmap, **xp;		/* mapping from abuf to bbuf */
    short *ymap;		/* order of dst y coords that avoids feedback */
    char *linewritten;		/* has scanline y been written? (debugging) */

    ALLOC(abuf, Pixel1_rgba, a->nx);
    ALLOC(bbuf, Pixel1_rgba, b->nx);
    ALLOC(xmap, Pixel1_rgba *, b->nx);
    ALLOC(ymap, short, b->ny);
    ALLOC_ZERO(linewritten, char, MAX(a->y1, b->y1)+1);

    /* if overlapping src & dst, find magic y ordering that avoids feedback */
    make_map_table(m->sy, m->ty, .5, a->y0, b->y0, b->ny, overlap, ymap);

    for (bx=0; bx<b->nx; bx++) {
	ax = MAP(bx, m->sx, m->ux);
	xmap[bx] = &abuf[ax];
    }

    ayold = -1;		/* impossible value for ay */
    for (byi=0; byi<b->ny; byi++) {
	by = ymap[byi];
	ay = MAP(by, m->sy, m->uy);
	/* scan line a.y0+ay goes to b.y0+by */
	if (ay!=ayold) {		/* new scan line; read it in */
	    ayold = ay;
	    if (overlap && linewritten[a->y0+ay])
		fprintf(stderr, "FEEDBACK ON LINE %d\n", a->y0+ay);
	    /* read scan line into abuf */
	    pic_read_row_rgba(apic, a->y0+ay, a->x0, a->nx, abuf);
	    /* resample in x */
	    for (bp=bbuf, xp=xmap, bx=0; bx<b->nx; bx++)
		*bp++ = **xp++;
	}
	pic_write_row_rgba(bpic, b->y0+by, b->x0, b->nx, bbuf);
	linewritten[b->y0+by] = 1;
    }
    free(abuf);
    free(bbuf);
    free(xmap);
    free(ymap);
}

/*----------------------------------------------------------------------*/

/*
 * zoom_filtered_xy: filtered zoom, xfilt before yfilt
 *
 * note: when calling make_weighttab, we can trim leading and trailing
 * zeros from the x weight buffers as an optimization,
 * but not for y weight buffers since the split formula is anticipating
 * a constant amount of buffering of source scanlines;
 * trimming zeros in yweight could cause feedback.
 */


zoom_filtered_xy(apic, a, bpic, b, m, xfilt, ax, yfilt, ay)
Pic *apic, *bpic;
Window_box *a, *b;
Mapping *m;
Filt *xfilt, *yfilt;	/* x and y filters */
Filtpar *ax, *ay;	/* extra x and y filter parameters */
{
    int ayf, bx, byi, by, overlap, nchan;

				/*PIXELTYPE NBITS  RAISON D'ETRE */
    Scanline abbuf;		/*   PIXEL1   8  src&dst scanline bufs */
    Scanline accum;		/*   PIXEL4  28  accumulator buf for yfilt */
    Scanline *linebuf, *tbuf;	/*   PIXEL2  14  circular buf of active lines */

    short *ymap;		/* order of dst y coords that avoids feedback */
    Weighttab *xweights;	/* sampled filter at each dst x pos; for xfilt*/
    short *xweightbuf, *xwp;	/* big block of memory addressed by xweights */
    Weighttab yweight;		/* a single sampled filter for current y pos */
    char *linewritten;		/* has scanline y been written? (debugging) */

    nchan = pic_get_nchan(apic)==1 ? PIXEL_MONO : PIXEL_RGB;
    scanline_alloc(&abbuf, PIXEL1|nchan, MAX(a->nx, b->nx));
    scanline_alloc(&accum, PIXEL4|nchan, b->nx);
    ALLOC(linebuf, Scanline, ay->wid);
    /* construct circular buffer of ay->wid intermediate scanlines */
    for (ayf=0; ayf<ay->wid; ayf++) {
	scanline_alloc(&linebuf[ayf], PIXEL2|nchan, b->nx);
	linebuf[ayf].y = -1;		/* mark scanline as unread */
    }

    ALLOC(ymap, short, b->ny);
    ALLOC(xweights, Weighttab, b->nx);
    ALLOC(xweightbuf, short, b->nx*ax->wid);
    ALLOC(yweight.weight, short, ay->wid);
    ALLOC_ZERO(linewritten, char, MAX(a->y1, b->y1)+1);

    /* do source and dest windows overlap? */
    overlap = apic==bpic && window_box_overlap(a, b);

    /*
    printf("-xy -map %g %g %g %g\n", m->sx, m->sy, m->tx, m->ty);
    printf("X: filt=%s support=%g,  scale=%g scaledsupport=%g width=%d\n",
	xfilt->name, xfilt->supp, ax->scale, ax->supp, ax->wid);
    printf("Y: filt=%s support=%g,  scale=%g scaledsupport=%g width=%d\n",
	yfilt->name, yfilt->supp, ay->scale, ay->supp, ay->wid);
    */

    /*
     * prepare a weighttab (a sampled filter for source pixels) for
     * each dest x position
     */
    for (xwp=xweightbuf, bx=0; bx<b->nx; bx++, xwp+=ax->wid) {
	xweights[bx].weight = xwp;
	make_weighttab(b->x0+bx, MAP(bx, m->sx, m->ux),
	    xfilt, ax, a->nx, zoom_trimzeros, &xweights[bx]);
    }

    /* if overlapping src & dst, find magic y ordering that avoids feedback */
    make_map_table(m->sy, m->ty, ay->supp, a->y0, b->y0, b->ny, overlap,
	ymap);

    for (byi=0; byi<b->ny; byi++) {	/* loop over dest scanlines */
	by = ymap[byi];
	if (zoom_debug) printf("by=%d: ", b->y0+by);
	/* prepare a weighttab for dest y position by */
	make_weighttab(b->y0+by, MAP(by, m->sy, m->uy),
	    yfilt, ay, a->ny, 0, &yweight);
	if (zoom_debug) printf("ay=%d-%d, reading: ",
	    a->y0+yweight.i0, a->y0+yweight.i1-1);
	scanline_zero(&accum);

	/* loop over source scanlines that influence this dest scanline */
	for (ayf=yweight.i0; ayf<yweight.i1; ayf++) {
	    tbuf = &linebuf[ayf % ay->wid];
	    if (tbuf->y != ayf) {	/* scanline needs to be read in */
		if (zoom_debug) printf("%d ", a->y0+ayf);
		if (overlap && linewritten[a->y0+ayf])
		    fprintf(stderr, "FEEDBACK ON LINE %d\n", a->y0+ayf);
		tbuf->y = ayf;
		/* read source scanline into abbuf */
		abbuf.len = a->nx;
		scanline_read(apic, a->x0, a->y0+ayf, &abbuf);
		/* and filter it into the appropriate line of linebuf (xfilt) */
		scanline_filter(CHANBITS, xweights, &abbuf, tbuf);
	    }
	    /* add weighted tbuf into accum (these do yfilt) */
	    scanline_accum(yweight.weight[ayf-yweight.i0], tbuf, &accum);
	}

	/* shift accum down into abbuf and write out dest scanline */
	abbuf.len = b->nx;
	scanline_shift(FINALSHIFT, &accum, &abbuf);
	scanline_write(bpic, b->x0, b->y0+by, &abbuf);
	linewritten[b->y0+by] = 1;
	if (zoom_debug) printf("\n");
    }

    scanline_free(&abbuf);
    scanline_free(&accum);
    for (ayf=0; ayf<ay->wid; ayf++)
	scanline_free(&linebuf[ayf]);
    free(ymap);
    free(linebuf);
    free(xweights);
    free(xweightbuf);
    free(yweight.weight);
    free(linewritten);
    statistics();
}

/* zoom_filtered_yx: filtered zoom, yfilt before xfilt */

zoom_filtered_yx(apic, a, bpic, b, m, xfilt, ax, yfilt, ay)
Pic *apic, *bpic;
Window_box *a, *b;
Mapping *m;
Filt *xfilt, *yfilt;	/* x and y filters */
Filtpar *ax, *ay;	/* extra x and y filter parameters */
{
    int ayf, bx, byi, by, overlap, nchan;

				/*PIXELTYPE NBITS  RAISON D'ETRE */
    Scanline bbuf;		/*   PIXEL1   8  dst scanline buf */
    Scanline accum;		/*   PIXEL4  22  accumulator buf for yfilt */
    Scanline *linebuf, *tbuf;	/*   PIXEL1   8  circular buf of active lines */

    short *ymap;		/* order of dst y coords that avoids feedback */
    Weighttab *xweights;	/* sampled filter at each dst x pos; for xfilt*/
    short *xweightbuf, *xwp;	/* big block of memory addressed by xweights */
    Weighttab yweight;		/* a single sampled filter for current y pos */
    char *linewritten;		/* has scanline y been written? (debugging) */

    nchan = pic_get_nchan(apic)==1 ? PIXEL_MONO : PIXEL_RGB;
    scanline_alloc(&bbuf, PIXEL1|nchan, b->nx);
    scanline_alloc(&accum, PIXEL4|nchan, a->nx);
    ALLOC(linebuf, Scanline, ay->wid);
    /* construct circular buffer of ay->wid intermediate scanlines */
    for (ayf=0; ayf<ay->wid; ayf++) {
	scanline_alloc(&linebuf[ayf], PIXEL1|nchan, a->nx);
	linebuf[ayf].y = -1;		/* mark scanline as unread */
    }

    ALLOC(ymap, short, b->ny);
    ALLOC(xweights, Weighttab, b->nx);
    ALLOC(xweightbuf, short, b->nx*ax->wid);
    ALLOC(yweight.weight, short, ay->wid);
    ALLOC_ZERO(linewritten, char, MAX(a->y1, b->y1)+1);

    /* do source and dest windows overlap? */
    overlap = apic==bpic && window_box_overlap(a, b);

    /*
    printf("-yx -map %g %g %g %g\n", m->sx, m->sy, m->tx, m->ty);
    printf("X: filt=%s supp=%g  scale=%g scaledsupp=%g wid=%d\n",
	xfilt->name, xfilt->supp, ax->scale, ax->supp, ax->wid);
    printf("Y: filt=%s supp=%g  scale=%g scaledsupp=%g wid=%d\n",
	yfilt->name, yfilt->supp, ay->scale, ay->supp, ay->wid);
    */

    /*
     * prepare a weighttab (a sampled filter for source pixels) for
     * each dest x position
     */
    for (xwp=xweightbuf, bx=0; bx<b->nx; bx++, xwp+=ax->wid) {
	xweights[bx].weight = xwp;
	make_weighttab(b->x0+bx, MAP(bx, m->sx, m->ux),
	    xfilt, ax, a->nx, zoom_trimzeros, &xweights[bx]);
    }

    /* if overlapping src & dst, find magic y ordering that avoids feedback */
    make_map_table(m->sy, m->ty, ay->supp, a->y0, b->y0, b->ny, overlap,
	ymap);

    for (byi=0; byi<b->ny; byi++) {     /* loop over dest scanlines */
	by = ymap[byi];
	if (zoom_debug) printf("by=%d: ", b->y0+by);
	/* prepare a weighttab for dest y position by */
	make_weighttab(b->y0+by, MAP(by, m->sy, m->uy),
	    yfilt, ay, a->ny, 0, &yweight);
	if (zoom_debug) printf("ay=%d-%d, reading: ",
	    a->y0+yweight.i0, a->y0+yweight.i1-1);
	scanline_zero(&accum);

	/* loop over source scanlines that influence this dest scanline */
	for (ayf=yweight.i0; ayf<yweight.i1; ayf++) {
	    tbuf = &linebuf[ayf % ay->wid];
	    if (tbuf->y != ayf) {	/* scanline needs to be read in */
		if (zoom_debug) printf("%d ", a->y0+ayf);
		if (overlap && linewritten[a->y0+ayf])
		    fprintf(stderr, "FEEDBACK ON LINE %d\n", a->y0+ayf);
		tbuf->y = ayf;
		/* read source scanline into linebuf */
		scanline_read(apic, a->x0, a->y0+ayf, tbuf);
	    }
	    /* add weighted tbuf into accum (these do yfilt) */
	    scanline_accum(yweight.weight[ayf-yweight.i0], tbuf, &accum);
	}

	/* and filter it into the appropriate line of linebuf (xfilt) */
	scanline_filter(FINALSHIFT, xweights, &accum, &bbuf);
	/* and write out dest scanline in bbuf */
	scanline_write(bpic, b->x0, b->y0+by, &bbuf);
	linewritten[b->y0+by] = 1;
	if (zoom_debug) printf("\n");
    }

    scanline_free(&bbuf);
    scanline_free(&accum);
    for (ayf=0; ayf<ay->wid; ayf++)
	scanline_free(&linebuf[ayf]);
    free(ymap);
    free(linebuf);
    free(xweights);
    free(xweightbuf);
    free(yweight.weight);
    free(linewritten);
    statistics();
}

/*
 * make_weighttab: sample the continuous filter, scaled by ap->scale and
 * positioned at continuous source coordinate cen, for source coordinates in
 * the range [0..len-1], writing the weights into wtab.
 * Scale the weights so they sum to WEIGHTONE, and trim leading and trailing
 * zeros if trimzeros!=0.
 * b is the dest coordinate (for diagnostics).
 */

static make_weighttab(b, cen, filter, ap, len, trimzeros, wtab)
int b, len;
double cen;
Filt *filter;
Filtpar *ap;
int trimzeros;
Weighttab *wtab;
{
    int i0, i1, i, sum, t, stillzero, lastnonzero, nz;
    short *wp;
    double den, sc, tr;

    /* find the source coord range of this positioned filter: [i0..i1-1] */
    i0 = cen-ap->supp+.5;
    i1 = cen+ap->supp+.5;
    if (i0<0) i0 = 0;
    if (i1>len) i1 = len;
    if (i0>=i1) {
	fprintf(stderr, "make_weighttab: null filter at %d\n", b);
	exit(1);
    }
    /* the range of source samples to buffer: */
    wtab->i0 = i0;
    wtab->i1 = i1;

    /* find scale factor sc to normalize the filter */
    for (den=0, i=i0; i<i1; i++)
	den += filt_func(filter, (i+.5-cen)/ap->scale);
    /* set sc so that sum of sc*func() is approximately WEIGHTONE */
    sc = den==0. ? WEIGHTONE : WEIGHTONE/den;
    if (zoom_debug>1) printf("    b=%d cen=%g scale=%g [%d..%d) sc=%g:  ",
	b, cen, ap->scale, i0, i1, sc);

    /* compute the discrete, sampled filter coefficients */
    stillzero = trimzeros;
    for (sum=0, wp=wtab->weight, i=i0; i<i1; i++) {

	/* evaluate the filter function: */
	tr = sc*filt_func(filter, (i+.5-cen)/ap->scale);

	if (tr<MINSHORT || tr>MAXSHORT) {
	    fprintf(stderr, "tr=%g at %d\n", tr, b);
	    exit(1);
	}
	t = floor(tr+.5);
	if (stillzero && t==0) i0++;	/* find first nonzero */
	else {
	    stillzero = 0;
	    *wp++ = t;			/* add weight to table */
	    sum += t;
	    if (t!=0) lastnonzero = i;	/* find last nonzero */
	}
    }
    ntot += wtab->i1-wtab->i0;
    if (sum==0) {
	nz = wtab->i1-wtab->i0;
	/* fprintf(stderr, "sum=0 at %d\n", b); */
	wtab->i0 = wtab->i0+wtab->i1 >> 1;
	wtab->i1 = wtab->i0+1;
	wtab->weight[0] = WEIGHTONE;
    }
    else {
	if (trimzeros) {		/* skip leading and trailing zeros */
	    /* set wtab->i0 and ->i1 to the nonzero support of the filter */
	    nz = wtab->i1-wtab->i0-(lastnonzero-i0+1);
	    wtab->i0 = i0;
	    wtab->i1 = i1 = lastnonzero+1;
	}
	else				/* keep leading and trailing zeros */
	    nz = 0;
	if (sum!=WEIGHTONE) {
	    /*
	     * Fudge the center slightly to make sum=WEIGHTONE exactly.
	     * Is this the best way to normalize a discretely sampled
	     * continuous filter?
	     */
	    i = cen+.5;
	    if (i<i0) i = i0; else if (i>=i1) i = i1-1;
	    t = WEIGHTONE-sum;
	    if (zoom_debug>1) printf("[%d]+=%d ", i, t);
	    wtab->weight[i-i0] += t;	/* fudge center sample */
	}
    }
    if (nz>nzmax) nzmax = nz;
    nzero += nz;
    if (zoom_debug>1) {
	printf("\t");
	for (wp=wtab->weight, i=i0; i<i1; i++, wp++)
	    printf("%5d ", *wp);
	printf("\n");
    }
}

/*
 * make_map_table:
 * construct a table for the mapping a = (b-t)/s for b-b0 in [0..bn-1]
 * ordered so that the buffer resampling operation
 *	    for (bi=0; bi<bn; bi++) {
 *		b = map[bi];
 *		a = MAP(b, scale, offset);
 *		buf[b] = buf[a];
 *	    }
 * can work IN PLACE without feedback
 *
 * a and b here are source and dest coordinates, in either x or y.
 * This is needed only if there is overlap between source and dest windows.
 */

static make_map_table(scale, tran, asupp, a0, b0, bn, overlap, map)
double scale, tran;	/* scale and translate */
double asupp;		/* filter support radius in source space */
int a0, b0, bn;		/* source and dest positions, dest length */
int overlap;		/* do source and dest overlap? */
short *map;		/* mapping table */
{
    int split, b, i0;
    double z, u;

    /* find fixed point of mapping; let split=b-b0 at the point where a=b */

    if (overlap) {			/* source and dest windows overlap */
	if (scale==1.)			/* no scale change, translation only */
	    /* if moving left then scan to right, and vice versa */
	    split = a0<b0 ? 0 : bn;
	else {				/* magnify or minify */

	    /* THE MAGIC SPLIT FORMULA: */

	    if (scale>1.) {		/* magnify */
		/*
		 * choose integer nearest midpoint of valid interval:
		 *	x < split <= x+s/(s-1)
		 * where x=(tran+scale*asupp)/(1-scale)-b0
		 */
		split = floor((tran+scale*asupp-.5)/(1.-scale)-b0+1.);
	    }
	    else {			/* minify */
		/*
		 * only one valid split pt in this case:
		 *	x <= split < x+1
		 * so we take extra care (x as above)
		 */
		split = ceil((tran+scale*asupp)/(1.-scale)-b0);
		/*
		 * The above formula is perfect for exact arithmetic,
		 * but hardware roundoff can cause split to be one too big.
		 * Check for roundoff by simulating precisely the calculation
		 * of i0 in make_weighttab.
		 */
		u = b0-scale*(a0-.5)-tran;	/* recalculate offset */
		z = MAP(split-1, scale, u);	/* cen at b=split-1 */
		z = z-asupp+.5;
		i0 = z;				/* i0 at b=split-1 */
		if (a0+i0>=b0+split)		/* feedback at b=split-1? */
		    split--;			/* correct for roundoff */
	    }
	    if (split<0) split = 0;
	    else if (split>bn) split = bn;
	    printf("split at y=%d\n", b0+split);
	}

	if (scale>=1.) {		/* magnify: scan in toward split */
	    for (b=0;    b<split;  b++) *map++ = b;
	    for (b=bn-1; b>=split; b--) *map++ = b;
	}
	else {				/* minify: scan out away from split */
	    for (b=split-1; b>=0;  b--) *map++ = b;
	    for (b=split;   b<bn;  b++) *map++ = b;
	}
    }
    else				/* no overlap; either order will work */
	for (b=0; b<bn; b++) *map++ = b;	/* we opt for forward order */
}

static statistics()
{
#   ifdef DEBUG
	printf("%d/%d=%.2f%% of filter coeffs were zero, nzmax=%d\n",
	    nzero, ntot, 100.*nzero/ntot, nzmax);
#   endif
}
