#ifndef __RWPNG_H__
#define __RWPNG_H__

unsigned char* read_png_file (char *filename, int *width, int *height);
void write_png_file (char *filename, int width, int height, unsigned char *data);

#endif
