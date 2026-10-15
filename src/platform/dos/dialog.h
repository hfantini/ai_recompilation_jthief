#ifndef JT_DOS_DIALOG_H
#define JT_DOS_DIALOG_H

/*
 * Modal dialogs in the Windows 3.1 style, laid out from the original's own
 * templates the way the dialog manager did: dialog units scaled by the
 * font's average character width and height. The game clock keeps running
 * while one is open, as in the original.
 */

#include "dib.h"
#include "font.h"
#include "templates.h"
#include "ui.h"

#include <stdbool.h>
#include <stdint.h>

#define DIALOG_TEXT_SIZE 256
#define DIALOG_MAX_CONTROLS TEMPLATE_MAX_ITEMS

/* Standard command ids. */
enum {
    ID_OK = 1,
    ID_CANCEL = 2,
    ID_YES = 6,
    ID_NO = 7
};

typedef enum {
    CONTROL_STATIC,
    CONTROL_PUSH_BUTTON,
    CONTROL_CHECKBOX,
    CONTROL_RADIO,
    CONTROL_GROUP_BOX,
    CONTROL_EDIT,
    CONTROL_IMAGE
} ControlKind;

typedef struct {
    ControlKind kind;
    Rect rect;
    uint16_t id;
    uint32_t style;
    char text[DIALOG_TEXT_SIZE];
    bool enabled;
    bool checked;
    bool automatic;   /* auto check boxes and radio buttons check themselves */
    bool is_default;  /* the default push button */
    size_t max_length;
    size_t caret;
    Dib image;
    bool has_image;
} Control;

/* A font and the dialog base units it gives. */
typedef struct {
    const Font *font;
    int unit_x, unit_y;
} DialogFont;

typedef struct Dialog Dialog;

/* Returns true to close the dialog with `id` as its result. */
typedef bool (*DialogCommand)(Dialog *dialog, uint16_t id);

struct Dialog {
    Layer layer; /* first, so the layer leads to the dialog */
    Rect client;
    char caption[DIALOG_TEXT_SIZE];
    bool has_caption;
    bool active;
    const Font *font;
    const Font *caption_font;
    int control_count;
    Control controls[DIALOG_MAX_CONTROLS];
    int focus;   /* index of the focused control, or -1 */
    int pressed; /* control held down by the mouse, or -1 */
    bool pressed_inside;
    DialogCommand on_command;
    void *context;
    uint32_t timeout_ticks; /* closes with ID_OK after so many clock ticks; 0 never */
    bool done;
    int result;
};

/*
 * `system` lays out templates without a font of their own, `small` those
 * asking for an 8-point face. Positions are relative to `owner`, the main
 * window's client area, as Windows placed dialogs.
 */
bool dialog_from_template(Dialog *dialog, const DialogTemplate *dialog_template,
                          const DialogFont *system, const DialogFont *small, Rect owner);

Control *dialog_control(Dialog *dialog, uint16_t id);
void dialog_set_text(Dialog *dialog, uint16_t id, const char *text);
bool dialog_is_checked(Dialog *dialog, uint16_t id);
void dialog_check(Dialog *dialog, uint16_t id, bool checked);
void dialog_enable(Dialog *dialog, uint16_t id, bool enabled);

/* Runs the dialog until it closes; returns its result. */
int dialog_run(Dialog *dialog);

/* Called as the main window loses the focus to a dialog and gets it back. */
void dialog_set_owner_focus_handler(void (*handler)(bool focused));

typedef enum {
    MESSAGE_OK,
    MESSAGE_YES_NO
} MessageButtons;

/* Like MessageBox without an icon; returns ID_OK, ID_YES or ID_NO. */
int message_box(const DialogFont *system, const char *text, const char *caption,
                MessageButtons buttons);

#endif
