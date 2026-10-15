#ifndef JT_DOS_FONT_H
#define JT_DOS_FONT_H

/*
 * Text in the VGA's own ROM fonts, read from the video BIOS at runtime so
 * nothing is shipped. Each glyph is trimmed to its ink, which gives a
 * proportional face close to the Windows 3.x system font. Texts are in
 * Windows-1252, like the original's, and are mapped onto the ROM's code
 * page 437 glyphs.
 */

#include "canvas.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FONT_MAX_HEIGHT 16

typedef struct {
    int height;
    int baseline; /* the row just below capitals */
    int average_width;
    uint8_t rows[256][FONT_MAX_HEIGHT];
    uint8_t left[256];  /* first column of ink */
    uint8_t width[256]; /* advance, one blank column included */
} Font;

/* The 8x14 font is used where the original asked for an 8-point face, the 8x16 one elsewhere. */
bool font_load(Font *font, int height);

/* Width of `length` characters of Windows-1252 text. */
int font_width(const Font *font, const char *text, size_t length);

void font_draw(const Font *font, Canvas *canvas, int x, int y, const char *text, size_t length,
               uint8_t color);

/* The same into a 32-bit image, for the status bar texts drawn into the game frame. */
void font_draw_rgb(const Font *font, Framebuffer *fb, int x, int y, const char *text,
                   uint32_t color);

/* Text with Windows mnemonics: "&x" underlines x and "&&" stands for "&". */
int font_label_width(const Font *font, const char *label);
void font_draw_label(const Font *font, Canvas *canvas, int x, int y, const char *label,
                     uint8_t color);

/* The underlined letter of a label, uppercase; 0 if it has none. */
char font_label_mnemonic(const char *label);

/* Code page 437, as the keyboard types it, to Windows-1252; 0 when it has no equivalent. */
char font_from_keyboard(uint8_t ascii);

#endif
