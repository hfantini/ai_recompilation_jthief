#ifndef JT_DOS_TEMPLATES_H
#define JT_DOS_TEMPLATES_H

/*
 * The original's Windows 3.x dialog, menu and accelerator resources, read
 * into plain structures for the dialogs and menus this port draws itself.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEMPLATE_TEXT_SIZE 160
#define TEMPLATE_MAX_ITEMS 24
#define MENU_MAX_POPUPS 4
#define MENU_MAX_ITEMS 12
#define MENU_TEXT_SIZE 48
#define ACCELERATORS_MAX 16

/* Predefined control classes, as their atoms. */
enum {
    CLASS_BUTTON = 0x80,
    CLASS_EDIT = 0x81,
    CLASS_STATIC = 0x82,
    CLASS_CUSTOM = 0 /* named class, such as the About box's SPLASH */
};

/* Window and control styles the original's templates use. */
#define STYLE_DISABLED 0x08000000UL
#define STYLE_CAPTION 0x00C00000UL
#define STYLE_GROUP 0x00020000UL
#define STYLE_TABSTOP 0x00010000UL
#define STYLE_SET_FONT 0x40UL
#define STYLE_TYPE_MASK 0x0FUL

/* The low bits of a button's style. */
enum {
    BUTTON_PUSH = 0,
    BUTTON_DEFAULT_PUSH = 1,
    BUTTON_CHECKBOX = 2,
    BUTTON_AUTO_CHECKBOX = 3,
    BUTTON_RADIO = 4,
    BUTTON_GROUP_BOX = 7,
    BUTTON_AUTO_RADIO = 9
};

/* The low bits of a static control's style. */
enum {
    STATIC_LEFT = 0,
    STATIC_CENTER = 1,
    STATIC_RIGHT = 2
};

typedef struct {
    int x, y, width, height; /* dialog units */
    uint16_t id;
    uint32_t style;
    uint8_t class_atom;
    char class_name[16];
    char text[TEMPLATE_TEXT_SIZE];
} DialogItemTemplate;

typedef struct {
    uint32_t style;
    int x, y, width, height; /* dialog units */
    char caption[TEMPLATE_TEXT_SIZE];
    int item_count;
    DialogItemTemplate items[TEMPLATE_MAX_ITEMS];
} DialogTemplate;

typedef struct {
    char text[MENU_TEXT_SIZE]; /* empty for a separator */
    uint16_t command;
} MenuItem;

typedef struct {
    char text[MENU_TEXT_SIZE];
    int item_count;
    MenuItem items[MENU_MAX_ITEMS];
} MenuPopup;

/* A menu bar of popups, which is all the original's menu holds. */
typedef struct {
    int popup_count;
    MenuPopup popups[MENU_MAX_POPUPS];
} MenuTemplate;

/* Accelerator flags, as stored. */
enum {
    ACCEL_VIRTUAL_KEY = 0x01,
    ACCEL_SHIFT = 0x04,
    ACCEL_CONTROL = 0x08,
    ACCEL_ALT = 0x10
};

typedef struct {
    uint8_t flags;
    uint16_t key;
    uint16_t command;
} Accelerator;

typedef struct {
    int count;
    Accelerator entries[ACCELERATORS_MAX];
} AcceleratorTable;

bool templates_read_dialog(const uint8_t *data, size_t size, DialogTemplate *out);
bool templates_read_menu(const uint8_t *data, size_t size, MenuTemplate *out);
bool templates_read_accelerators(const uint8_t *data, size_t size, AcceleratorTable *out);

#endif
