/* filt.h: definitions for filter data types and routines */

#ifndef FILT_HDR
#define FILT_HDR

/* $Header: /mnt/homes/cvsroot/metapixel/metapixel/libzoom/filt.h,v 1.1 2000/01/04 03:35:21 schani Exp $ */

typedef struct {		/* A 1-D FILTER */
    char *name;			/* name of filter */
    double (*func)();		/* filter function */
    double supp;		/* radius of nonzero portion */
    double blur;		/* blur factor (1=normal) */
    char windowme;		/* should filter be windowed? */
    char cardinal;		/* is this filter cardinal?
				   ie, does func(x) = (x==0) for integer x? */
    char unitrange;		/* does filter stay within the range [0..1] */
    void (*initproc)();		/* initialize client data, if any */
    void (*printproc)();	/* print client data, if any */
    char *clientdata;		/* client info to be passed to func */
} Filt;

#define filt_func(f, x) (*(f)->func)((x), (f)->clientdata)
#define filt_print_client(f) (*(f)->printproc)((f)->clientdata)

Filt *filt_find();
Filt *filt_window();
void filt_print();
void filt_catalog();


/* the filter collection: */

double filt_box();		/* box, pulse, Fourier window, */
double filt_triangle();		/* triangle, Bartlett window, */
double filt_quadratic();	/* 3rd order (quadratic) b-spline */
double filt_cubic();		/* 4th order (cubic) b-spline */
double filt_catrom();		/* Catmull-Rom spline, Overhauser spline */
double filt_mitchell();		/* Mitchell & Netravali's two-param cubic */
double filt_gaussian();		/* Gaussian (infinite) */
double filt_sinc();		/* Sinc, perfect lowpass filter (infinite) */
double filt_bessel();		/* Bessel (for circularly symm. 2-d filt, inf)*/

double filt_hanning();		/* Hanning window */
double filt_hamming();		/* Hamming window */
double filt_blackman();		/* Blackman window */
double filt_kaiser();		/* parameterized Kaiser window */

double filt_normal();		/* normal distribution (infinite) */


/* support routines */
double bessel_i0();

#endif
