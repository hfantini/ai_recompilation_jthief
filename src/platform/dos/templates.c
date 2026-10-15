#include "templates.h"

#include <stdio.h>
#include <string.h>

#define ORDINAL_MARKER 0xFF
#define MENU_HEADER_SIZE 4
#define MENU_POPUP 0x0010
#define MENU_END 0x0080
#define ACCEL_LAST_ENTRY 0x80

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t position;
    bool ok;
} Reader;

static uint8_t read_u8(Reader *reader)
{
    if (!reader->ok || reader->position >= reader->size) {
        reader->ok = false;
        return 0;
    }
    return reader->data[reader->position++];
}

static uint16_t read_u16(Reader *reader)
{
    uint16_t low = read_u8(reader);
    return (uint16_t)(low | (read_u8(reader) << 8));
}

static uint32_t read_u32(Reader *reader)
{
    uint32_t low = read_u16(reader);
    return low | ((uint32_t)read_u16(reader) << 16);
}

static uint8_t peek_u8(const Reader *reader)
{
    return reader->ok && reader->position < reader->size ? reader->data[reader->position] : 0;
}

/* Copies the NUL-terminated string at the cursor, truncated to fit, and moves past it. */
static void read_string(Reader *reader, char *out, size_t size)
{
    const char *start = (const char *)reader->data + reader->position;

    out[0] = '\0';
    if (!reader->ok || reader->position >= reader->size
        || memchr(start, '\0', reader->size - reader->position) == NULL) {
        reader->ok = false;
        return;
    }
    snprintf(out, size, "%s", start);
    reader->position += strlen(start) + 1;
}

/* Menus and control texts are a string, or 0xFF followed by a 16-bit ordinal. */
static void read_name_or_ordinal(Reader *reader, char *out, size_t size)
{
    if (peek_u8(reader) == ORDINAL_MARKER) {
        read_u8(reader);
        read_u16(reader);
        out[0] = '\0';
        return;
    }
    read_string(reader, out, size);
}

static void read_dialog_item(Reader *reader, DialogItemTemplate *item)
{
    item->x = (int16_t)read_u16(reader);
    item->y = (int16_t)read_u16(reader);
    item->width = (int16_t)read_u16(reader);
    item->height = (int16_t)read_u16(reader);
    item->id = read_u16(reader);
    item->style = read_u32(reader);
    if (peek_u8(reader) & CLASS_BUTTON) {
        item->class_atom = read_u8(reader);
        item->class_name[0] = '\0';
    }
    else {
        item->class_atom = CLASS_CUSTOM;
        read_string(reader, item->class_name, sizeof(item->class_name));
    }
    read_name_or_ordinal(reader, item->text, sizeof(item->text));
    reader->position += read_u8(reader); /* creation data, unused by the original */
}

bool templates_read_dialog(const uint8_t *data, size_t size, DialogTemplate *out)
{
    Reader reader = { data, size, 0, true };
    char ignored[TEMPLATE_TEXT_SIZE];
    int count, i;

    memset(out, 0, sizeof(*out));
    out->style = read_u32(&reader);
    count = read_u8(&reader);
    out->x = (int16_t)read_u16(&reader);
    out->y = (int16_t)read_u16(&reader);
    out->width = (int16_t)read_u16(&reader);
    out->height = (int16_t)read_u16(&reader);
    read_name_or_ordinal(&reader, ignored, sizeof(ignored)); /* menu */
    read_string(&reader, ignored, sizeof(ignored));          /* class */
    read_string(&reader, out->caption, sizeof(out->caption));
    if (out->style & STYLE_SET_FONT) {
        /* The point size and face; the port picks its font from the flag alone. */
        read_u16(&reader);
        read_string(&reader, ignored, sizeof(ignored));
    }
    if (count > TEMPLATE_MAX_ITEMS) {
        return false;
    }
    for (i = 0; i < count && reader.ok; i++) {
        read_dialog_item(&reader, &out->items[i]);
    }
    out->item_count = count;
    return reader.ok;
}

static void read_popup_items(Reader *reader, MenuPopup *popup)
{
    for (;;) {
        uint16_t flags = read_u16(reader);
        MenuItem *item = &popup->items[popup->item_count];

        if (!reader->ok || (flags & MENU_POPUP) || popup->item_count >= MENU_MAX_ITEMS) {
            /* The original's popups hold no nested popups. */
            reader->ok = false;
            return;
        }
        item->command = read_u16(reader);
        read_string(reader, item->text, sizeof(item->text));
        popup->item_count++;
        if (flags & MENU_END) {
            return;
        }
    }
}

bool templates_read_menu(const uint8_t *data, size_t size, MenuTemplate *out)
{
    Reader reader = { data, size, MENU_HEADER_SIZE, size >= MENU_HEADER_SIZE };

    memset(out, 0, sizeof(*out));
    while (reader.ok && out->popup_count < MENU_MAX_POPUPS) {
        uint16_t flags = read_u16(&reader);
        MenuPopup *popup = &out->popups[out->popup_count];

        if (!(flags & MENU_POPUP)) {
            return false;
        }
        read_string(&reader, popup->text, sizeof(popup->text));
        read_popup_items(&reader, popup);
        out->popup_count++;
        if (flags & MENU_END) {
            break;
        }
    }
    return reader.ok;
}

bool templates_read_accelerators(const uint8_t *data, size_t size, AcceleratorTable *out)
{
    Reader reader = { data, size, 0, true };

    memset(out, 0, sizeof(*out));
    while (out->count < ACCELERATORS_MAX) {
        uint8_t flags = read_u8(&reader);
        Accelerator *entry = &out->entries[out->count];

        entry->flags = (uint8_t)(flags & ~ACCEL_LAST_ENTRY);
        entry->key = read_u16(&reader);
        entry->command = read_u16(&reader);
        if (!reader.ok) {
            return false;
        }
        out->count++;
        if (flags & ACCEL_LAST_ENTRY) {
            break;
        }
    }
    return true;
}
