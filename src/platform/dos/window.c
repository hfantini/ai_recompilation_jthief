#include "window.h"

#include "menu.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define WINDOW_LEFT 60
#define WINDOW_TOP 30
#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 300
#define MENU_BAR_HEIGHT 19
#define BUTTON_SIZE 18

static const Font *caption_font;
static char title[64];
static bool active = true;

static Rect frame_rect(void)
{
    return rect_make(WINDOW_LEFT, WINDOW_TOP, WINDOW_WIDTH, WINDOW_HEIGHT);
}

static Rect caption_rect(void)
{
    return rect_make(WINDOW_LEFT + 1, WINDOW_TOP + 1, WINDOW_WIDTH - 2, CAPTION_HEIGHT);
}

Rect window_menu_bar(void)
{
    return rect_make(WINDOW_LEFT + 1, caption_rect().bottom, WINDOW_WIDTH - 2, MENU_BAR_HEIGHT - 1);
}

Rect window_client(void)
{
    Rect frame = frame_rect();
    return rect_make(frame.left + 1, window_menu_bar().bottom + 1, WINDOW_WIDTH - 2,
                     frame.bottom - 1 - (window_menu_bar().bottom + 1));
}

void window_open(const Font *font, const char *initial_title)
{
    caption_font = font;
    window_set_title(initial_title);
}

void window_set_title(const char *new_title)
{
    snprintf(title, sizeof(title), "%s", new_title);
    ui_invalidate(caption_rect());
}

void window_set_active(bool now_active)
{
    if (now_active != active) {
        active = now_active;
        ui_invalidate(caption_rect());
    }
}

/* The system menu box: a bar with a shadow on a gray square. */
static void paint_system_box(Canvas *canvas, Rect box)
{
    int bar_top = box.top + (box.bottom - box.top) / 2 - 1;

    canvas_fill(canvas, box, SCHEME_BUTTON_FACE);
    canvas_fill(canvas, rect_make(box.left + 3, bar_top, box.right - box.left - 7, 3),
                SCHEME_BUTTON_HIGHLIGHT);
    canvas_fill(canvas, rect_make(box.left + 4, bar_top + 3, box.right - box.left - 7, 1),
                SCHEME_BUTTON_SHADOW);
    canvas_fill(canvas, rect_make(box.right - 4, bar_top + 1, 1, 3), SCHEME_BUTTON_SHADOW);
}

static void paint_minimize_button(Canvas *canvas, Rect box)
{
    int center = (box.left + box.right) / 2;
    int top = box.top + 7, row;

    canvas_fill(canvas, box, SCHEME_BUTTON_FACE);
    canvas_bevel(canvas, box, 1, SCHEME_BUTTON_HIGHLIGHT, SCHEME_BUTTON_SHADOW);
    for (row = 0; row < 4; row++) {
        canvas_fill(canvas, rect_make(center - 3 + row, top + row, 7 - 2 * row, 1), COLOR_BLACK);
    }
}

void window_paint_caption(Canvas *canvas, Rect rect, const Font *font, const char *text,
                          bool is_active)
{
    Rect bar = { rect.left, rect.top, rect.right, rect.bottom - 1 };
    int width = font_width(font, text, strlen(text));

    canvas_fill(canvas, bar, is_active ? SCHEME_ACTIVE_CAPTION : SCHEME_INACTIVE_CAPTION);
    canvas_fill(canvas, rect_make(rect.left, rect.bottom - 1, rect.right - rect.left, 1), SCHEME_FRAME);
    font_draw(font, canvas, (bar.left + bar.right - width) / 2,
              bar.top + (bar.bottom - bar.top - font->height) / 2, text, strlen(text),
              is_active ? SCHEME_CAPTION_TEXT : SCHEME_INACTIVE_CAPTION_TEXT);
}

static void paint_caption(Canvas *canvas)
{
    Rect caption = caption_rect();
    Rect system_box = rect_make(caption.left, caption.top, BUTTON_SIZE, BUTTON_SIZE);
    Rect minimize = rect_make(caption.right - BUTTON_SIZE, caption.top, BUTTON_SIZE, BUTTON_SIZE);

    window_paint_caption(canvas, caption, caption_font, title, active);
    paint_system_box(canvas, system_box);
    canvas_fill(canvas, rect_make(system_box.right, caption.top, 1, BUTTON_SIZE), SCHEME_FRAME);
    paint_minimize_button(canvas, minimize);
    canvas_fill(canvas, rect_make(minimize.left - 1, caption.top, 1, BUTTON_SIZE), SCHEME_FRAME);
}

void window_paint(Canvas *canvas)
{
    Rect menu_bar = window_menu_bar();

    canvas_fill(canvas, rect_make(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT), SCHEME_DESKTOP);
    canvas_frame(canvas, frame_rect(), SCHEME_FRAME);
    paint_caption(canvas);
    menu_paint_bar(canvas, menu_bar);
    canvas_fill(canvas, rect_make(menu_bar.left, menu_bar.bottom, menu_bar.right - menu_bar.left, 1),
                SCHEME_FRAME);
}
