#ifndef JT_GFX_DIB_H
#define JT_GFX_DIB_H

/*
 * Packed device-independent bitmaps, as stored in Windows 3.x BITMAP
 * resources: a BITMAPINFOHEADER, the color table, then the pixel rows,
 * bottom-up and padded to 4 bytes. Unlike a .bmp file there is no
 * BITMAPFILEHEADER in front.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A view into a packed DIB; valid while the buffer it was parsed from lives. */
typedef struct {
    const uint8_t *info;    /* BITMAPINFOHEADER immediately followed by the palette */
    size_t info_size;
    const uint8_t *pixels;
    size_t row_stride;
    int32_t width;
    int32_t height;
    uint16_t bits_per_pixel;
    uint32_t palette_entries;
} Dib;

/* Validates `data` and fills `dib` with views into it; nothing is copied. */
bool dib_parse(const uint8_t *data, size_t size, Dib *dib);

/* Palette index of the pixel at (x, y), counted from the top-left corner. */
uint8_t dib_pixel_index(const Dib *dib, int32_t x, int32_t y);

/* Palette entry as 0x00RRGGBB; black for indices past the palette. */
uint32_t dib_palette_color(const Dib *dib, uint8_t index);

/* Color of the pixel at (x, y) as 0x00RRGGBB. */
uint32_t dib_pixel_color(const Dib *dib, int32_t x, int32_t y);

#endif
