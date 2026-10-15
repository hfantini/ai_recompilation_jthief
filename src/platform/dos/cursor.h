#ifndef JT_DOS_CURSOR_H
#define JT_DOS_CURSOR_H

/*
 * Mouse cursors drawn by the game itself: mouse drivers cannot draw them in
 * graphics modes reliably. As on a palette device, the screen's color index
 * is ANDed with the mask and XORed with the color index, so white over the
 * mask inverts what lies below.
 */

#include "canvas.h"
#include "neres.h"

#include <stdbool.h>
#include <stdint.h>

#define CURSOR_MAX_SIZE 32

typedef struct {
    int width, height;
    int hot_x, hot_y;
    uint8_t and_mask[CURSOR_MAX_SIZE * CURSOR_MAX_SIZE]; /* 0x0F keeps the screen, 0 clears it */
    uint8_t xor_index[CURSOR_MAX_SIZE * CURSOR_MAX_SIZE];
} Cursor;

/* A Windows 3.x cursor resource: the hotspot, then the image. */
bool cursor_from_resource(Cursor *cursor, const NeBlob *blob);

/* Stand-ins for the Windows system cursors, which the original does not carry. */
void cursor_arrow(Cursor *cursor);
void cursor_hourglass(Cursor *cursor);

/* Pixels under the cursor, to put back once the frame has been shown. */
typedef struct {
    Rect area;
    uint8_t pixels[CURSOR_MAX_SIZE * CURSOR_MAX_SIZE];
} CursorBackground;

void cursor_draw(const Cursor *cursor, Canvas *canvas, int x, int y, CursorBackground *saved);
void cursor_erase(Canvas *canvas, const CursorBackground *saved);

#endif
