#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "../readimage.h"
#include "../rwpng.h"

#include "raw.h"

static Pic_procs pic_raw_procs = {
    (char*(*)())raw_open, raw_close,
    raw_get_name,
    raw_clear, raw_clear_rgba,

    raw_set_nchan, raw_set_box,
    raw_write_pixel, raw_write_pixel_rgba,
    raw_write_row, raw_write_row_rgba,

    raw_get_nchan, raw_get_box,
    raw_read_pixel, raw_read_pixel_rgba,
    raw_read_row, raw_read_row_rgba
};

void
pic_create_raw (Pic *picpic, unsigned char *data, int width, int height)
{
    raw_pic_t *pic = (raw_pic_t*)malloc(sizeof(raw_pic_t));

    pic->width = width;
    pic->height = height;
    pic->filename = 0;
    pic->data = data;
    pic->save = 0;

    picpic->dev = "raw";
    picpic->procs = &pic_raw_procs;
    picpic->data = (char*)pic;
}

raw_pic_t*
raw_open (char *filename, char *mode)
{
    unsigned char *data;
    int width, height, save;
    raw_pic_t *pic;

    if (strcmp(mode, "r") == 0)
    {
	data = read_image(filename, &width, &height);
	assert(data != 0);
	save = 0;
    }
    else if (strcmp(mode, "w") == 0)
    {
	data = 0;
	width = PIXEL_UNDEFINED;
	height = PIXEL_UNDEFINED;
	save = 1;
    }
    else
	assert(0);

    pic = (raw_pic_t*)malloc(sizeof(raw_pic_t));
    pic->width = width;
    pic->height = height;
    pic->filename = strdup(filename);
    pic->data = data;
    pic->save = save;

    return pic;
}

void
raw_close (raw_pic_t *pic)
{
    if (pic->save)
    {
	assert(pic->data != 0);
	assert(pic->filename != 0);
	write_png_file(pic->filename, pic->width, pic->height, pic->data);
    }

    if (pic->data != 0)
	free(pic->data);
    free(pic);
}

char*
raw_get_name (raw_pic_t *pic)
{
    return pic->filename;
}

void
raw_clear (raw_pic_t *pic, Pixel1 pv)
{
    assert(0);
}

void
raw_clear_rgba (raw_pic_t *pic, Pixel1 r, Pixel1 g, Pixel1 b, Pixel1 a)
{
    assert(0);
}

void
raw_set_nchan (raw_pic_t *pic, int nchan)
{
    assert(nchan == 3);
}

void
raw_set_box (raw_pic_t *pic, int ox, int oy, int dx, int dy)
{
    assert(ox == 0 && oy == 0);

    if (pic->data != 0)
    {
	assert(dx == pic->width && dy == pic->height);
	return;
    }

    pic->width = dx;
    pic->height = dy;

    pic->data = (unsigned char*)malloc(pic->width * pic->height * 3);
}

void
raw_write_pixel (raw_pic_t *pic, int x, int y, Pixel1 pv)
{
    assert(0);
}

void
raw_write_pixel_rgba (raw_pic_t *pic, int x, int y, Pixel1 r, Pixel1 g, Pixel1 b, Pixel1 a)
{
    pic->data[(y * pic->width + x) * 3 + 0] = r;
    pic->data[(y * pic->width + x) * 3 + 1] = g;
    pic->data[(y * pic->width + x) * 3 + 2] = b;
}

void
raw_write_row (raw_pic_t *pic, int y, int x0, int nx, Pixel1 *buf)
{
    assert(0);
}

void
raw_write_row_rgba (raw_pic_t *pic, int y, int x0, int nx, Pixel1_rgba *buf)
{
    int x;

    for (x = 0; x < nx; ++x)
    {
	pic->data[(y * pic->width + x0 + x) * 3 + 0] = buf[x].r;
	pic->data[(y * pic->width + x0 + x) * 3 + 1] = buf[x].g;
	pic->data[(y * pic->width + x0 + x) * 3 + 2] = buf[x].b;
    }
}

int
raw_get_nchan (raw_pic_t *pic)
{
    return 3;
}

void
raw_get_box (raw_pic_t *pic, int *ox, int *oy, int *dx, int *dy)
{
    if (pic->width == PIXEL_UNDEFINED)
	*ox = *oy = PIXEL_UNDEFINED;
    else
	*ox = *oy = 0;
    *dx = pic->width;
    *dy = pic->height;
}

Pixel1
raw_read_pixel (raw_pic_t *pic, int x, int y)
{
    assert(0);
    return 0;
}

void
raw_read_pixel_rgba (raw_pic_t *pic, int x, int y, Pixel1_rgba *pv)
{
    pv->r = pic->data[(y * pic->width + x) * 3 + 0];
    pv->g = pic->data[(y * pic->width + x) * 3 + 1];
    pv->b = pic->data[(y * pic->width + x) * 3 + 2];
}

void
raw_read_row (raw_pic_t *pic, int y, int x0, int nx, Pixel1 *buf)
{
    assert(0);
}

void
raw_read_row_rgba (raw_pic_t *pic, int y, int x0, int nx, Pixel1_rgba *buf)
{
    int x;

    for (x = 0; x < nx; ++x)
    {
	buf[x].r = pic->data[(y * pic->width + x0 + x) * 3 + 0];
	buf[x].g = pic->data[(y * pic->width + x0 + x) * 3 + 1];
	buf[x].b = pic->data[(y * pic->width + x0 + x) * 3 + 2];
    }
}
