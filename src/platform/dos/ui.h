#ifndef JT_DOS_UI_H
#define JT_DOS_UI_H

/*
 * What Windows did for the original: input events, the game clock, and a
 * screen made of the desktop, the main window and the menus and dialogs
 * stacked over it. Modal loops call ui_wait_event() like a message loop;
 * clock ticks reach the main window from any of them, as WM_TIMER did.
 */

#include "canvas.h"
#include "cursor.h"
#include "hardware.h"

#include <stdbool.h>

typedef enum {
    EVENT_TICK,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_KEY,
    EVENT_ALT /* Alt pressed and released on its own */
} EventType;

typedef struct {
    EventType type;
    int x, y; /* mouse position on screen, for every event */
    Key key;
} Event;

/* Something drawn over the main window, such as a menu or a dialog. */
typedef struct Layer Layer;
struct Layer {
    Rect rect;
    void (*paint)(Layer *layer, Canvas *canvas);
    bool dirty;
    Layer *next; /* the one above */
};

typedef void (*PaintBackground)(Canvas *canvas);

/*
 * `background` draws everything but the game: the desktop and the window
 * around it. The game frame, `frame`, is shown at `client`.
 */
bool ui_open(PaintBackground background, Framebuffer *frame, Rect client);
void ui_close(void);

void ui_set_tick_handler(void (*handler)(void));

void ui_wait_event(Event *event);

/* Mouse position on screen. */
void ui_mouse(int *x, int *y);

void ui_set_cursor(const Cursor *cursor);
const Cursor *ui_cursor(void);

/* The background changed within `rect`. */
void ui_invalidate(Rect rect);

/* The game frame was drawn again. */
void ui_frame_changed(void);

void ui_push_layer(Layer *layer);
void ui_remove_layer(Layer *layer);
void ui_layer_changed(Layer *layer);

/* Shows what changed right away, for work that keeps the loop busy. */
void ui_present(void);

#endif
