#include "win16res.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define WIN16_CODE_PAGE 1252
#define ORDINAL_MARKER 0xFF
#define PREDEFINED_CLASS_FLAG 0x80
#define MENU_HEADER_SIZE 4
#define ACCEL_LAST_ENTRY 0x80
#define MAX_ACCELERATORS 64

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t position;
    bool ok;
} Reader;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
    bool ok;
} Writer;

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

/* Returns the NUL-terminated string at the cursor and moves past it. */
static const char *read_string(Reader *reader)
{
    const char *start = (const char *)reader->data + reader->position;

    if (!reader->ok || reader->position >= reader->size
        || memchr(start, '\0', reader->size - reader->position) == NULL) {
        reader->ok = false;
        return "";
    }
    reader->position += strlen(start) + 1;
    return start;
}

static void write_bytes(Writer *writer, const void *bytes, size_t count)
{
    if (!writer->ok) {
        return;
    }
    if (writer->size + count > writer->capacity) {
        size_t capacity = writer->capacity * 2 + count;
        uint8_t *grown = realloc(writer->data, capacity);
        if (grown == NULL) {
            writer->ok = false;
            return;
        }
        writer->data = grown;
        writer->capacity = capacity;
    }
    memcpy(writer->data + writer->size, bytes, count);
    writer->size += count;
}

static void write_u16(Writer *writer, uint16_t value)
{
    write_bytes(writer, &value, sizeof(value));
}

static void write_u32(Writer *writer, uint32_t value)
{
    write_bytes(writer, &value, sizeof(value));
}

static void align_to_dword(Writer *writer)
{
    static const uint8_t zeros[3] = { 0 };
    write_bytes(writer, zeros, (4 - writer->size % 4) % 4);
}

/* Writes an ANSI string as UTF-16, terminator included. */
static void write_wide_string(Writer *writer, const char *text)
{
    int length = MultiByteToWideChar(WIN16_CODE_PAGE, 0, text, -1, NULL, 0);
    WCHAR *wide;

    if (length <= 0) {
        write_u16(writer, 0);
        return;
    }
    wide = malloc((size_t)length * sizeof(WCHAR));
    if (wide == NULL) {
        writer->ok = false;
        return;
    }
    MultiByteToWideChar(WIN16_CODE_PAGE, 0, text, -1, wide, length);
    write_bytes(writer, wide, (size_t)length * sizeof(WCHAR));
    free(wide);
}

/* A name or an 0xFF-prefixed ordinal, as dialog menus and control texts are stored. */
static void convert_name_or_ordinal(Reader *reader, Writer *writer)
{
    if (peek_u8(reader) == ORDINAL_MARKER) {
        read_u8(reader);
        write_u16(writer, 0xFFFF);
        write_u16(writer, read_u16(reader));
    }
    else {
        write_wide_string(writer, read_string(reader));
    }
}

static void *finish(Reader *reader, Writer *writer)
{
    if (!reader->ok || !writer->ok) {
        free(writer->data);
        return NULL;
    }
    return writer->data;
}

static void convert_dialog_item(Reader *reader, Writer *writer)
{
    int16_t x, y, cx, cy;
    uint16_t id;
    uint32_t style;
    uint8_t extra;

    x = (int16_t)read_u16(reader);
    y = (int16_t)read_u16(reader);
    cx = (int16_t)read_u16(reader);
    cy = (int16_t)read_u16(reader);
    id = read_u16(reader);
    style = read_u32(reader);

    align_to_dword(writer);
    write_u32(writer, style);
    write_u32(writer, 0); /* extended style */
    write_u16(writer, (uint16_t)x);
    write_u16(writer, (uint16_t)y);
    write_u16(writer, (uint16_t)cx);
    write_u16(writer, (uint16_t)cy);
    write_u16(writer, id);

    /* Predefined classes (0x80 button ... 0x85 combo box) keep their atoms. */
    if (peek_u8(reader) & PREDEFINED_CLASS_FLAG) {
        write_u16(writer, 0xFFFF);
        write_u16(writer, read_u8(reader));
    }
    else {
        write_wide_string(writer, read_string(reader));
    }
    convert_name_or_ordinal(reader, writer);

    /* Creation data is dropped; none of the original's controls use it. */
    extra = read_u8(reader);
    reader->position += extra;
    write_u16(writer, 0);
}

void *win16_dialog_to_win32(const uint8_t *data, size_t size)
{
    Reader reader = { data, size, 0, true };
    Writer writer = { NULL, 0, 0, true };
    uint32_t style;
    uint8_t count;
    int i;

    style = read_u32(&reader);
    count = read_u8(&reader);
    write_u32(&writer, style);
    write_u32(&writer, 0); /* extended style */
    write_u16(&writer, count);
    for (i = 0; i < 4; i++) {
        write_u16(&writer, read_u16(&reader)); /* x, y, cx, cy */
    }
    convert_name_or_ordinal(&reader, &writer);                 /* menu */
    write_wide_string(&writer, read_string(&reader));          /* class */
    write_wide_string(&writer, read_string(&reader));          /* caption */
    if (style & DS_SETFONT) {
        write_u16(&writer, read_u16(&reader));                 /* point size */
        write_wide_string(&writer, read_string(&reader));      /* face name */
    }
    for (i = 0; i < count && reader.ok; i++) {
        convert_dialog_item(&reader, &writer);
    }
    return finish(&reader, &writer);
}

/* Items until the one flagged MF_END; popups hold a nested list. */
static void convert_menu_items(Reader *reader, Writer *writer)
{
    for (;;) {
        uint16_t flags = read_u16(reader);

        if (!reader->ok) {
            return;
        }
        write_u16(writer, flags);
        if (!(flags & MF_POPUP)) {
            write_u16(writer, read_u16(reader));
        }
        write_wide_string(writer, read_string(reader));
        if (flags & MF_POPUP) {
            convert_menu_items(reader, writer);
        }
        if (flags & MF_END) {
            return;
        }
    }
}

void *win16_menu_to_win32(const uint8_t *data, size_t size)
{
    Reader reader = { data, size, 0, true };
    Writer writer = { NULL, 0, 0, true };

    /* The header (version and offset, both 0) is the same in both formats. */
    write_u32(&writer, read_u32(&reader));
    if (reader.ok && size >= MENU_HEADER_SIZE) {
        convert_menu_items(&reader, &writer);
    }
    return finish(&reader, &writer);
}

HACCEL win16_create_accelerators(const uint8_t *data, size_t size)
{
    Reader reader = { data, size, 0, true };
    ACCEL table[MAX_ACCELERATORS];
    int count = 0;

    /* Entries are flags, key and command; the flag values match Win32's. */
    while (count < MAX_ACCELERATORS) {
        uint8_t flags = read_u8(&reader);

        table[count].fVirt = (BYTE)(flags & ~ACCEL_LAST_ENTRY);
        table[count].key = read_u16(&reader);
        table[count].cmd = read_u16(&reader);
        if (!reader.ok) {
            return NULL;
        }
        count++;
        if (flags & ACCEL_LAST_ENTRY) {
            break;
        }
    }
    return CreateAcceleratorTable(table, count);
}
