/*
 * scanline: package of generic scanline operations
 * Performs various filtering operations on scanlines of pixels.
 * black&white or color, at 1, 2, or 4 bytes per pixel.
 * Operations are currently implemented for only a subset of the possible data
 * type combinations.
 *
 * Paul Heckbert	12 Sept 1988
 */

static char rcsid[] = "$Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/scanline.c,v 1.1 2000/01/04 03:35:21 schani Exp $";
#include <stdio.h>

#include <simple.h>
#include <pic.h>
#include "scanline.h"

/* some machines/compilers don't do unsigned chars correctly (PDP 11's) */
/* #define BAD_UCHAR /* define this if your machine has bad uchars */
#ifdef BAD_UCHAR
#   define UCHAR(x) ((x)&255)
#else
#   define UCHAR(x) (x)
#endif

#define BYTES(buf) ((buf)->type&PIXEL_BYTESMASK)
#define CHAN(buf) ((buf)->type&PIXEL_CHANMASK)
#define TYPECOMB(type1, type2) ((type1)<<3 | (type2))
#define PEG(val, t) (t = (val), t < 0 ? 0 : t > CHANWHITE ? CHANWHITE : t)

#define YOU_LOSE(routine) { \
    fprintf(stderr, "scanline_%s: bad scanline type: %d\n", \
	routine, buf->type); \
    exit(1); \
}

#define YOU_BLEW_IT(routine) { \
    fprintf(stderr, "scanline_%s: bad type combination: %d & %d\n", \
	routine, abuf->type, bbuf->type); \
    exit(1); \
}

/*
 * scanline_alloc: allocate memory for scanline buf of the given type and length
 */

scanline_alloc(buf, type, len)
Scanline *buf;
int type, len;
{
    buf->type = type;
    buf->len = len;
    switch (type) {
	case PIXEL_MONO|PIXEL1: ALLOC(buf->u.row1, Pixel1, len); break;
	case PIXEL_MONO|PIXEL2: ALLOC(buf->u.row2, Pixel2, len); break;
	case PIXEL_MONO|PIXEL4: ALLOC(buf->u.row4, Pixel4, len); break;
	case PIXEL_RGB |PIXEL1:
	    ALLOC(buf->u.row1_rgb, Scanline_rgb1, len); break;
	case PIXEL_RGB |PIXEL2:
	    ALLOC(buf->u.row2_rgb, Scanline_rgb2, len); break;
	case PIXEL_RGB |PIXEL4:
	    ALLOC(buf->u.row4_rgb, Scanline_rgb4, len); break;
	default: YOU_LOSE("alloc");
    }
}

/*
 * scanline_free: free the memory used by buf (but not buf itself)
 */

scanline_free(buf)
Scanline *buf;
{
    switch (buf->type) {
	case PIXEL_MONO|PIXEL1: free(buf->u.row1); break;
	case PIXEL_MONO|PIXEL2: free(buf->u.row2); break;
	case PIXEL_MONO|PIXEL4: free(buf->u.row4); break;
	case PIXEL_RGB |PIXEL1: free(buf->u.row1_rgb); break;
	case PIXEL_RGB |PIXEL2: free(buf->u.row2_rgb); break;
	case PIXEL_RGB |PIXEL4: free(buf->u.row4_rgb); break;
	default: YOU_LOSE("free");
    }
}

/*
 * scanline_zero: zero a scanline
 */

scanline_zero(buf)
Scanline *buf;
{
    switch (buf->type) {
	case PIXEL_MONO|PIXEL4:
	    bzero(buf->u.row4, buf->len*sizeof(Pixel4));
	    break;
	case PIXEL_RGB |PIXEL4:
	    bzero(buf->u.row4_rgb, buf->len*sizeof(Scanline_rgb4));
	    break;
	default:
	    YOU_LOSE("zero");
    }
}

/*
 * scanline_read: read buf->len pixels into buf starting at (x0,y0) in picture p
 */

scanline_read(p, x0, y0, buf)
Pic *p;
int x0, y0;
Scanline *buf;
{
    switch (buf->type) {
	case PIXEL_MONO|PIXEL1:
	    pic_read_row(p, y0, x0, buf->len, buf->u.row1);
	    break;
	case PIXEL_RGB |PIXEL1:
	    pic_read_row_rgba(p, y0, x0, buf->len, buf->u.row1_rgb);
	    break;
	default:
	    YOU_LOSE("read");
    }
}

/*
 * scanline_write: write buf->len pixels from buf into pic p starting at (x0,y0)
 */

scanline_write(p, x0, y0, buf)
Pic *p;
int x0, y0;
Scanline *buf;
{
    switch (buf->type) {
	case PIXEL_MONO|PIXEL1:
	    pic_write_row(p, y0, x0, buf->len, buf->u.row1);
	    break;
	case PIXEL_RGB |PIXEL1:
	    pic_write_row_rgba(p, y0, x0, buf->len, buf->u.row1_rgb);
	    break;
	default:
	    YOU_LOSE("write");
    }
}

/*
 * scanline_filter: convolve abuf with the sampled filter in wtab,
 * writing the result to bbuf after shifting down by shift bits
 * length of arrays taken to be bbuf->len
 *
 * Note that the Pixel4->Pixel1 routines shift abuf down by 8 bits before
 * multiplying, to avoid overflow.
 */

scanline_filter(shift, wtab, abuf, bbuf)
int shift;
Weighttab *wtab;
Scanline *abuf, *bbuf;
{
    switch (TYPECOMB(abuf->type, bbuf->type)) {
	case TYPECOMB(PIXEL_MONO|PIXEL1, PIXEL_MONO|PIXEL2):
	    pixel12_filter(bbuf->len, shift, wtab, abuf->u.row1, bbuf->u.row2);
	    break;
	case TYPECOMB(PIXEL_MONO|PIXEL4, PIXEL_MONO|PIXEL1):
	    pixel41_filter(bbuf->len, shift, wtab, abuf->u.row4, bbuf->u.row1);
	    break;
	case TYPECOMB(PIXEL_RGB |PIXEL1, PIXEL_RGB |PIXEL2):
	    pixel12_rgb_filter(bbuf->len, shift, wtab,
		abuf->u.row1_rgb, bbuf->u.row2_rgb);
	    break;
	case TYPECOMB(PIXEL_RGB |PIXEL4, PIXEL_RGB |PIXEL1):
	    pixel41_rgb_filter(bbuf->len, shift, wtab,
		abuf->u.row4_rgb, bbuf->u.row1_rgb);
	    break;
	default:
	    YOU_BLEW_IT("filter");
    }
}

pixel12_filter(bn, shift, wtab, abuf, bbuf)
int bn, shift;
Weighttab *wtab;
Pixel1 *abuf;
Pixel2 *bbuf;
{
    register int af, sum;
    register Pixel1 *ap;
    register short *wp;
    int b;

    for (b=0; b<bn; b++, wtab++) {
	/* start sum at 1<<shift-1 for rounding */
	for (sum=1 << shift-1, wp=wtab->weight, ap= &abuf[wtab->i0],
	    af=wtab->i1-wtab->i0; af>0; af--)
		sum += *wp++ * (short)UCHAR(*ap++);
	*bbuf++ = sum>>shift;
    }
}

pixel41_filter(bn, shift, wtab, abuf, bbuf)
int bn, shift;
Weighttab *wtab;
Pixel4 *abuf;
Pixel1 *bbuf;
{
    register int af, sum;
    register Pixel4 *ap;
    register short *wp;
    int t, b;

    for (b=0; b<bn; b++, wtab++) {
	/* start sum at 1<<shift-1 for rounding */
	for (sum=1 << shift-1, wp=wtab->weight, ap= &abuf[wtab->i0],
	    af=wtab->i1-wtab->i0; af>0; af--)
		sum += *wp++ * (short)(*ap++ >> CHANBITS);
	*bbuf++ = PEG(sum>>shift, t);
    }
}

pixel12_rgb_filter(bn, shift, wtab, abuf, bbuf)
int bn, shift;
Weighttab *wtab;
Scanline_rgb1 *abuf;
Scanline_rgb2 *bbuf;
{
    register Scanline_rgb1 *ap;
    register short *wp;
    register int sumr, sumg, sumb, af;
    int b;

    for (b=0; b<bn; b++, wtab++, bbuf++) {
	/* start sum at 1<<shift-1 for rounding */
	sumr = sumg = sumb = 1 << shift-1;
	for (wp=wtab->weight, ap= &abuf[wtab->i0],
	    af=wtab->i1-wtab->i0; af>0; af--, wp++, ap++) {
		sumr += *wp * (short)UCHAR(ap->r);
		sumg += *wp * (short)UCHAR(ap->g);
		sumb += *wp * (short)UCHAR(ap->b);
	    }
	bbuf->r = sumr>>shift;
	bbuf->g = sumg>>shift;
	bbuf->b = sumb>>shift;
    }
}

pixel41_rgb_filter(bn, shift, wtab, abuf, bbuf)
int bn, shift;
Weighttab *wtab;
Scanline_rgb4 *abuf;
Scanline_rgb1 *bbuf;
{
    register Scanline_rgb4 *ap;
    register short *wp;
    register int sumr, sumg, sumb, af;
    int t, b;

    for (b=0; b<bn; b++, wtab++, bbuf++) {
	/* start sum at 1<<shift-1 for rounding */
	sumr = sumg = sumb = 1 << shift-1;
	for (wp=wtab->weight, ap= &abuf[wtab->i0],
	    af=wtab->i1-wtab->i0; af>0; af--, wp++, ap++) {
		sumr += *wp * (short)(ap->r >> CHANBITS);
		sumg += *wp * (short)(ap->g >> CHANBITS);
		sumb += *wp * (short)(ap->b >> CHANBITS);
	    }
	bbuf->r = PEG(sumr>>shift, t);
	bbuf->g = PEG(sumg>>shift, t);
	bbuf->b = PEG(sumb>>shift, t);
    }
}

/*
 * scanline_accum: bbuf += weight*accum
 * length of arrays taken to be bbuf->len
 */

scanline_accum(weight, abuf, bbuf)
int weight;
Scanline *abuf, *bbuf;
{
    if (weight==0) return;
    if (BYTES(bbuf)!=PIXEL4 || CHAN(abuf)!=CHAN(bbuf))
	YOU_BLEW_IT("accum");
    switch (abuf->type) {
	case PIXEL_MONO|PIXEL1:
	    pixel14_accum(bbuf->len, weight, abuf->u.row1, bbuf->u.row4);
	    break;
	case PIXEL_MONO|PIXEL2:
	    pixel24_accum(bbuf->len, weight, abuf->u.row2, bbuf->u.row4);
	    break;
	case PIXEL_RGB |PIXEL1:
	    pixel14_rgb_accum(bbuf->len, weight,
		abuf->u.row1_rgb, bbuf->u.row4_rgb);
	    break;
	case PIXEL_RGB |PIXEL2:
	    pixel24_rgb_accum(bbuf->len, weight,
		abuf->u.row2_rgb, bbuf->u.row4_rgb);
	    break;
	default:
	    YOU_BLEW_IT("accum");
    }
}

pixel14_accum(n, weight, buf, accum)
register int n, weight;
register Pixel1 *buf;
register Pixel4 *accum;
{
    for (; n>0; n--)
	*accum++ += (short)weight * (short)UCHAR(*buf++);
}

pixel24_accum(n, weight, buf, accum)
register int n, weight;
register Pixel2 *buf;
register Pixel4 *accum;
{
    for (; n>0; n--)
	*accum++ += (short)weight * *buf++;
}

pixel14_rgb_accum(n, weight, buf, accum)
register int n, weight;
register Scanline_rgb1 *buf;
register Scanline_rgb4 *accum;
{
    for (; n>0; n--, accum++, buf++) {
	accum->r += (short)weight * (short)UCHAR(buf->r);
	accum->g += (short)weight * (short)UCHAR(buf->g);
	accum->b += (short)weight * (short)UCHAR(buf->b);
    }
}

pixel24_rgb_accum(n, weight, buf, accum)
register int n, weight;
register Scanline_rgb2 *buf;
register Scanline_rgb4 *accum;
{
    for (; n>0; n--, accum++, buf++) {
	accum->r += (short)weight * buf->r;
	accum->g += (short)weight * buf->g;
	accum->b += (short)weight * buf->b;
    }
}

/*
 * scanline_shift: bbuf = abuf>>shift
 * length of arrays taken to be bbuf->len
 */

scanline_shift(shift, abuf, bbuf)
int shift;
Scanline *abuf, *bbuf;
{
    if (BYTES(abuf)!=PIXEL4 || BYTES(bbuf)!=PIXEL1 || CHAN(abuf)!=CHAN(bbuf))
	YOU_BLEW_IT("shift");
    switch (CHAN(abuf)) {
	case PIXEL_MONO:
	    pixel41_shift(bbuf->len, shift, abuf->u.row4, bbuf->u.row1);
	    break;
	case PIXEL_RGB:
	    pixel41_rgb_shift(bbuf->len, shift,
		abuf->u.row4_rgb, bbuf->u.row1_rgb);
	    break;
    }
}

pixel41_shift(n, shift, accum, bbuf)
register int n, shift;
register Pixel4 *accum;
register Pixel1 *bbuf;
{
    register int t, half;

    half = 1 << shift-1;
    for (; n>0; n--)
	*bbuf++ = PEG(*accum+++half >> shift, t);
}

pixel41_rgb_shift(n, shift, accum, bbuf)
register int n, shift;
register Scanline_rgb4 *accum;
register Scanline_rgb1 *bbuf;
{
    register int t, half;

    half = 1 << shift-1;
    for (; n>0; n--, accum++, bbuf++) {
	bbuf->r = PEG(accum->r+half >> shift, t);
	bbuf->g = PEG(accum->g+half >> shift, t);
	bbuf->b = PEG(accum->b+half >> shift, t);
    }
}
