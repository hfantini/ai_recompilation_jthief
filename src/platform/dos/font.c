#include "font.h"

#include <ctype.h>
#include <dpmi.h>
#include <string.h>
#include <sys/movedata.h>

#define GLYPHS 256
#define GLYPH_COLUMNS 8
#define FIRST_HIGH_CHARACTER 0x80
#define COPYRIGHT 0xA9

/*
 * Code page 437 glyph for each Windows-1252 character from 0x80 on, or the
 * plain letter when the ROM has no accented one; 0 when nothing fits.
 */
static const uint8_t CP437_OF_1252[128] = {
    0, 0, ',', 0x9F, '"', 0, 0, 0, '^', 0, 'S', '<', 0, 0, 'Z', 0,
    0, '\'', '\'', '"', '"', 0x07, '-', '-', '~', 0, 's', '>', 0, 0, 'z', 'Y',
    ' ', 0xAD, 0x9B, 0x9C, 0, 0x9D, '|', 0x15, '"', 0, 0xA6, 0xAE, 0xAA, '-', 0, '-',
    0xF8, 0xF1, 0xFD, 0, '\'', 0xE6, 0x14, 0xFA, ',', 0, 0xA7, 0xAF, 0xAC, 0xAB, 0, 0xA8,
    'A', 'A', 'A', 'A', 0x8E, 0x8F, 0x92, 0x80, 'E', 0x90, 'E', 'E', 'I', 'I', 'I', 'I',
    'D', 0xA5, 'O', 'O', 'O', 'O', 0x99, 'x', 'O', 'U', 'U', 'U', 0x9A, 'Y', 0, 0xE1,
    0x85, 0xA0, 0x83, 'a', 0x84, 0x86, 0x91, 0x87, 0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,
    0, 0xA4, 0x95, 0xA2, 0x93, 'o', 0x94, 0xF6, 'o', 0x97, 0xA3, 0x96, 0x81, 'y', 0, 0x98,
};

/* The ROM has no copyright sign, which the About box needs. */
static const uint8_t COPYRIGHT_GLYPH[] = { 0x38, 0x44, 0xB2, 0xA2, 0xB2, 0x44, 0x38 };

typedef void (*PlotPixel)(void *target, int x, int y);

typedef struct {
    Canvas *canvas;
    uint8_t color;
} CanvasPlot;

typedef struct {
    Framebuffer *fb;
    uint32_t color;
} RgbPlot;

static uint8_t rom_glyph(unsigned character)
{
    if (character < FIRST_HIGH_CHARACTER) {
        return (uint8_t)character;
    }
    return CP437_OF_1252[character - FIRST_HIGH_CHARACTER];
}

/* INT 10h function 1130h hands out the ROM fonts: 2 for 8x14, 6 for 8x16. */
static bool read_rom(int height, uint8_t *out)
{
    __dpmi_regs regs;

    memset(&regs, 0, sizeof(regs));
    regs.x.ax = 0x1130;
    regs.h.bh = height == 14 ? 2 : 6;
    __dpmi_int(0x10, &regs);
    if (regs.x.es == 0 && regs.x.bp == 0) {
        return false;
    }
    dosmemget((unsigned long)regs.x.es * 16 + regs.x.bp, (size_t)GLYPHS * (size_t)height, out);
    return true;
}

static int last_inked_row(const uint8_t *rows, int height)
{
    int row;

    for (row = height - 1; row >= 0; row--) {
        if (rows[row] != 0) {
            return row;
        }
    }
    return -1;
}

static void measure(Font *font, int character)
{
    uint8_t ink = 0;
    int row, first, last;

    for (row = 0; row < font->height; row++) {
        ink |= font->rows[character][row];
    }
    if (ink == 0) {
        font->left[character] = 0;
        font->width[character] = (uint8_t)(font->height / 3);
        return;
    }
    for (first = 0; !(ink & (0x80 >> first)); first++) {
    }
    for (last = GLYPH_COLUMNS - 1; !(ink & (0x80 >> last)); last--) {
    }
    font->left[character] = (uint8_t)first;
    font->width[character] = (uint8_t)(last - first + 2);
}

static void place_copyright(Font *font)
{
    int rows = (int)sizeof(COPYRIGHT_GLYPH);
    int top = font->baseline - rows;

    memset(font->rows[COPYRIGHT], 0, sizeof(font->rows[COPYRIGHT]));
    memcpy(font->rows[COPYRIGHT] + (top < 0 ? 0 : top), COPYRIGHT_GLYPH, (size_t)rows);
}

bool font_load(Font *font, int height)
{
    uint8_t rom[GLYPHS * FONT_MAX_HEIGHT];
    int character, total = 0;

    memset(font, 0, sizeof(*font));
    if (height > FONT_MAX_HEIGHT || !read_rom(height, rom)) {
        return false;
    }
    font->height = height;
    font->baseline = last_inked_row(rom + 'H' * height, height) + 1;
    for (character = 0; character < GLYPHS; character++) {
        uint8_t glyph = rom_glyph((unsigned)character);
        memcpy(font->rows[character], rom + glyph * height, (size_t)height);
    }
    place_copyright(font);
    for (character = 0; character < GLYPHS; character++) {
        measure(font, character);
    }
    for (character = 'A'; character <= 'Z'; character++) {
        total += font->width[character] + font->width[tolower(character)];
    }
    font->average_width = (total + 26) / 52;
    return true;
}

int font_width(const Font *font, const char *text, size_t length)
{
    int width = 0;
    size_t i;

    for (i = 0; i < length && text[i] != '\0'; i++) {
        width += font->width[(uint8_t)text[i]];
    }
    return width;
}

static int draw_glyph(const Font *font, unsigned character, int x, int y, PlotPixel plot,
                      void *target)
{
    int row, column;

    for (row = 0; row < font->height; row++) {
        uint8_t bits = (uint8_t)(font->rows[character][row] << font->left[character]);
        for (column = 0; bits != 0; column++, bits = (uint8_t)(bits << 1)) {
            if (bits & 0x80) {
                plot(target, x + column, y + row);
            }
        }
    }
    return font->width[character];
}

static void draw_text(const Font *font, int x, int y, const char *text, size_t length,
                      PlotPixel plot, void *target)
{
    size_t i;

    for (i = 0; i < length && text[i] != '\0'; i++) {
        x += draw_glyph(font, (uint8_t)text[i], x, y, plot, target);
    }
}

static void plot_canvas(void *target, int x, int y)
{
    CanvasPlot *plot = target;
    canvas_pixel(plot->canvas, x, y, plot->color);
}

static void plot_rgb(void *target, int x, int y)
{
    RgbPlot *plot = target;

    if (x >= 0 && y >= 0 && x < plot->fb->width && y < plot->fb->height) {
        plot->fb->pixels[(size_t)y * (size_t)plot->fb->width + (size_t)x] = plot->color;
    }
}

void font_draw(const Font *font, Canvas *canvas, int x, int y, const char *text, size_t length,
               uint8_t color)
{
    CanvasPlot plot = { canvas, color };
    draw_text(font, x, y, text, length, plot_canvas, &plot);
}

void font_draw_rgb(const Font *font, Framebuffer *fb, int x, int y, const char *text,
                   uint32_t color)
{
    RgbPlot plot = { fb, color };
    draw_text(font, x, y, text, strlen(text), plot_rgb, &plot);
}

int font_label_width(const Font *font, const char *label)
{
    int width = 0;

    for (; *label != '\0'; label++) {
        if (*label == '&' && label[1] != '\0') {
            label++;
        }
        width += font->width[(uint8_t)*label];
    }
    return width;
}

void font_draw_label(const Font *font, Canvas *canvas, int x, int y, const char *label,
                     uint8_t color)
{
    CanvasPlot plot = { canvas, color };

    for (; *label != '\0'; label++) {
        bool underline = false;
        int width;

        if (*label == '&' && label[1] != '\0') {
            label++;
            underline = *label != '&';
        }
        width = draw_glyph(font, (uint8_t)*label, x, y, plot_canvas, &plot);
        if (underline) {
            canvas_fill(canvas, rect_make(x, y + font->baseline + 1, width - 1, 1), color);
        }
        x += width;
    }
}

char font_label_mnemonic(const char *label)
{
    for (; *label != '\0'; label++) {
        if (*label == '&') {
            label++;
            if (*label != '&') {
                return (char)toupper((unsigned char)*label);
            }
        }
    }
    return 0;
}

char font_from_keyboard(uint8_t ascii)
{
    int i;

    if (ascii < FIRST_HIGH_CHARACTER) {
        return ascii >= ' ' ? (char)ascii : 0;
    }
    for (i = 0; i < 128; i++) {
        if (CP437_OF_1252[i] == ascii) {
            return (char)(FIRST_HIGH_CHARACTER + i);
        }
    }
    return 0;
}
