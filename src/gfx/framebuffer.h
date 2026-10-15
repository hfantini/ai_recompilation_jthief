#ifndef JT_GFX_FRAMEBUFFER_H
#define JT_GFX_FRAMEBUFFER_H

/*
 * The 32-bit pixel buffer the game draws each frame into. The platform layer
 * copies it to the screen. Pixels are 0x00RRGGBB, rows top-down.
 */

#include "dib.h"

#include <stdbool.h>
#include <stdint.h>

#define FB_RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

#define FB_VGA_COLORS 16

/* The 16 Windows VGA colors, in device palette order. */
extern const uint32_t FB_VGA_PALETTE[FB_VGA_COLORS];

unsigned fb_nearest_vga_index(uint32_t color);

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    bool owns_pixels;
} Framebuffer;

bool fb_create(Framebuffer *fb, int width, int height);

/* Draws into memory owned by someone else, such as a platform bitmap. */
void fb_wrap(Framebuffer *fb, uint32_t *pixels, int width, int height);

void fb_destroy(Framebuffer *fb);

/* Parts outside the buffer are clipped by every drawing function. */
void fb_fill_rect(Framebuffer *fb, int x, int y, int width, int height, uint32_t color);

/*
 * Horizontal or vertical line from (x0, y0) toward (x1, y1), excluding the
 * end point, like GDI LineTo. Diagonal lines are not supported.
 */
void fb_draw_line(Framebuffer *fb, int x0, int y0, int x1, int y1, uint32_t color);

/* Plain copy, for scenery and the status bar. */
void fb_draw_bitmap(Framebuffer *fb, const Dib *dib, int x, int y);

/* Plain copy of the width x height area of `dib` starting at (src_x, src_y). */
void fb_draw_bitmap_region(Framebuffer *fb, const Dib *dib, int src_x, int src_y, int width,
                           int height, int x, int y);

/*
 * Draws a jewel or guard the way the original looked in 16 and 256 colors.
 * The original blits sprites with raster operation DSxn, NOT(dest XOR src),
 * which video cards applied to palette indices, not to RGB values; white
 * sprite pixels therefore leave the destination untouched.
 */
void fb_draw_sprite(Framebuffer *fb, const Dib *dib, int x, int y);

#endif
