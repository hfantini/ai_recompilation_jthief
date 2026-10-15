#include "dialog.h"

#include "window.h"

#include <stdio.h>
#include <string.h>

/* A 1-pixel black line, a band in the caption color, another black line. */
#define FRAME_WIDTH 4
#define BOX_SIZE 12
#define BOX_TEXT_GAP 4
#define EDIT_MARGIN 3
#define CARET_BLINK_TICKS 9
#define MAX_LINES 24

/* Message box layout, in pixels. */
#define MESSAGE_MARGIN 16
#define MESSAGE_MAX_TEXT_WIDTH 400
#define MESSAGE_BUTTON_WIDTH 76
#define MESSAGE_BUTTON_HEIGHT 28
#define MESSAGE_BUTTON_GAP 16

static const char *const RADIO[BOX_SIZE] = {
    "    XXXX    ",
    "  XX....XX  ",
    " X........X ",
    " X........X ",
    "X..........X",
    "X..........X",
    "X..........X",
    "X..........X",
    " X........X ",
    " X........X ",
    "  XX....XX  ",
    "    XXXX    ",
};

/* How many dialogs are open: the main window is inactive while any is. */
static int open_dialogs;
static Dialog *top_dialog;
static void (*owner_focus_handler)(bool focused);

/* ---- Layout ------------------------------------------------------------- */

static int to_pixels_x(const DialogFont *font, int units)
{
    return units * font->unit_x / 4;
}

static int to_pixels_y(const DialogFont *font, int units)
{
    return units * font->unit_y / 8;
}

static Rect outer_rect(const Dialog *dialog)
{
    Rect outer = rect_inflate(dialog->client, FRAME_WIDTH);

    if (dialog->has_caption) {
        outer.top -= CAPTION_HEIGHT;
    }
    return outer;
}

/* Moves the dialog as needed so it is entirely on the screen. */
static void keep_on_screen(Dialog *dialog)
{
    Rect outer = outer_rect(dialog);
    int dx = 0, dy = 0;

    if (outer.right > CANVAS_WIDTH) {
        dx = CANVAS_WIDTH - outer.right;
    }
    if (outer.left + dx < 0) {
        dx = -outer.left;
    }
    if (outer.bottom > CANVAS_HEIGHT) {
        dy = CANVAS_HEIGHT - outer.bottom;
    }
    if (outer.top + dy < 0) {
        dy = -outer.top;
    }
    dialog->client = rect_offset(dialog->client, dx, dy);
}

static ControlKind button_kind(uint32_t style, bool *automatic, bool *is_default)
{
    uint32_t type = style & STYLE_TYPE_MASK;

    *automatic = type == BUTTON_AUTO_CHECKBOX || type == BUTTON_AUTO_RADIO;
    *is_default = type == BUTTON_DEFAULT_PUSH;
    switch (type) {
    case BUTTON_CHECKBOX:
    case BUTTON_AUTO_CHECKBOX:
        return CONTROL_CHECKBOX;
    case BUTTON_RADIO:
    case BUTTON_AUTO_RADIO:
        return CONTROL_RADIO;
    case BUTTON_GROUP_BOX:
        return CONTROL_GROUP_BOX;
    }
    return CONTROL_PUSH_BUTTON;
}

static void add_control(Dialog *dialog, const DialogItemTemplate *item, const DialogFont *font)
{
    Control *control = &dialog->controls[dialog->control_count++];

    memset(control, 0, sizeof(*control));
    control->rect = rect_make(dialog->client.left + to_pixels_x(font, item->x),
                              dialog->client.top + to_pixels_y(font, item->y),
                              to_pixels_x(font, item->width), to_pixels_y(font, item->height));
    control->id = item->id;
    control->style = item->style;
    control->enabled = !(item->style & STYLE_DISABLED);
    snprintf(control->text, sizeof(control->text), "%s", item->text);
    switch (item->class_atom) {
    case CLASS_BUTTON:
        control->kind = button_kind(item->style, &control->automatic, &control->is_default);
        break;
    case CLASS_EDIT:
        control->kind = CONTROL_EDIT;
        control->max_length = DIALOG_TEXT_SIZE - 1;
        control->caret = strlen(control->text);
        break;
    case CLASS_STATIC:
        control->kind = CONTROL_STATIC;
        break;
    default:
        /* Custom classes, such as SPLASH, show a picture the caller supplies. */
        control->kind = CONTROL_IMAGE;
        break;
    }
}

static bool can_focus(const Control *control)
{
    return control->enabled && control->kind != CONTROL_STATIC && control->kind != CONTROL_GROUP_BOX
        && control->kind != CONTROL_IMAGE;
}

static int first_tab_stop(const Dialog *dialog)
{
    int i;

    for (i = 0; i < dialog->control_count; i++) {
        if (can_focus(&dialog->controls[i]) && (dialog->controls[i].style & STYLE_TABSTOP)) {
            return i;
        }
    }
    return -1;
}

static void paint_dialog(Layer *layer, Canvas *canvas);

static void init_dialog(Dialog *dialog, const DialogFont *font)
{
    dialog->layer.paint = paint_dialog;
    dialog->font = font->font;
    dialog->active = true;
    dialog->focus = -1;
    dialog->pressed = -1;
}

static void lay_out(Dialog *dialog, const DialogTemplate *dialog_template, const DialogFont *font,
                    Rect owner)
{
    int i;

    dialog->client = rect_make(owner.left + to_pixels_x(font, dialog_template->x),
                               owner.top + to_pixels_y(font, dialog_template->y),
                               to_pixels_x(font, dialog_template->width),
                               to_pixels_y(font, dialog_template->height));
    keep_on_screen(dialog);
    dialog->control_count = 0;
    for (i = 0; i < dialog_template->item_count && i < DIALOG_MAX_CONTROLS; i++) {
        add_control(dialog, &dialog_template->items[i], font);
    }
}

static bool text_fits(const Dialog *dialog);

/*
 * Windows sized dialog units after the font so that texts fit their
 * controls. The ROM fonts are wider for their size than the faces the
 * original was laid out with, so the horizontal unit grows until they fit.
 */
static void lay_out_fitting(Dialog *dialog, const DialogTemplate *dialog_template,
                            const DialogFont *font, Rect owner)
{
    DialogFont fitted = *font;

    lay_out(dialog, dialog_template, &fitted, owner);
    while (!text_fits(dialog)) {
        fitted.unit_x++;
        if (to_pixels_x(&fitted, dialog_template->width) + 2 * FRAME_WIDTH > CANVAS_WIDTH) {
            fitted.unit_x--;
            lay_out(dialog, dialog_template, &fitted, owner);
            return;
        }
        lay_out(dialog, dialog_template, &fitted, owner);
    }
}

bool dialog_from_template(Dialog *dialog, const DialogTemplate *dialog_template,
                          const DialogFont *system, const DialogFont *small, Rect owner)
{
    const DialogFont *font = (dialog_template->style & STYLE_SET_FONT) ? small : system;

    memset(dialog, 0, sizeof(*dialog));
    init_dialog(dialog, font);
    dialog->caption_font = system->font;
    dialog->has_caption = (dialog_template->style & STYLE_CAPTION) == STYLE_CAPTION;
    snprintf(dialog->caption, sizeof(dialog->caption), "%s", dialog_template->caption);
    lay_out_fitting(dialog, dialog_template, font, owner);
    dialog->focus = first_tab_stop(dialog);
    return true;
}

Control *dialog_control(Dialog *dialog, uint16_t id)
{
    int i;

    for (i = 0; i < dialog->control_count; i++) {
        if (dialog->controls[i].id == id) {
            return &dialog->controls[i];
        }
    }
    return NULL;
}

void dialog_set_text(Dialog *dialog, uint16_t id, const char *text)
{
    Control *control = dialog_control(dialog, id);

    if (control != NULL) {
        snprintf(control->text, sizeof(control->text), "%s", text);
        control->caret = strlen(control->text);
        ui_layer_changed(&dialog->layer);
    }
}

bool dialog_is_checked(Dialog *dialog, uint16_t id)
{
    Control *control = dialog_control(dialog, id);
    return control != NULL && control->checked;
}

void dialog_check(Dialog *dialog, uint16_t id, bool checked)
{
    Control *control = dialog_control(dialog, id);

    if (control != NULL) {
        control->checked = checked;
        ui_layer_changed(&dialog->layer);
    }
}

void dialog_enable(Dialog *dialog, uint16_t id, bool enabled)
{
    Control *control = dialog_control(dialog, id);

    if (control == NULL) {
        return;
    }
    control->enabled = enabled;
    if (!enabled && dialog->focus >= 0 && &dialog->controls[dialog->focus] == control) {
        dialog->focus = first_tab_stop(dialog);
    }
    ui_layer_changed(&dialog->layer);
}

/* ---- Text --------------------------------------------------------------- */

typedef struct {
    const char *start;
    size_t length;
} Line;

static int label_width(const Font *font, const char *text, size_t length)
{
    char buffer[DIALOG_TEXT_SIZE];

    if (length >= sizeof(buffer)) {
        length = sizeof(buffer) - 1;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';
    return font_label_width(font, buffer);
}

static size_t word_end(const char *text)
{
    size_t length = 0;

    while (text[length] != '\0' && text[length] != ' ' && text[length] != '\r'
           && text[length] != '\n') {
        length++;
    }
    return length;
}

/* Breaks text into lines no wider than `width`, at spaces and line breaks. */
static int wrap(const Font *font, const char *text, int width, Line *lines, int max_lines)
{
    int count = 0;

    while (*text != '\0' && count < max_lines) {
        const char *start = text;
        size_t length = 0;

        for (;;) {
            size_t spaces = 0, word;

            while (start[length + spaces] == ' ') {
                spaces++;
            }
            word = word_end(start + length + spaces);
            if (word == 0 || (length > 0
                              && label_width(font, start, length + spaces + word) > width)) {
                break;
            }
            length += spaces + word;
        }
        lines[count].start = start;
        lines[count].length = length;
        count++;
        text = start + length;
        while (*text == ' ') {
            text++;
        }
        if (*text == '\r') {
            text++;
        }
        if (*text == '\n') {
            text++;
        }
        else if (length == 0 && *text != '\0') {
            text++; /* a lone character that is no word at all */
        }
    }
    return count;
}

static void draw_line(const Font *font, Canvas *canvas, int x, int y, const Line *line,
                      uint8_t color)
{
    char buffer[DIALOG_TEXT_SIZE];
    size_t length = line->length < sizeof(buffer) ? line->length : sizeof(buffer) - 1;

    memcpy(buffer, line->start, length);
    buffer[length] = '\0';
    font_draw_label(font, canvas, x, y, buffer, color);
}

/* Paints wrapped text; returns the bounds of what was drawn. */
static Rect paint_text(const Font *font, Canvas *canvas, Rect rect, const char *text, int align,
                       uint8_t color)
{
    Line lines[MAX_LINES];
    int count = wrap(font, text, rect.right - rect.left, lines, MAX_LINES);
    Rect bounds = { rect.left, rect.top, rect.left, rect.top };
    int i;

    for (i = 0; i < count; i++) {
        int width = label_width(font, lines[i].start, lines[i].length);
        int x = rect.left;
        int y = rect.top + i * font->height;

        if (align == STATIC_CENTER) {
            x = (rect.left + rect.right - width) / 2;
        }
        else if (align == STATIC_RIGHT) {
            x = rect.right - width;
        }
        draw_line(font, canvas, x, y, &lines[i], color);
        bounds = rect_union(bounds, rect_make(x, y, width, font->height));
    }
    return bounds;
}

static bool control_text_fits(const Font *font, const Control *control)
{
    Line lines[MAX_LINES];
    int width = control->rect.right - control->rect.left;
    int height = control->rect.bottom - control->rect.top;
    int count;

    switch (control->kind) {
    case CONTROL_STATIC:
        count = wrap(font, control->text, width, lines, MAX_LINES);
        return count * font->height <= (height > font->height ? height : font->height);
    case CONTROL_PUSH_BUTTON:
        return font_label_width(font, control->text) <= width - 4;
    case CONTROL_CHECKBOX:
    case CONTROL_RADIO:
        return font_label_width(font, control->text) <= width - BOX_SIZE - BOX_TEXT_GAP;
    case CONTROL_GROUP_BOX:
        return font_label_width(font, control->text) <= width - 16;
    case CONTROL_EDIT:
    case CONTROL_IMAGE:
        break;
    }
    return true;
}

static bool text_fits(const Dialog *dialog)
{
    int i;

    for (i = 0; i < dialog->control_count; i++) {
        if (!control_text_fits(dialog->font, &dialog->controls[i])) {
            return false;
        }
    }
    return true;
}

/* ---- Painting ----------------------------------------------------------- */

static uint8_t text_color(const Control *control)
{
    return control->enabled ? SCHEME_TEXT : SCHEME_GRAY_TEXT;
}

static bool shows_as_default(const Dialog *dialog, const Control *control)
{
    const Control *focused = dialog->focus >= 0 ? &dialog->controls[dialog->focus] : NULL;

    if (focused != NULL && focused->kind == CONTROL_PUSH_BUTTON) {
        return focused == control;
    }
    return control->is_default;
}

static void paint_push_button(const Dialog *dialog, const Control *control, Canvas *canvas,
                              bool focused, bool pressed)
{
    Rect rect = control->rect;
    Rect face = rect_inflate(rect, shows_as_default(dialog, control) ? -2 : -1);
    int width = font_label_width(dialog->font, control->text);
    int x = (rect.left + rect.right - width) / 2 + (pressed ? 1 : 0);
    int y = (rect.top + rect.bottom - dialog->font->height) / 2 + (pressed ? 1 : 0);

    /* A black outline with its corners cut off, doubled on the default button. */
    canvas_frame(canvas, rect, SCHEME_FRAME);
    canvas_pixel(canvas, rect.left, rect.top, SCHEME_WINDOW);
    canvas_pixel(canvas, rect.right - 1, rect.top, SCHEME_WINDOW);
    canvas_pixel(canvas, rect.left, rect.bottom - 1, SCHEME_WINDOW);
    canvas_pixel(canvas, rect.right - 1, rect.bottom - 1, SCHEME_WINDOW);
    if (shows_as_default(dialog, control)) {
        canvas_frame(canvas, rect_inflate(rect, -1), SCHEME_FRAME);
    }
    canvas_fill(canvas, face, SCHEME_BUTTON_FACE);
    if (pressed) {
        canvas_fill(canvas, rect_make(face.left, face.top, face.right - face.left, 1),
                    SCHEME_BUTTON_SHADOW);
        canvas_fill(canvas, rect_make(face.left, face.top, 1, face.bottom - face.top),
                    SCHEME_BUTTON_SHADOW);
    }
    else {
        canvas_bevel(canvas, face, 1, SCHEME_BUTTON_HIGHLIGHT, SCHEME_BUTTON_SHADOW);
        canvas_fill(canvas, rect_make(face.left + 1, face.bottom - 2, face.right - face.left - 2, 1),
                    SCHEME_BUTTON_SHADOW);
        canvas_fill(canvas, rect_make(face.right - 2, face.top + 1, 1, face.bottom - face.top - 2),
                    SCHEME_BUTTON_SHADOW);
    }
    font_draw_label(dialog->font, canvas, x, y, control->text, text_color(control));
    if (focused) {
        canvas_dotted_frame(canvas, rect_make(x - 2, y, width + 4, dialog->font->height),
                            SCHEME_TEXT);
    }
}

static void paint_box(Canvas *canvas, int left, int top, bool checked)
{
    Rect box = rect_make(left, top, BOX_SIZE, BOX_SIZE);
    int i;

    canvas_fill(canvas, box, SCHEME_WINDOW);
    canvas_frame(canvas, box, SCHEME_FRAME);
    if (checked) {
        for (i = 2; i < BOX_SIZE - 2; i++) {
            canvas_pixel(canvas, left + i, top + i, SCHEME_TEXT);
            canvas_pixel(canvas, left + BOX_SIZE - 1 - i, top + i, SCHEME_TEXT);
        }
    }
}

static void paint_radio(Canvas *canvas, int left, int top, bool checked)
{
    int x, y;

    for (y = 0; y < BOX_SIZE; y++) {
        for (x = 0; x < BOX_SIZE; x++) {
            char cell = RADIO[y][x];
            bool dot = checked && x >= 4 && x < 8 && y >= 4 && y < 8;

            if (cell == 'X' || dot) {
                canvas_pixel(canvas, left + x, top + y, SCHEME_TEXT);
            }
            else if (cell == '.') {
                canvas_pixel(canvas, left + x, top + y, SCHEME_WINDOW);
            }
        }
    }
}

static void paint_check_control(const Dialog *dialog, const Control *control, Canvas *canvas,
                                bool focused)
{
    Rect rect = control->rect;
    int box_top = (rect.top + rect.bottom - BOX_SIZE) / 2;
    Rect text = { rect.left + BOX_SIZE + BOX_TEXT_GAP,
                  (rect.top + rect.bottom - dialog->font->height) / 2, rect.right, rect.bottom };
    Rect drawn;

    if (control->kind == CONTROL_RADIO) {
        paint_radio(canvas, rect.left, box_top, control->checked);
    }
    else {
        paint_box(canvas, rect.left, box_top, control->checked);
    }
    drawn = paint_text(dialog->font, canvas, text, control->text, STATIC_LEFT, text_color(control));
    if (focused) {
        canvas_dotted_frame(canvas, rect_inflate(drawn, 1), SCHEME_TEXT);
    }
}

static void paint_group_box(const Dialog *dialog, const Control *control, Canvas *canvas)
{
    Rect rect = control->rect;
    int middle = rect.top + dialog->font->height / 2;
    int text_left = rect.left + 8;
    int width = font_label_width(dialog->font, control->text);

    canvas_frame(canvas, rect_make(rect.left, middle, rect.right - rect.left, rect.bottom - middle),
                 SCHEME_FRAME);
    canvas_fill(canvas, rect_make(text_left - 2, middle, width + 4, 1), SCHEME_WINDOW);
    font_draw_label(dialog->font, canvas, text_left, rect.top, control->text, text_color(control));
}

/* The first character shown, so that the caret stays inside the field. */
static size_t edit_scroll(const Font *font, const Control *control, int width)
{
    size_t start = 0;

    while (start < control->caret
           && font_width(font, control->text + start, control->caret - start) > width) {
        start++;
    }
    return start;
}

static void paint_edit(const Dialog *dialog, const Control *control, Canvas *canvas, bool focused)
{
    const Font *font = dialog->font;
    Rect rect = control->rect;
    int width = rect.right - rect.left - 2 * EDIT_MARGIN;
    size_t start = edit_scroll(font, control, width);
    int text_top = (rect.top + rect.bottom - font->height) / 2;
    size_t length = strlen(control->text + start);
    Rect inside = rect_inflate(rect, -1);

    canvas_fill(canvas, rect, SCHEME_WINDOW);
    canvas_frame(canvas, rect, SCHEME_FRAME);
    canvas_set_clip(canvas, inside);
    font_draw(font, canvas, rect.left + EDIT_MARGIN, text_top, control->text + start, length,
              SCHEME_TEXT);
    if (focused && (clock_ticks() / CARET_BLINK_TICKS) % 2 == 0) {
        int caret_x = rect.left + EDIT_MARGIN + font_width(font, control->text + start,
                                                           control->caret - start);
        canvas_fill(canvas, rect_make(caret_x, text_top, 1, font->height), SCHEME_TEXT);
    }
    canvas_reset_clip(canvas);
}

static void paint_image(const Control *control, Canvas *canvas)
{
    if (control->has_image) {
        canvas_set_clip(canvas, control->rect);
        canvas_blit_dib(canvas, &control->image, control->rect.left, control->rect.top);
        canvas_reset_clip(canvas);
    }
}

static void paint_control(const Dialog *dialog, int index, Canvas *canvas)
{
    const Control *control = &dialog->controls[index];
    bool focused = index == dialog->focus && dialog->active;
    bool pressed = index == dialog->pressed && dialog->pressed_inside;

    /* Like Windows, a control paints within its rectangle; focus marks may touch its edge. */
    canvas_set_clip(canvas, rect_intersect(rect_inflate(control->rect, 1), dialog->client));
    switch (control->kind) {
    case CONTROL_STATIC:
        paint_text(dialog->font, canvas, control->rect, control->text,
                   (int)(control->style & STYLE_TYPE_MASK), text_color(control));
        break;
    case CONTROL_PUSH_BUTTON:
        paint_push_button(dialog, control, canvas, focused, pressed);
        break;
    case CONTROL_CHECKBOX:
    case CONTROL_RADIO:
        paint_check_control(dialog, control, canvas, focused);
        break;
    case CONTROL_GROUP_BOX:
        paint_group_box(dialog, control, canvas);
        break;
    case CONTROL_EDIT:
        paint_edit(dialog, control, canvas, focused);
        break;
    case CONTROL_IMAGE:
        paint_image(control, canvas);
        break;
    }
    canvas_reset_clip(canvas);
}

static void paint_dialog(Layer *layer, Canvas *canvas)
{
    Dialog *dialog = (Dialog *)layer;
    Rect outer = outer_rect(dialog);
    int i;

    canvas_frame(canvas, outer, SCHEME_FRAME);
    canvas_fill(canvas, rect_inflate(outer, -1),
                dialog->active ? SCHEME_ACTIVE_CAPTION : SCHEME_INACTIVE_CAPTION);
    canvas_frame(canvas, rect_inflate(outer, -(FRAME_WIDTH - 1)), SCHEME_FRAME);
    if (dialog->has_caption) {
        Rect caption = rect_make(dialog->client.left, outer.top + FRAME_WIDTH,
                                 dialog->client.right - dialog->client.left, CAPTION_HEIGHT);
        window_paint_caption(canvas, caption, dialog->caption_font, dialog->caption,
                             dialog->active);
    }
    canvas_fill(canvas, dialog->client, SCHEME_WINDOW);
    for (i = 0; i < dialog->control_count; i++) {
        paint_control(dialog, i, canvas);
    }
}

/* ---- Behavior ----------------------------------------------------------- */

static void changed(Dialog *dialog)
{
    ui_layer_changed(&dialog->layer);
}

static void end_dialog(Dialog *dialog, int result)
{
    dialog->done = true;
    dialog->result = result;
}

static void command(Dialog *dialog, uint16_t id)
{
    bool close = dialog->on_command != NULL ? dialog->on_command(dialog, id)
                                            : id == ID_OK || id == ID_CANCEL;
    if (close && !dialog->done) {
        end_dialog(dialog, id);
    }
}

static bool starts_group(const Control *control)
{
    return (control->style & STYLE_GROUP) != 0;
}

/* The controls of the group holding `index`: from a WS_GROUP control to the next. */
static void group_range(const Dialog *dialog, int index, int *first, int *last)
{
    *first = index;
    while (*first > 0 && !starts_group(&dialog->controls[*first])) {
        (*first)--;
    }
    *last = index + 1;
    while (*last < dialog->control_count && !starts_group(&dialog->controls[*last])) {
        (*last)++;
    }
}

static void check_radio(Dialog *dialog, int index)
{
    int first, last, i;

    group_range(dialog, index, &first, &last);
    for (i = first; i < last; i++) {
        if (dialog->controls[i].kind == CONTROL_RADIO) {
            dialog->controls[i].checked = i == index;
        }
    }
}

static void set_focus(Dialog *dialog, int index)
{
    if (index != dialog->focus) {
        dialog->focus = index;
        changed(dialog);
    }
}

static void activate(Dialog *dialog, int index)
{
    Control *control = &dialog->controls[index];

    if (!control->enabled) {
        return;
    }
    if (control->automatic && control->kind == CONTROL_CHECKBOX) {
        control->checked = !control->checked;
    }
    else if (control->automatic && control->kind == CONTROL_RADIO) {
        check_radio(dialog, index);
    }
    changed(dialog);
    command(dialog, control->id);
}

static int next_tab_stop(const Dialog *dialog, int step)
{
    int count = dialog->control_count, index = dialog->focus, i;

    for (i = 0; i < count; i++) {
        index = (index + step + count) % count;
        const Control *control = &dialog->controls[index];
        if (can_focus(control) && (control->style & STYLE_TABSTOP)) {
            return index;
        }
    }
    return dialog->focus;
}

/* Arrow keys move within the focused control's group, checking radio buttons on the way. */
static void move_in_group(Dialog *dialog, int step)
{
    int first, last, index = dialog->focus, i;
    int size;

    if (index < 0) {
        return;
    }
    group_range(dialog, index, &first, &last);
    size = last - first;
    for (i = 0; i < size; i++) {
        index = first + (index - first + step + size) % size;
        if (can_focus(&dialog->controls[index])) {
            break;
        }
    }
    set_focus(dialog, index);
    if (dialog->controls[index].kind == CONTROL_RADIO && dialog->controls[index].automatic) {
        check_radio(dialog, index);
        changed(dialog);
    }
}

static int default_button(const Dialog *dialog)
{
    int i;

    for (i = 0; i < dialog->control_count; i++) {
        if (dialog->controls[i].kind == CONTROL_PUSH_BUTTON && dialog->controls[i].is_default) {
            return i;
        }
    }
    return -1;
}

static void press_enter(Dialog *dialog)
{
    int index = dialog->focus;

    if (index < 0 || dialog->controls[index].kind != CONTROL_PUSH_BUTTON) {
        index = default_button(dialog);
    }
    if (index >= 0) {
        activate(dialog, index);
    }
    else {
        command(dialog, ID_OK);
    }
}

static int control_for(const Dialog *dialog, char letter)
{
    int i;

    for (i = 0; i < dialog->control_count; i++) {
        const Control *control = &dialog->controls[i];
        if (can_focus(control) && control->kind != CONTROL_EDIT
            && font_label_mnemonic(control->text) == letter) {
            return i;
        }
    }
    return -1;
}

/* Keys that move the caret or delete; with Num Lock on, the keypad types digits instead. */
static bool edit_command_key(Control *edit, const Key *key)
{
    size_t length = strlen(edit->text);

    if (key->scan == SCAN_BACKSPACE) {
        if (edit->caret > 0) {
            memmove(edit->text + edit->caret - 1, edit->text + edit->caret, length - edit->caret + 1);
            edit->caret--;
        }
        return true;
    }
    if (key->ascii != 0) {
        return false;
    }
    switch (key->scan) {
    case SCAN_DELETE:
        if (edit->caret < length) {
            memmove(edit->text + edit->caret, edit->text + edit->caret + 1, length - edit->caret);
        }
        return true;
    case SCAN_LEFT:
        edit->caret -= edit->caret > 0 ? 1 : 0;
        return true;
    case SCAN_RIGHT:
        edit->caret += edit->caret < length ? 1 : 0;
        return true;
    case SCAN_HOME:
        edit->caret = 0;
        return true;
    case SCAN_END:
        edit->caret = length;
        return true;
    }
    return false;
}

static bool edit_key(Dialog *dialog, Control *edit, const Key *key)
{
    size_t length = strlen(edit->text);
    char character;

    if (!edit_command_key(edit, key)) {
        character = key->alt || key->ctrl ? 0 : font_from_keyboard(key->ascii);
        if (character == 0) {
            return false;
        }
        if (length < edit->max_length) {
            memmove(edit->text + edit->caret + 1, edit->text + edit->caret, length - edit->caret + 1);
            edit->text[edit->caret++] = character;
        }
    }
    changed(dialog);
    return true;
}

static void on_key(Dialog *dialog, const Key *key)
{
    Control *focused = dialog->focus >= 0 ? &dialog->controls[dialog->focus] : NULL;
    char letter;
    int index;

    switch (key->scan) {
    case SCAN_ESCAPE:
        command(dialog, ID_CANCEL);
        return;
    case SCAN_ENTER:
    case SCAN_KEYPAD_ENTER:
        press_enter(dialog);
        return;
    case SCAN_TAB:
        set_focus(dialog, next_tab_stop(dialog, key->shift ? -1 : 1));
        return;
    }
    if (focused != NULL && focused->kind == CONTROL_EDIT && edit_key(dialog, focused, key)) {
        return;
    }
    switch (key->scan) {
    case SCAN_UP:
    case SCAN_LEFT:
        move_in_group(dialog, -1);
        return;
    case SCAN_DOWN:
    case SCAN_RIGHT:
        move_in_group(dialog, 1);
        return;
    case SCAN_SPACE:
        if (focused != NULL && focused->kind != CONTROL_EDIT) {
            activate(dialog, dialog->focus);
        }
        return;
    }
    letter = keyboard_letter(key);
    if (letter != 0 && (index = control_for(dialog, letter)) >= 0) {
        if (dialog->controls[index].kind != CONTROL_PUSH_BUTTON) {
            set_focus(dialog, index);
        }
        activate(dialog, index);
    }
}

static int control_at(const Dialog *dialog, int x, int y)
{
    int i;

    for (i = dialog->control_count - 1; i >= 0; i--) {
        const Control *control = &dialog->controls[i];
        if (can_focus(control) && rect_contains(control->rect, x, y)) {
            return i;
        }
    }
    return -1;
}

static void on_mouse_down(Dialog *dialog, int x, int y)
{
    int index = control_at(dialog, x, y);
    Control *control;

    if (index < 0) {
        return;
    }
    control = &dialog->controls[index];
    set_focus(dialog, index);
    if (control->kind == CONTROL_EDIT) {
        control->caret = strlen(control->text);
        changed(dialog);
        return;
    }
    dialog->pressed = index;
    dialog->pressed_inside = true;
    changed(dialog);
}

static void on_mouse_move(Dialog *dialog, int x, int y)
{
    bool inside;

    if (dialog->pressed < 0) {
        return;
    }
    inside = rect_contains(dialog->controls[dialog->pressed].rect, x, y);
    if (inside != dialog->pressed_inside) {
        dialog->pressed_inside = inside;
        changed(dialog);
    }
}

static void on_mouse_up(Dialog *dialog)
{
    int index = dialog->pressed;
    bool inside = dialog->pressed_inside;

    if (index < 0) {
        return;
    }
    dialog->pressed = -1;
    dialog->pressed_inside = false;
    changed(dialog);
    if (inside) {
        activate(dialog, index);
    }
}

static void on_tick(Dialog *dialog, uint32_t opened)
{
    if (dialog->timeout_ticks != 0 && clock_ticks() - opened >= dialog->timeout_ticks) {
        end_dialog(dialog, ID_OK);
    }
    if (dialog->focus >= 0 && dialog->controls[dialog->focus].kind == CONTROL_EDIT) {
        changed(dialog);
    }
}

static void deactivate_owner(Dialog *dialog, Dialog **owner)
{
    *owner = top_dialog;
    if (*owner != NULL) {
        (*owner)->active = false;
        changed(*owner);
    }
    top_dialog = dialog;
    if (open_dialogs++ == 0) {
        window_set_active(false);
        if (owner_focus_handler != NULL) {
            owner_focus_handler(false);
        }
    }
}

static void reactivate_owner(Dialog *owner)
{
    top_dialog = owner;
    if (owner != NULL) {
        owner->active = true;
        changed(owner);
    }
    if (--open_dialogs == 0) {
        window_set_active(true);
        if (owner_focus_handler != NULL) {
            owner_focus_handler(true);
        }
    }
}

void dialog_set_owner_focus_handler(void (*handler)(bool focused))
{
    owner_focus_handler = handler;
}

int dialog_run(Dialog *dialog)
{
    const Cursor *previous_cursor = ui_cursor();
    uint32_t opened = clock_ticks();
    static Cursor arrow;
    Dialog *owner;
    Event event;

    cursor_arrow(&arrow);
    deactivate_owner(dialog, &owner);
    dialog->layer.rect = outer_rect(dialog);
    ui_push_layer(&dialog->layer);
    while (!dialog->done) {
        ui_wait_event(&event);
        switch (event.type) {
        case EVENT_TICK:
            on_tick(dialog, opened);
            break;
        case EVENT_MOUSE_MOVE:
            ui_set_cursor(&arrow);
            on_mouse_move(dialog, event.x, event.y);
            break;
        case EVENT_MOUSE_DOWN:
            on_mouse_down(dialog, event.x, event.y);
            break;
        case EVENT_MOUSE_UP:
            on_mouse_up(dialog);
            break;
        case EVENT_KEY:
            on_key(dialog, &event.key);
            break;
        case EVENT_ALT:
            break;
        }
    }
    ui_remove_layer(&dialog->layer);
    reactivate_owner(owner);
    ui_set_cursor(previous_cursor);
    return dialog->result;
}

/* ---- Message boxes ------------------------------------------------------ */

static void add_button(Dialog *dialog, Rect rect, uint16_t id, const char *text, bool is_default)
{
    Control *control = &dialog->controls[dialog->control_count++];

    memset(control, 0, sizeof(*control));
    control->kind = CONTROL_PUSH_BUTTON;
    control->rect = rect;
    control->id = id;
    control->style = STYLE_TABSTOP | (is_default ? BUTTON_DEFAULT_PUSH : BUTTON_PUSH);
    control->enabled = true;
    control->is_default = is_default;
    snprintf(control->text, sizeof(control->text), "%s", text);
}

/* Without a Cancel button, Escape only closes a box that has a single OK. */
static bool message_command(Dialog *dialog, uint16_t id)
{
    if (id == ID_CANCEL) {
        if (dialog_control(dialog, ID_OK) != NULL) {
            dialog->done = true;
            dialog->result = ID_OK;
        }
        return false;
    }
    return true;
}

int message_box(const DialogFont *system, const char *text, const char *caption,
                MessageButtons buttons)
{
    static Dialog dialog;
    const Font *font = system->font;
    Line lines[MAX_LINES];
    int line_count = wrap(font, text, MESSAGE_MAX_TEXT_WIDTH, lines, MAX_LINES);
    int button_count = buttons == MESSAGE_YES_NO ? 2 : 1;
    int buttons_width =
        button_count * MESSAGE_BUTTON_WIDTH + (button_count - 1) * MESSAGE_BUTTON_GAP;
    int text_width = 0, width, height, i;
    Rect button;
    Control *message;

    for (i = 0; i < line_count; i++) {
        int line_width = label_width(font, lines[i].start, lines[i].length);
        text_width = line_width > text_width ? line_width : text_width;
    }
    width = (text_width > buttons_width ? text_width : buttons_width) + 2 * MESSAGE_MARGIN;
    height = MESSAGE_MARGIN + line_count * font->height + MESSAGE_MARGIN + MESSAGE_BUTTON_HEIGHT
           + MESSAGE_MARGIN / 2;

    memset(&dialog, 0, sizeof(dialog));
    init_dialog(&dialog, system);
    dialog.caption_font = font;
    dialog.has_caption = true;
    snprintf(dialog.caption, sizeof(dialog.caption), "%s", caption);
    dialog.client = rect_make((CANVAS_WIDTH - width) / 2,
                              (CANVAS_HEIGHT - height + CAPTION_HEIGHT) / 2, width, height);
    dialog.on_command = message_command;

    message = &dialog.controls[dialog.control_count++];
    message->kind = CONTROL_STATIC;
    message->rect = rect_make(dialog.client.left + MESSAGE_MARGIN, dialog.client.top + MESSAGE_MARGIN,
                              text_width, line_count * font->height);
    message->style = STYLE_GROUP | STATIC_LEFT;
    message->enabled = true;
    snprintf(message->text, sizeof(message->text), "%s", text);

    button = rect_make((dialog.client.left + dialog.client.right - buttons_width) / 2,
                       dialog.client.bottom - MESSAGE_MARGIN / 2 - MESSAGE_BUTTON_HEIGHT,
                       MESSAGE_BUTTON_WIDTH, MESSAGE_BUTTON_HEIGHT);
    if (buttons == MESSAGE_YES_NO) {
        add_button(&dialog, button, ID_YES, "&Yes", true);
        add_button(&dialog, rect_offset(button, MESSAGE_BUTTON_WIDTH + MESSAGE_BUTTON_GAP, 0),
                   ID_NO, "&No", false);
    }
    else {
        add_button(&dialog, button, ID_OK, "OK", true);
    }
    dialog.focus = first_tab_stop(&dialog);
    return dialog_run(&dialog);
}
