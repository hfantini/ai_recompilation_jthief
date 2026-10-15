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

/*
 * The two layers as stored, for platforms that draw icons themselves: the
 * screen is ANDed with the mask, then XORed with the color.
 */
typedef struct {
    int width;
    int height;
    uint32_t *colors;      /* 0x00RRGGBB, rows top-down */
    uint8_t *keeps_screen; /* the AND mask, one byte per pixel */
} IconLayers;

/* `data` is the image without the cursor hotspot that precedes cursor images. */
bool icon_decode(const uint8_t *data, size_t size, IconImage *out);

void icon_free(IconImage *image);

bool icon_decode_layers(const uint8_t *data, size_t size, IconLayers *out);

void icon_free_layers(IconLayers *layers);

#endif
