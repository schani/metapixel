#include <assert.h>
#include <stdlib.h>

#include <png.h>

#include "rwpng.h"

unsigned char*
read_png_file (char *filename, int *width, int *height)
{
     FILE *inFile = fopen(filename, "r");
     png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
     png_infop infoPtr,
	 endInfo;
     unsigned char *data, *row;
     int i;
     int bps, spp;

     assert(inFile != 0);
     assert(pngPtr != 0);

     infoPtr = png_create_info_struct(pngPtr);
     endInfo = png_create_info_struct(pngPtr);

     assert(infoPtr != 0);
     assert(endInfo != 0);

     if (setjmp(pngPtr->jmpbuf))
	 assert(0);

     png_init_io(pngPtr, inFile);

     png_read_info(pngPtr, infoPtr);

     *width = infoPtr->width;
     *height = infoPtr->height;

     assert(infoPtr->bit_depth == 8 || infoPtr->bit_depth == 16);
     assert(infoPtr->color_type == PNG_COLOR_TYPE_RGB || infoPtr->color_type == PNG_COLOR_TYPE_RGB_ALPHA);
     assert(infoPtr->interlace_type == PNG_INTERLACE_NONE);

     if (infoPtr->color_type == PNG_COLOR_TYPE_RGB)
	 spp = 3;
     else
	 spp = 4;

     if (infoPtr->bit_depth == 16)
	 bps = 2;
     else
	 bps = 1;

     data = (unsigned char*)malloc(*width * *height * 3);
     row = (unsigned char*)malloc(*width * spp * bps);

     for (i = 0; i < *height; ++i)
     {
	 int j, channel;

	 png_read_row(pngPtr, (png_bytep)row, 0);
	 for (j = 0; j < *width; ++j)
	     for (channel = 0; channel < 3; ++channel)
		 data[i * *width * 3 + j * 3 + channel] = row[j * spp * bps + channel * bps];
     }

     free(row);

     png_read_end(pngPtr, endInfo);
     png_destroy_read_struct(&pngPtr, &infoPtr, &endInfo);
     fclose(inFile);

     return data;
}

void
write_png_file (char *filename, int width, int height, unsigned char *data)
{
    FILE *outFile = fopen(filename, "w");
    png_structp pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop infoPtr;
    int i;

    assert(outFile != 0);
    assert(pngPtr != 0);

    infoPtr = png_create_info_struct(pngPtr);
    assert(infoPtr != 0);

    if (setjmp(pngPtr->jmpbuf))
	assert(0);

    png_init_io(pngPtr, outFile);

    infoPtr->width = width;
    infoPtr->height = height;
    infoPtr->valid = 0;
    infoPtr->rowbytes = width * 3;
    infoPtr->palette = 0;
    infoPtr->num_palette = 0;
    infoPtr->num_trans = 0;
    infoPtr->bit_depth = 8;
    infoPtr->color_type = PNG_COLOR_TYPE_RGB;
    infoPtr->compression_type = PNG_COMPRESSION_TYPE_BASE;
    infoPtr->filter_type = PNG_FILTER_TYPE_BASE;
    infoPtr->interlace_type = PNG_INTERLACE_NONE;

    png_write_info(pngPtr, infoPtr);

    for (i = 0; i < height; ++i)
	png_write_row(pngPtr, (png_bytep)(data + i * 3 * width));

    png_write_end(pngPtr, infoPtr);
    png_destroy_write_struct(&pngPtr, &infoPtr);
    fclose(outFile);
}
