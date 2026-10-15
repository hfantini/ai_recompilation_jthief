#include "menu.h"

#include "ui.h"
#include "window.h"

#include <string.h>

#define BAR_PADDING 8
#define POPUP_TEXT_LEFT 12
#define POPUP_TEXT_RIGHT 12
#define POPUP_ACCELERATOR_GAP 24
#define SEPARATOR_HEIGHT 9

typedef struct {
    Layer layer; /* first, so the layer leads to the popup */
    int popup;
    int item; /* highlighted item, or -1 */
} PopupLayer;

static const MenuTemplate *menu;
static const Font *font;
static int selected = -1;
static PopupLayer popup = { { { 0, 0, 0, 0 }, NULL, false, NULL }, -1, -1 };
static bool popup_shown;

static int item_height(const MenuItem *item)
{
    return item->text[0] == '\0' ? SEPARATOR_HEIGHT : font->height + 2;
}

static bool is_separator(const MenuItem *item)
{
    return item->text[0] == '\0';
}

/* The label before the tab, and the accelerator text after it. */
static void split_text(const MenuItem *item, char *label, size_t size, const char **accelerator)
{
    const char *tab = strchr(item->text, '\t');
    size_t length = tab != NULL ? (size_t)(tab - item->text) : strlen(item->text);

    if (length >= size) {
        length = size - 1;
    }
    memcpy(label, item->text, length);
    label[length] = '\0';
    *accelerator = tab != NULL ? tab + 1 : "";
}

static Rect bar_item_rect(int index)
{
    Rect bar = window_menu_bar();
    int left = bar.left, i;

    for (i = 0; i < index; i++) {
        left += font_label_width(font, menu->popups[i].text) + 2 * BAR_PADDING;
    }
    return rect_make(left, bar.top, font_label_width(font, menu->popups[index].text) + 2 * BAR_PADDING,
                     bar.bottom - bar.top);
}

static Rect popup_rect(int index)
{
    const MenuPopup *items = &menu->popups[index];
    Rect anchor = bar_item_rect(index);
    int width = 0, height = 2, i, left;

    for (i = 0; i < items->item_count; i++) {
        char label[MENU_TEXT_SIZE];
        const char *accelerator;
        int text_width;

        split_text(&items->items[i], label, sizeof(label), &accelerator);
        text_width = font_label_width(font, label);
        if (accelerator[0] != '\0') {
            text_width += POPUP_ACCELERATOR_GAP + font_width(font, accelerator, strlen(accelerator));
        }
        width = text_width > width ? text_width : width;
        height += item_height(&items->items[i]);
    }
    width += 2 + POPUP_TEXT_LEFT + POPUP_TEXT_RIGHT;
    left = anchor.left + width > CANVAS_WIDTH ? CANVAS_WIDTH - width : anchor.left;
    return rect_make(left, anchor.bottom + 1, width, height);
}

static Rect popup_item_rect(int index)
{
    const MenuPopup *items = &menu->popups[popup.popup];
    Rect rect = popup.layer.rect;
    int top = rect.top + 1, i;

    for (i = 0; i < index; i++) {
        top += item_height(&items->items[i]);
    }
    return rect_make(rect.left + 1, top, rect.right - rect.left - 2, item_height(&items->items[index]));
}

void menu_open(const MenuTemplate *template_menu, const Font *menu_font)
{
    menu = template_menu;
    font = menu_font;
}

void menu_paint_bar(Canvas *canvas, Rect bar)
{
    int i;

    canvas_fill(canvas, bar, SCHEME_WINDOW);
    for (i = 0; i < menu->popup_count; i++) {
        Rect item = bar_item_rect(i);
        bool highlighted = i == selected;

        if (highlighted) {
            canvas_fill(canvas, item, SCHEME_HIGHLIGHT);
        }
        font_draw_label(font, canvas, item.left + BAR_PADDING,
                        item.top + (item.bottom - item.top - font->height) / 2, menu->popups[i].text,
                        highlighted ? SCHEME_HIGHLIGHT_TEXT : SCHEME_TEXT);
    }
}

static void paint_popup(Layer *layer, Canvas *canvas)
{
    const MenuPopup *items = &menu->popups[popup.popup];
    int i;

    canvas_fill(canvas, layer->rect, SCHEME_WINDOW);
    canvas_frame(canvas, layer->rect, SCHEME_FRAME);
    for (i = 0; i < items->item_count; i++) {
        Rect rect = popup_item_rect(i);
        char label[MENU_TEXT_SIZE];
        const char *accelerator;
        uint8_t color = i == popup.item ? SCHEME_HIGHLIGHT_TEXT : SCHEME_TEXT;
        int text_top = rect.top + 1;

        if (is_separator(&items->items[i])) {
            canvas_fill(canvas, rect_make(rect.left, rect.top + SEPARATOR_HEIGHT / 2,
                                          rect.right - rect.left, 1),
                        SCHEME_FRAME);
            continue;
        }
        if (i == popup.item) {
            canvas_fill(canvas, rect, SCHEME_HIGHLIGHT);
        }
        split_text(&items->items[i], label, sizeof(label), &accelerator);
        font_draw_label(font, canvas, rect.left + POPUP_TEXT_LEFT, text_top, label, color);
        font_draw(font, canvas,
                  rect.right - POPUP_TEXT_RIGHT - font_width(font, accelerator, strlen(accelerator)),
                  text_top, accelerator, strlen(accelerator), color);
    }
}

int menu_bar_item_at(int x, int y)
{
    int i;

    for (i = 0; i < menu->popup_count; i++) {
        if (rect_contains(bar_item_rect(i), x, y)) {
            return i;
        }
    }
    return -1;
}

int menu_bar_item_for(char letter)
{
    int i;

    for (i = 0; i < menu->popup_count; i++) {
        if (letter != 0 && font_label_mnemonic(menu->popups[i].text) == letter) {
            return i;
        }
    }
    return -1;
}

static void select_bar_item(int item)
{
    if (item != selected) {
        selected = item;
        ui_invalidate(window_menu_bar());
    }
}

static void hide_popup(void)
{
    if (popup_shown) {
        ui_remove_layer(&popup.layer);
        popup_shown = false;
    }
}

static void show_popup(int index, int item)
{
    hide_popup();
    popup.popup = index;
    popup.item = item;
    popup.layer.rect = popup_rect(index);
    popup.layer.paint = paint_popup;
    ui_push_layer(&popup.layer);
    popup_shown = true;
}

static void highlight_item(int item)
{
    if (item != popup.item) {
        popup.item = item;
        ui_layer_changed(&popup.layer);
    }
}

/* The next selectable item from `item`, going `step` (1 or -1), wrapping around. */
static int next_item(int item, int step)
{
    const MenuPopup *items = &menu->popups[popup.popup];
    int i;

    for (i = 0; i < items->item_count; i++) {
        item = (item + step + items->item_count) % items->item_count;
        if (!is_separator(&items->items[item])) {
            return item;
        }
    }
    return -1;
}

static int popup_item_at(int x, int y)
{
    const MenuPopup *items;
    int i;

    if (!popup_shown || !rect_contains(popup.layer.rect, x, y)) {
        return -1;
    }
    items = &menu->popups[popup.popup];
    for (i = 0; i < items->item_count; i++) {
        if (rect_contains(popup_item_rect(i), x, y) && !is_separator(&items->items[i])) {
            return i;
        }
    }
    return -1;
}

static int command_of(int item)
{
    return menu->popups[popup.popup].items[item].command;
}

static int item_for(char letter)
{
    const MenuPopup *items = &menu->popups[popup.popup];
    int i;

    for (i = 0; i < items->item_count; i++) {
        if (letter != 0 && font_label_mnemonic(items->items[i].text) == letter) {
            return i;
        }
    }
    return -1;
}

static void open_bar_item(int item, bool highlight_first)
{
    select_bar_item(item);
    show_popup(item, -1);
    if (highlight_first) {
        highlight_item(next_item(-1, 1));
    }
}

typedef struct {
    bool done;
    int command;
    bool dragging;
} Tracking;

static void on_mouse_move(Tracking *tracking, int x, int y)
{
    int bar_item = menu_bar_item_at(x, y);

    if (!tracking->dragging && !popup_shown) {
        return;
    }
    if (bar_item >= 0 && bar_item != selected) {
        open_bar_item(bar_item, false);
    }
    else if (popup_shown && (tracking->dragging || rect_contains(popup.layer.rect, x, y))) {
        highlight_item(popup_item_at(x, y));
    }
}

static void on_mouse_down(Tracking *tracking, int x, int y)
{
    int bar_item = menu_bar_item_at(x, y);

    if (bar_item >= 0) {
        /* Clicking the open item again closes the menu. */
        if (bar_item == selected && popup_shown) {
            tracking->done = true;
            return;
        }
        open_bar_item(bar_item, false);
        tracking->dragging = true;
    }
    else if (popup_shown && rect_contains(popup.layer.rect, x, y)) {
        tracking->dragging = true;
    }
    else {
        tracking->done = true;
    }
}

static void on_mouse_up(Tracking *tracking, int x, int y)
{
    int item = popup_item_at(x, y);

    if (item >= 0) {
        tracking->command = command_of(item);
        tracking->done = true;
    }
    else if (tracking->dragging && menu_bar_item_at(x, y) < 0
             && !(popup_shown && rect_contains(popup.layer.rect, x, y))) {
        tracking->done = true;
    }
    tracking->dragging = false;
}

static void on_key(Tracking *tracking, const Key *key)
{
    char letter = keyboard_letter(key);
    int item;

    switch (key->scan) {
    case SCAN_ESCAPE:
        if (popup_shown) {
            hide_popup();
        }
        else {
            tracking->done = true;
        }
        return;
    case SCAN_F10:
        tracking->done = true;
        return;
    case SCAN_LEFT:
    case SCAN_RIGHT:
        item = (selected + (key->scan == SCAN_RIGHT ? 1 : -1) + menu->popup_count)
             % menu->popup_count;
        if (popup_shown) {
            open_bar_item(item, true);
        }
        else {
            select_bar_item(item);
        }
        return;
    case SCAN_DOWN:
    case SCAN_UP:
        if (!popup_shown) {
            open_bar_item(selected, true);
        }
        else {
            highlight_item(next_item(popup.item, key->scan == SCAN_DOWN ? 1 : -1));
        }
        return;
    case SCAN_ENTER:
    case SCAN_KEYPAD_ENTER:
        if (!popup_shown) {
            open_bar_item(selected, true);
        }
        else if (popup.item >= 0) {
            tracking->command = command_of(popup.item);
            tracking->done = true;
        }
        return;
    }
    if (popup_shown && (item = item_for(letter)) >= 0) {
        tracking->command = command_of(item);
        tracking->done = true;
    }
    else if ((item = menu_bar_item_for(letter)) >= 0) {
        open_bar_item(item, true);
    }
}

int menu_track(int item, bool open, bool by_mouse)
{
    Tracking tracking = { false, 0, by_mouse };
    const Cursor *previous_cursor = ui_cursor();
    static Cursor arrow;
    Event event;

    cursor_arrow(&arrow);
    ui_set_cursor(&arrow);
    select_bar_item(item);
    if (open) {
        open_bar_item(item, !by_mouse);
    }
    while (!tracking.done) {
        ui_wait_event(&event);
        switch (event.type) {
        case EVENT_MOUSE_MOVE:
            on_mouse_move(&tracking, event.x, event.y);
            break;
        case EVENT_MOUSE_DOWN:
            on_mouse_down(&tracking, event.x, event.y);
            break;
        case EVENT_MOUSE_UP:
            on_mouse_up(&tracking, event.x, event.y);
            break;
        case EVENT_KEY:
            on_key(&tracking, &event.key);
            break;
        case EVENT_ALT:
            tracking.done = true;
            break;
        case EVENT_TICK:
            break;
        }
    }
    hide_popup();
    select_bar_item(-1);
    ui_set_cursor(previous_cursor);
    return tracking.command;
}
