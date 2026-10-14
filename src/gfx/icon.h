#ifndef JT_GFX_ICON_H
#define JT_GFX_ICON_H

/*
 * Windows 3.x icon and cursor images turned into RGBA, for platforms that
 * cannot load them directly. Such an image is a DIB of twice the image's
 * height: the color (XOR) image, then the 1-bit AND mask. Pixels the mask
 * keeps and the color is black stay transparent; the ones that would invert
 * the screen, which RGBA cannot express, become black.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int width;
    int height;
    uint8_t *rgba; /* non-premultiplied, rows top-down, 4 bytes per pixel */
} IconImage;

/* `data` is the image without the cursor hotspot that precedes cursor images. */
bool icon_decode(const uint8_t *data, size_t size, IconImage *out);

void icon_free(IconImage *image);

#endif
