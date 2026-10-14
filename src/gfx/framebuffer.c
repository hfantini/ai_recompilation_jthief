#include "framebuffer.h"

#include <stdlib.h>

#define VGA_COLORS 16
#define WHITE FB_RGB(0xFF, 0xFF, 0xFF)

/*
 * The 16 Windows VGA colors, in device palette order. Color 15 - n is the
 * complement of color n, which is what made index arithmetic look right.
 * The 256-color static palette keeps these at indices 0-7 and 248-255, so
 * the same ROP gave the same result there too.
 */
static const uint32_t VGA_PALETTE[VGA_COLORS] = {
    FB_RGB(0x00, 0x00, 0x00), FB_RGB(0x80, 0x00, 0x00), FB_RGB(0x00, 0x80, 0x00),
    FB_RGB(0x80, 0x80, 0x00), FB_RGB(0x00, 0x00, 0x80), FB_RGB(0x80, 0x00, 0x80),
    FB_RGB(0x00, 0x80, 0x80), FB_RGB(0xC0, 0xC0, 0xC0), FB_RGB(0x80, 0x80, 0x80),
    FB_RGB(0xFF, 0x00, 0x00), FB_RGB(0x00, 0xFF, 0x00), FB_RGB(0xFF, 0xFF, 0x00),
    FB_RGB(0x00, 0x00, 0xFF), FB_RGB(0xFF, 0x00, 0xFF), FB_RGB(0x00, 0xFF, 0xFF),
    FB_RGB(0xFF, 0xFF, 0xFF),
};

/* Clipped intersection of a width x height image drawn at (x, y). */
typedef struct {
    int src_x, src_y;
    int dst_x, dst_y;
    int width, height;
} Clip;

static int channel_distance(uint32_t a, uint32_t b, int shift)
{
    int delta = (int)((a >> shift) & 0xFF) - (int)((b >> shift) & 0xFF);
    return delta * delta;
}

static unsigned nearest_vga_index(uint32_t color)
{
    unsigned best = 0, i;
    int best_distance = -1;

    for (i = 0; i < VGA_COLORS; i++) {
        int distance = channel_distance(color, VGA_PALETTE[i], 16)
                     + channel_distance(color, VGA_PALETTE[i], 8)
                     + channel_distance(color, VGA_PALETTE[i], 0);
        if (best_distance < 0 || distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    return best;
}

static uint32_t dsxn(uint32_t dest, uint32_t src)
{
    unsigned index = ~(nearest_vga_index(dest) ^ nearest_vga_index(src)) & (VGA_COLORS - 1);
    return VGA_PALETTE[index];
}

static bool clip(const Framebuffer *fb, int x, int y, int width, int height, Clip *out)
{
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right = x + width > fb->width ? fb->width : x + width;
    int bottom = y + height > fb->height ? fb->height : y + height;

    if (left >= right || top >= bottom) {
        return false;
    }
    out->src_x = left - x;
    out->src_y = top - y;
    out->dst_x = left;
    out->dst_y = top;
    out->width = right - left;
    out->height = bottom - top;
    return true;
}

bool fb_create(Framebuffer *fb, int width, int height)
{
    fb->pixels = calloc((size_t)width * (size_t)height, sizeof(*fb->pixels));
    fb->width = fb->pixels != NULL ? width : 0;
    fb->height = fb->pixels != NULL ? height : 0;
    fb->owns_pixels = fb->pixels != NULL;
    return fb->pixels != NULL;
}

void fb_wrap(Framebuffer *fb, uint32_t *pixels, int width, int height)
{
    fb->pixels = pixels;
    fb->width = width;
    fb->height = height;
    fb->owns_pixels = false;
}

void fb_destroy(Framebuffer *fb)
{
    if (fb->owns_pixels) {
        free(fb->pixels);
    }
    fb->pixels = NULL;
    fb->width = 0;
    fb->height = 0;
    fb->owns_pixels = false;
}

void fb_fill_rect(Framebuffer *fb, int x, int y, int width, int height, uint32_t color)
{
    Clip area;
    int row, col;

    if (!clip(fb, x, y, width, height, &area)) {
        return;
    }
    for (row = 0; row < area.height; row++) {
        uint32_t *dest = fb->pixels + (size_t)(area.dst_y + row) * (size_t)fb->width + area.dst_x;
        for (col = 0; col < area.width; col++) {
            dest[col] = color;
        }
    }
}

void fb_draw_line(Framebuffer *fb, int x0, int y0, int x1, int y1, uint32_t color)
{
    if (y0 == y1) {
        int left = x0 < x1 ? x0 : x1 + 1;
        fb_fill_rect(fb, left, y0, x0 < x1 ? x1 - x0 : x0 - x1, 1, color);
    }
    else if (x0 == x1) {
        int top = y0 < y1 ? y0 : y1 + 1;
        fb_fill_rect(fb, x0, top, 1, y0 < y1 ? y1 - y0 : y0 - y1, color);
    }
}

void fb_draw_bitmap(Framebuffer *fb, const Dib *dib, int x, int y)
{
    fb_draw_bitmap_region(fb, dib, 0, 0, dib->width, dib->height, x, y);
}

void fb_draw_bitmap_region(Framebuffer *fb, const Dib *dib, int src_x, int src_y, int width,
                           int height, int x, int y)
{
    Clip area;
    int row, col;

    /* Keep the source area inside the bitmap. */
    if (src_x < 0 || src_y < 0 || src_x >= dib->width || src_y >= dib->height) {
        return;
    }
    if (width > dib->width - src_x) {
        width = dib->width - src_x;
    }
    if (height > dib->height - src_y) {
        height = dib->height - src_y;
    }
    if (!clip(fb, x, y, width, height, &area)) {
        return;
    }
    for (row = 0; row < area.height; row++) {
        uint32_t *dest = fb->pixels + (size_t)(area.dst_y + row) * (size_t)fb->width + area.dst_x;
        for (col = 0; col < area.width; col++) {
            dest[col] = dib_pixel_color(dib, src_x + area.src_x + col, src_y + area.src_y + row);
        }
    }
}

void fb_draw_sprite(Framebuffer *fb, const Dib *dib, int x, int y)
{
    Clip area;
    int row, col;

    if (!clip(fb, x, y, dib->width, dib->height, &area)) {
        return;
    }
    for (row = 0; row < area.height; row++) {
        uint32_t *dest = fb->pixels + (size_t)(area.dst_y + row) * (size_t)fb->width + area.dst_x;
        for (col = 0; col < area.width; col++) {
            uint32_t src = dib_pixel_color(dib, area.src_x + col, area.src_y + row);
            /* Exact for white, and keeps backgrounds outside the VGA palette intact. */
            if (src != WHITE) {
                dest[col] = dsxn(dest[col], src);
            }
        }
    }
}
