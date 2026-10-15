#include "canvas.h"

#include <string.h>

#define CACHE_SIZE 64
#define DITHER_CELLS 64
/* Mixes are made of sixteenths, each four cells of the dither matrix. */
#define MIX_PARTS 16
#define MIX_COLORS 3

/*
 * A color outside the palette is shown as the mix of up to three palette
 * colors that comes closest, spread with an ordered dither the way Windows
 * 3.1 dithered brushes on a VGA. Matrix cells below `limits[0]` take the
 * first color, those below `limits[1]` the second, the rest the third.
 */
typedef struct {
    uint32_t rgb;
    uint8_t colors[MIX_COLORS];
    uint8_t limits[MIX_COLORS - 1];
    bool used;
} Mix;

static const uint8_t BAYER[8][8] = {
    { 0, 32, 8, 40, 2, 34, 10, 42 },  { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44, 4, 36, 14, 46, 6, 38 }, { 60, 28, 52, 20, 62, 30, 54, 22 },
    { 3, 35, 11, 43, 1, 33, 9, 41 },  { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47, 7, 39, 13, 45, 5, 37 }, { 63, 31, 55, 23, 61, 29, 53, 21 },
};

static Mix cache[CACHE_SIZE];

Rect rect_make(int left, int top, int width, int height)
{
    Rect rect = { left, top, left + width, top + height };
    return rect;
}

bool rect_empty(Rect rect)
{
    return rect.left >= rect.right || rect.top >= rect.bottom;
}

bool rect_contains(Rect rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

Rect rect_intersect(Rect a, Rect b)
{
    Rect result;

    result.left = a.left > b.left ? a.left : b.left;
    result.top = a.top > b.top ? a.top : b.top;
    result.right = a.right < b.right ? a.right : b.right;
    result.bottom = a.bottom < b.bottom ? a.bottom : b.bottom;
    return result;
}

Rect rect_union(Rect a, Rect b)
{
    Rect result;

    if (rect_empty(a)) {
        return b;
    }
    if (rect_empty(b)) {
        return a;
    }
    result.left = a.left < b.left ? a.left : b.left;
    result.top = a.top < b.top ? a.top : b.top;
    result.right = a.right > b.right ? a.right : b.right;
    result.bottom = a.bottom > b.bottom ? a.bottom : b.bottom;
    return result;
}

Rect rect_inflate(Rect rect, int amount)
{
    Rect result = { rect.left - amount, rect.top - amount, rect.right + amount,
                    rect.bottom + amount };
    return result;
}

Rect rect_offset(Rect rect, int dx, int dy)
{
    Rect result = { rect.left + dx, rect.top + dy, rect.right + dx, rect.bottom + dy };
    return result;
}

void dirty_clear(DirtyRows *dirty)
{
    int y;

    for (y = 0; y < CANVAS_HEIGHT; y++) {
        dirty->left[y] = CANVAS_WIDTH;
        dirty->right[y] = 0;
    }
}

void dirty_add(DirtyRows *dirty, Rect rect)
{
    Rect area = rect_intersect(rect, rect_make(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT));
    int y;

    for (y = area.top; y < area.bottom; y++) {
        if (area.left < dirty->left[y]) {
            dirty->left[y] = (int16_t)area.left;
        }
        if (area.right > dirty->right[y]) {
            dirty->right[y] = (int16_t)area.right;
        }
    }
}

void canvas_set_clip(Canvas *canvas, Rect clip)
{
    canvas->clip = rect_intersect(clip, rect_make(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT));
}

void canvas_reset_clip(Canvas *canvas)
{
    canvas->clip = rect_make(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
}

void canvas_pixel(Canvas *canvas, int x, int y, uint8_t color)
{
    if (rect_contains(canvas->clip, x, y)) {
        canvas->pixels[y * CANVAS_WIDTH + x] = color;
    }
}

void canvas_fill(Canvas *canvas, Rect rect, uint8_t color)
{
    Rect area = rect_intersect(rect, canvas->clip);
    int y;

    if (rect_empty(area)) {
        return;
    }
    for (y = area.top; y < area.bottom; y++) {
        memset(canvas->pixels + y * CANVAS_WIDTH + area.left, color,
               (size_t)(area.right - area.left));
    }
}

void canvas_frame(Canvas *canvas, Rect rect, uint8_t color)
{
    canvas_fill(canvas, rect_make(rect.left, rect.top, rect.right - rect.left, 1), color);
    canvas_fill(canvas, rect_make(rect.left, rect.bottom - 1, rect.right - rect.left, 1), color);
    canvas_fill(canvas, rect_make(rect.left, rect.top, 1, rect.bottom - rect.top), color);
    canvas_fill(canvas, rect_make(rect.right - 1, rect.top, 1, rect.bottom - rect.top), color);
}

void canvas_dotted_frame(Canvas *canvas, Rect rect, uint8_t color)
{
    int x, y;

    for (x = rect.left; x < rect.right; x += 2) {
        canvas_pixel(canvas, x, rect.top, color);
        canvas_pixel(canvas, x, rect.bottom - 1, color);
    }
    for (y = rect.top; y < rect.bottom; y += 2) {
        canvas_pixel(canvas, rect.left, y, color);
        canvas_pixel(canvas, rect.right - 1, y, color);
    }
}

void canvas_bevel(Canvas *canvas, Rect rect, int width, uint8_t light, uint8_t dark)
{
    int i;

    for (i = 0; i < width; i++) {
        int left = rect.left + i, top = rect.top + i;
        int right = rect.right - 1 - i, bottom = rect.bottom - 1 - i;

        canvas_fill(canvas, rect_make(left, top, right - left, 1), light);
        canvas_fill(canvas, rect_make(left, top, 1, bottom - top), light);
        canvas_fill(canvas, rect_make(left + 1, bottom, right - left, 1), dark);
        canvas_fill(canvas, rect_make(right, top + 1, 1, bottom - top), dark);
    }
}

static int channel(uint32_t rgb, int shift)
{
    return (int)((rgb >> shift) & 0xFF);
}

/* Channels weigh as they do for the eye, roughly 0.3 red, 0.6 green and 0.1 blue. */
static int channel_weight(int shift)
{
    return shift == 16 ? 3 : shift == 8 ? 6 : 1;
}

/* Weighted squared distance between `target` and the mix, everything in sixteenths. */
static int32_t mix_error(uint32_t target, const uint8_t *colors, const int *parts)
{
    int32_t error = 0;
    int shift, i;

    for (shift = 0; shift <= 16; shift += 8) {
        int32_t difference = MIX_PARTS * channel(target, shift);

        for (i = 0; i < MIX_COLORS; i++) {
            difference -= parts[i] * channel(FB_VGA_PALETTE[colors[i]], shift);
        }
        error += channel_weight(shift) * difference * difference;
    }
    return error;
}

/* The best parts of the three colors, at or below `best_error`; false if none is. */
static bool best_parts(uint32_t rgb, const uint8_t *colors, int *parts, int32_t *best_error)
{
    int trial[MIX_COLORS];
    bool found = false;

    for (trial[0] = 0; trial[0] <= MIX_PARTS; trial[0]++) {
        for (trial[1] = 0; trial[0] + trial[1] <= MIX_PARTS; trial[1]++) {
            int32_t error;

            trial[2] = MIX_PARTS - trial[0] - trial[1];
            error = mix_error(rgb, colors, trial);
            if (*best_error < 0 || error < *best_error) {
                memcpy(parts, trial, sizeof(trial));
                *best_error = error;
                found = true;
            }
        }
    }
    return found;
}

static Mix find_mix(uint32_t rgb)
{
    Mix best;
    int32_t best_error = -1;
    uint8_t colors[MIX_COLORS];
    int parts[MIX_COLORS];

    memset(&best, 0, sizeof(best));
    best.rgb = rgb;
    best.used = true;
    for (colors[0] = 0; colors[0] < FB_VGA_COLORS; colors[0]++) {
        for (colors[1] = (uint8_t)(colors[0] + 1); colors[1] < FB_VGA_COLORS; colors[1]++) {
            for (colors[2] = (uint8_t)(colors[1] + 1); colors[2] < FB_VGA_COLORS; colors[2]++) {
                if (best_parts(rgb, colors, parts, &best_error)) {
                    memcpy(best.colors, colors, sizeof(colors));
                    best.limits[0] = (uint8_t)(parts[0] * DITHER_CELLS / MIX_PARTS);
                    best.limits[1] = (uint8_t)((parts[0] + parts[1]) * DITHER_CELLS / MIX_PARTS);
                }
            }
        }
    }
    return best;
}

/* Open addressing: the few colors a game uses all stay cached once mixed. */
static Mix lookup_mix(uint32_t rgb)
{
    unsigned slot = (rgb ^ (rgb >> 7) ^ (rgb >> 15)) % CACHE_SIZE;
    int probe;

    for (probe = 0; probe < CACHE_SIZE; probe++, slot = (slot + 1) % CACHE_SIZE) {
        if (!cache[slot].used) {
            cache[slot] = find_mix(rgb);
            return cache[slot];
        }
        if (cache[slot].rgb == rgb) {
            return cache[slot];
        }
    }
    return find_mix(rgb);
}

uint8_t canvas_color(uint32_t rgb, int x, int y)
{
    Mix mix = lookup_mix(rgb);
    uint8_t cell = BAYER[y & 7][x & 7];

    if (cell < mix.limits[0]) {
        return mix.colors[0];
    }
    return cell < mix.limits[1] ? mix.colors[1] : mix.colors[2];
}

void canvas_blit_rgb(Canvas *canvas, const Framebuffer *source, Rect source_area, int x, int y)
{
    Rect target = rect_intersect(rect_offset(source_area, x - source_area.left, y - source_area.top),
                                 canvas->clip);
    int row, column;

    for (row = target.top; row < target.bottom; row++) {
        const uint32_t *from = source->pixels
                             + (size_t)(row - y + source_area.top) * (size_t)source->width
                             + (target.left - x + source_area.left);
        uint8_t *to = canvas->pixels + row * CANVAS_WIDTH;

        for (column = target.left; column < target.right; column++) {
            to[column] = canvas_color(*from++, column, row);
        }
    }
}

void canvas_blit_dib(Canvas *canvas, const Dib *dib, int x, int y)
{
    int row, column;

    for (row = 0; row < dib->height; row++) {
        for (column = 0; column < dib->width; column++) {
            uint32_t rgb = dib_pixel_color(dib, column, row);
            canvas_pixel(canvas, x + column, y + row, (uint8_t)fb_nearest_vga_index(rgb));
        }
    }
}
