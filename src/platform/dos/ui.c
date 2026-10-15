#include "ui.h"

#include "vga.h"

#include <dpmi.h>
#include <stdlib.h>
#include <string.h>

static Canvas canvas;

static PaintBackground paint_background;
static Framebuffer *frame;
/* The frame as last shown, to convert only the pixels that changed. */
static Framebuffer shown_frame;
static Rect client;
static bool frame_changed;

static Layer *layers;
static Rect damage;
static bool canvas_changed;
static DirtyRows dirty;

static const Cursor *cursor;
static int mouse_x, mouse_y;
static bool mouse_pressed;
static bool cursor_changed;
static Rect cursor_area;

static void (*tick_handler)(void);
static bool in_tick_handler;
static uint32_t last_tick;

/* Alt alone opens the menu bar, so its press is remembered until its release. */
static bool alt_was_held;
static bool alt_alone;

static Rect screen_rect(void)
{
    return rect_make(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
}

bool ui_open(PaintBackground background, Framebuffer *game_frame, Rect client_rect)
{
    paint_background = background;
    frame = game_frame;
    client = client_rect;
    if (!fb_create(&shown_frame, frame->width, frame->height)) {
        return false;
    }
    canvas_reset_clip(&canvas);
    damage = screen_rect();
    dirty_clear(&dirty);
    last_tick = clock_ticks();
    vga_enter();
    /* Drivers set themselves up for the video mode when reset after it. */
    mouse_open(CANVAS_WIDTH, CANVAS_HEIGHT);
    mouse_read(&mouse_x, &mouse_y, &mouse_pressed);
    return true;
}

void ui_close(void)
{
    speaker_silence();
    vga_leave();
    fb_destroy(&shown_frame);
}

void ui_set_tick_handler(void (*handler)(void))
{
    tick_handler = handler;
}

void ui_mouse(int *x, int *y)
{
    *x = mouse_x;
    *y = mouse_y;
}

void ui_set_cursor(const Cursor *new_cursor)
{
    if (new_cursor != cursor) {
        cursor = new_cursor;
        cursor_changed = true;
    }
}

const Cursor *ui_cursor(void)
{
    return cursor;
}

void ui_invalidate(Rect rect)
{
    damage = rect_union(damage, rect_intersect(rect, screen_rect()));
}

void ui_frame_changed(void)
{
    frame_changed = true;
}

void ui_push_layer(Layer *layer)
{
    Layer **top = &layers;

    while (*top != NULL) {
        top = &(*top)->next;
    }
    layer->next = NULL;
    layer->dirty = true;
    *top = layer;
}

void ui_remove_layer(Layer *layer)
{
    Layer **link = &layers;

    while (*link != NULL && *link != layer) {
        link = &(*link)->next;
    }
    if (*link != NULL) {
        *link = layer->next;
        ui_invalidate(layer->rect);
    }
}

void ui_layer_changed(Layer *layer)
{
    layer->dirty = true;
}

/* Converts the whole frame within `area`, which is in screen coordinates. */
static void show_frame_area(Rect area)
{
    Rect source = rect_offset(rect_intersect(area, client), -client.left, -client.top);
    int row;

    if (rect_empty(source)) {
        return;
    }
    canvas_blit_rgb(&canvas, frame, source, client.left + source.left, client.top + source.top);
    for (row = source.top; row < source.bottom; row++) {
        size_t offset = (size_t)row * (size_t)frame->width + (size_t)source.left;
        memcpy(shown_frame.pixels + offset, frame->pixels + offset,
               (size_t)(source.right - source.left) * sizeof(*frame->pixels));
    }
}

/* Converts the pixels of the frame that changed; returns where, on screen. */
static Rect show_frame_changes(void)
{
    Rect changed = { 0, 0, 0, 0 };
    int row;

    for (row = 0; row < frame->height; row++) {
        const uint32_t *now = frame->pixels + (size_t)row * (size_t)frame->width;
        const uint32_t *before = shown_frame.pixels + (size_t)row * (size_t)frame->width;
        int first = 0, last = frame->width - 1;

        if (memcmp(now, before, (size_t)frame->width * sizeof(*now)) == 0) {
            continue;
        }
        while (now[first] == before[first]) {
            first++;
        }
        while (now[last] == before[last]) {
            last--;
        }
        dirty_add(&dirty, rect_make(client.left + first, client.top + row, last - first + 1, 1));
        changed = rect_union(changed, rect_make(client.left + first, client.top + row,
                                                last - first + 1, 1));
    }
    if (!rect_empty(changed)) {
        show_frame_area(changed);
    }
    return changed;
}

static void compose(void)
{
    Rect changed = damage;
    Layer *layer;

    if (!rect_empty(damage)) {
        dirty_add(&dirty, damage);
        canvas_set_clip(&canvas, damage);
        paint_background(&canvas);
        show_frame_area(damage);
        canvas_reset_clip(&canvas);
        damage = rect_make(0, 0, 0, 0);
        canvas_changed = true;
    }
    if (frame_changed) {
        changed = rect_union(changed, show_frame_changes());
        frame_changed = false;
        canvas_changed = true;
    }
    for (layer = layers; layer != NULL; layer = layer->next) {
        if (layer->dirty || !rect_empty(rect_intersect(layer->rect, changed))) {
            layer->paint(layer, &canvas);
            dirty_add(&dirty, layer->rect);
            layer->dirty = false;
            changed = rect_union(changed, layer->rect);
            canvas_changed = true;
        }
    }
}

void ui_present(void)
{
    CursorBackground saved;

    compose();
    if (!canvas_changed && !cursor_changed) {
        return;
    }
    /* Where the cursor was must be shown again, and where it is now. */
    dirty_add(&dirty, cursor_area);
    if (cursor != NULL) {
        cursor_draw(cursor, &canvas, mouse_x, mouse_y, &saved);
        cursor_area = saved.area;
        dirty_add(&dirty, cursor_area);
    }
    vga_present(&canvas, &dirty);
    if (cursor != NULL) {
        cursor_erase(&canvas, &saved);
    }
    dirty_clear(&dirty);
    canvas_changed = false;
    cursor_changed = false;
}

/* Late ticks are merged, and a stall is not caught up, like WM_TIMER. */
static bool poll_tick(void)
{
    uint32_t now = clock_ticks();

    if (now == last_tick) {
        return false;
    }
    last_tick = now;
    if (tick_handler != NULL && !in_tick_handler) {
        in_tick_handler = true;
        tick_handler();
        in_tick_handler = false;
    }
    return true;
}

static bool poll_mouse(Event *event)
{
    int x, y;
    bool pressed;

    mouse_read(&x, &y, &pressed);
    if (x != mouse_x || y != mouse_y) {
        mouse_x = x;
        mouse_y = y;
        cursor_changed = true;
        event->type = EVENT_MOUSE_MOVE;
        return true;
    }
    if (pressed != mouse_pressed) {
        mouse_pressed = pressed;
        alt_alone = false;
        event->type = pressed ? EVENT_MOUSE_DOWN : EVENT_MOUSE_UP;
        return true;
    }
    return false;
}

static bool poll_keyboard(Event *event)
{
    bool alt_held = keyboard_alt_held();
    bool alt_released = alt_was_held && !alt_held;

    if (alt_held && !alt_was_held) {
        alt_alone = true;
    }
    alt_was_held = alt_held;
    if (keyboard_read(&event->key)) {
        alt_alone = false;
        event->type = EVENT_KEY;
        return true;
    }
    if (alt_released && alt_alone) {
        alt_alone = false;
        event->type = EVENT_ALT;
        return true;
    }
    return false;
}

void ui_wait_event(Event *event)
{
    memset(event, 0, sizeof(*event));
    for (;;) {
        speaker_update();
        if (poll_tick()) {
            event->type = EVENT_TICK;
            break;
        }
        if (poll_mouse(event) || poll_keyboard(event)) {
            break;
        }
        ui_present();
        /* Lets multitasking hosts, such as Windows, run something else meanwhile. */
        __dpmi_yield();
    }
    event->x = mouse_x;
    event->y = mouse_y;
}
