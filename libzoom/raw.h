#include "pic.h"

#include "pixel.h"

typedef struct {
    int width;
    int height;
    char *filename;
    int save;
    unsigned char *data;
} raw_pic_t;

void pic_create_raw (Pic *picpic, unsigned char *data, int width, int height);

raw_pic_t* raw_open (char *filename, char *mode);
void raw_close (raw_pic_t *pic);

char* raw_get_name (raw_pic_t *pic);

void raw_clear (raw_pic_t *pic, Pixel1 pv);
void raw_clear_rgba (raw_pic_t *pic, Pixel1 r, Pixel1 g, Pixel1 b, Pixel1 a);

void raw_set_nchan (raw_pic_t *pic, int nchan);
void raw_set_box (raw_pic_t *pic, int ox, int oy, int dx, int dy);

void raw_write_pixel (raw_pic_t *pic, int x, int y, Pixel1 pv);
void raw_write_pixel_rgba (raw_pic_t *pic, int x, int y, Pixel1 r, Pixel1 g, Pixel1 b, Pixel1 a);
void raw_write_row (raw_pic_t *pic, int y, int x0, int nx, Pixel1 *buf);
void raw_write_row_rgba (raw_pic_t *pic, int y, int x0, int nx, Pixel1_rgba *buf);

int raw_get_nchan (raw_pic_t *pic);
void raw_get_box (raw_pic_t *pic, int *ox, int *oy, int *dx, int *dy);

Pixel1 raw_read_pixel (raw_pic_t *pic, int x, int y);
void raw_read_pixel_rgba (raw_pic_t *pic, int x, int y, Pixel1_rgba *pv);
void raw_read_row (raw_pic_t *pic, int y, int x0, int nx, Pixel1 *buf);
void raw_read_row_rgba (raw_pic_t *pic, int y, int x0, int nx, Pixel1_rgba *buf);
