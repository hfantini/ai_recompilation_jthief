#ifndef JT_DOS_CANVAS_H
#define JT_DOS_CANVAS_H

/*
 * The whole screen as one byte per pixel, holding indices into the Windows
 * VGA palette, which is what the VGA shows. Everything is drawn here first,
 * then copied to the card where it changed.
 */

#include "dib.h"
#include "framebuffer.h"

#include <stdbool.h>
#include <stdint.h>

#define CANVAS_WIDTH 640
#define CANVAS_HEIGHT 480

/* The Windows VGA palette, in its order. */
enum {
    COLOR_BLACK = 0,
    COLOR_MAROON = 1,
    COLOR_GREEN = 2,
    COLOR_OLIVE = 3,
    COLOR_NAVY = 4,
    COLOR_PURPLE = 5,
    COLOR_TEAL = 6,
    COLOR_SILVER = 7,
    COLOR_GRAY = 8,
    COLOR_RED = 9,
    COLOR_LIME = 10,
    COLOR_YELLOW = 11,
    COLOR_BLUE = 12,
    COLOR_FUCHSIA = 13,
    COLOR_AQUA = 14,
    COLOR_WHITE = 15
};

/* Right and bottom are exclusive. */
typedef struct {
    int left, top, right, bottom;
} Rect;

typedef struct {
    uint8_t pixels[CANVAS_WIDTH * CANVAS_HEIGHT];
    Rect clip;
} Canvas;

Rect rect_make(int left, int top, int width, int height);
bool rect_empty(Rect rect);
bool rect_contains(Rect rect, int x, int y);
Rect rect_intersect(Rect a, Rect b);
Rect rect_union(Rect a, Rect b);
Rect rect_inflate(Rect rect, int amount);
Rect rect_offset(Rect rect, int dx, int dy);

/* The span of each screen row that may have changed; empty where left >= right. */
typedef struct {
    int16_t left[CANVAS_HEIGHT];
    int16_t right[CANVAS_HEIGHT];
} DirtyRows;

void dirty_clear(DirtyRows *dirty);
void dirty_add(DirtyRows *dirty, Rect rect);

/* Drawing is limited to `clip`, itself kept inside the screen. */
void canvas_set_clip(Canvas *canvas, Rect clip);
void canvas_reset_clip(Canvas *canvas);

void canvas_pixel(Canvas *canvas, int x, int y, uint8_t color);
void canvas_fill(Canvas *canvas, Rect rect, uint8_t color);

/* A one-pixel outline just inside `rect`. */
void canvas_frame(Canvas *canvas, Rect rect, uint8_t color);

/* Every other pixel of the outline, as Windows draws focus rectangles. */
void canvas_dotted_frame(Canvas *canvas, Rect rect, uint8_t color);

/* Raised (or sunken) edges `width` pixels wide just inside `rect`. */
void canvas_bevel(Canvas *canvas, Rect rect, int width, uint8_t light, uint8_t dark);

/* Copies a 32-bit image, dithering colors the VGA palette lacks. */
void canvas_blit_rgb(Canvas *canvas, const Framebuffer *source, Rect source_area, int x, int y);

/* A bitmap in the nearest VGA colors, the way Windows showed DIBs on a 16-color screen. */
void canvas_blit_dib(Canvas *canvas, const Dib *dib, int x, int y);

/* The VGA color for `rgb` at screen pixel (x, y), dithered when it is not a palette color. */
uint8_t canvas_color(uint32_t rgb, int x, int y);

#endif
