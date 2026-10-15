#ifndef JT_DOS_WINDOW_H
#define JT_DOS_WINDOW_H

/*
 * The main window as Windows 3.1 drew it on a VGA: 500x300 at (60, 30) on a
 * 640x480 desktop, where the original's CreateWindow put it, with a caption,
 * a menu bar and the 498x260 client area the game was designed around.
 */

#include "canvas.h"
#include "font.h"

#include <stdbool.h>

/* The Windows 3.1 default color scheme, in VGA palette indices. */
enum {
    SCHEME_DESKTOP = COLOR_SILVER,
    SCHEME_WINDOW = COLOR_WHITE,
    SCHEME_TEXT = COLOR_BLACK,
    SCHEME_FRAME = COLOR_BLACK,
    SCHEME_ACTIVE_CAPTION = COLOR_NAVY,
    SCHEME_CAPTION_TEXT = COLOR_WHITE,
    SCHEME_INACTIVE_CAPTION = COLOR_WHITE,
    SCHEME_INACTIVE_CAPTION_TEXT = COLOR_BLACK,
    SCHEME_HIGHLIGHT = COLOR_NAVY,
    SCHEME_HIGHLIGHT_TEXT = COLOR_WHITE,
    SCHEME_BUTTON_FACE = COLOR_SILVER,
    SCHEME_BUTTON_SHADOW = COLOR_GRAY,
    SCHEME_BUTTON_HIGHLIGHT = COLOR_WHITE,
    SCHEME_GRAY_TEXT = COLOR_GRAY
};

/* The height of a caption, its bottom line included. */
#define CAPTION_HEIGHT 19

void window_open(const Font *font, const char *title);

void window_set_title(const char *title);

/* The caption turns white while a dialog has the focus. */
void window_set_active(bool active);

Rect window_client(void);
Rect window_menu_bar(void);

/* Desktop, frame, caption and menu bar: all but the client area. */
void window_paint(Canvas *canvas);

/* A caption bar, shared with dialogs. */
void window_paint_caption(Canvas *canvas, Rect rect, const Font *font, const char *title,
                          bool active);

#endif
