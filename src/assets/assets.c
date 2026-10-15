#include "assets.h"

#include "fs.h"
#include "original.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Returns the index of the directory holding the original, or -1. */
static int find_original(const char *const *dirs, size_t dir_count, char *path, size_t capacity)
{
    size_t i;

    for (i = 0; i < dir_count; i++) {
        if (dirs[i] != NULL && original_find_in_dir(dirs[i], path, capacity)) {
            return (int)i;
        }
    }
    return -1;
}

/* Takes ownership of `data`; `assets` must be zeroed. */
static AssetsStatus open_image(Assets *assets, uint8_t *data, size_t size)
{
    assets->exe_data = data;
    assets->exe_size = size;
    if (ne_image_open(&assets->image, data, size) != NE_OK
        || ne_image_auto_data_segment(&assets->image, &assets->data_segment) != NE_OK) {
        assets_close(assets);
        return ASSETS_ERR_MALFORMED;
    }
    return ASSETS_OK;
}

AssetsStatus assets_open(Assets *assets, const char *const *dirs, size_t dir_count)
{
    char path[FS_MAX_PATH];
    uint8_t *data;
    size_t size;
    AssetsStatus status;
    int found;

    memset(assets, 0, sizeof(*assets));
    found = find_original(dirs, dir_count, path, sizeof(path));
    if (found < 0 || strlen(dirs[found]) >= sizeof(assets->dir)) {
        return ASSETS_ERR_NOT_FOUND;
    }
    if (!fs_read_file(path, &data, &size)) {
        return ASSETS_ERR_UNREADABLE;
    }
    status = open_image(assets, data, size);
    if (status == ASSETS_OK) {
        strcpy(assets->dir, dirs[found]);
    }
    return status;
}

AssetsStatus assets_open_memory(Assets *assets, uint8_t *data, size_t size)
{
    memset(assets, 0, sizeof(*assets));
    if (!original_is_supported_data(data, size)) {
        free(data);
        return ASSETS_ERR_NOT_FOUND;
    }
    return open_image(assets, data, size);
}

const char *assets_text(const Assets *assets, AssetsText text)
{
    size_t offset = (size_t)text;
    const uint8_t *data = assets->data_segment.data;

    if (offset >= assets->data_segment.size
        || memchr(data + offset, '\0', assets->data_segment.size - offset) == NULL) {
        return "";
    }
    return (const char *)data + offset;
}

bool assets_resource_named(const Assets *assets, uint16_t type, const char *name, NeBlob *out)
{
    return ne_image_find_named(&assets->image, type, name, out) == NE_OK;
}

bool assets_resource_id(const Assets *assets, uint16_t type, uint16_t id, NeBlob *out)
{
    return ne_image_find_id(&assets->image, type, id, out) == NE_OK;
}

void assets_close(Assets *assets)
{
    free(assets->exe_data);
    memset(assets, 0, sizeof(*assets));
}

bool assets_bitmap(const Assets *assets, const char *name, Dib *out)
{
    NeBlob blob;

    return ne_image_find_named(&assets->image, NE_RT_BITMAP, name, &blob) == NE_OK
        && dib_parse(blob.data, blob.size, out);
}

/*
 * Cursor and icon groups share a layout: a 6-byte header, then 14-byte
 * entries ending in the id of the image resource. The first image is used.
 */
static bool find_group_image(const Assets *assets, uint16_t group_type, uint16_t image_type,
                             const char *name, NeBlob *out)
{
    enum { GROUP_HEADER_SIZE = 6, GROUP_ENTRY_SIZE = 14, ENTRY_ID_OFFSET = 12 };
    NeBlob group;
    uint16_t count, id;

    if (ne_image_find_named(&assets->image, group_type, name, &group) != NE_OK
        || group.size < GROUP_HEADER_SIZE + GROUP_ENTRY_SIZE) {
        return false;
    }
    count = (uint16_t)(group.data[4] | (group.data[5] << 8));
    if (count == 0) {
        return false;
    }
    id = (uint16_t)(group.data[GROUP_HEADER_SIZE + ENTRY_ID_OFFSET]
                    | (group.data[GROUP_HEADER_SIZE + ENTRY_ID_OFFSET + 1] << 8));
    return ne_image_find_id(&assets->image, image_type, id, out) == NE_OK;
}

bool assets_cursor(const Assets *assets, const char *name, NeBlob *out)
{
    return find_group_image(assets, NE_RT_GROUP_CURSOR, NE_RT_CURSOR, name, out);
}

bool assets_icon(const Assets *assets, const char *name, NeBlob *out)
{
    return find_group_image(assets, NE_RT_GROUP_ICON, NE_RT_ICON, name, out);
}

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t position;
    bool ok;
} TemplateReader;

static void skip(TemplateReader *reader, size_t count)
{
    if (!reader->ok || count > reader->size - reader->position) {
        reader->ok = false;
        return;
    }
    reader->position += count;
}

static uint8_t read_byte(TemplateReader *reader)
{
    if (!reader->ok || reader->position >= reader->size) {
        reader->ok = false;
        return 0;
    }
    return reader->data[reader->position++];
}

static const char *read_string(TemplateReader *reader)
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

/* Menus and control texts are a string, or 0xFF followed by a 16-bit ordinal. */
static const char *read_name_or_ordinal(TemplateReader *reader)
{
    enum { ORDINAL_MARKER = 0xFF };

    if (reader->ok && reader->position < reader->size
        && reader->data[reader->position] == ORDINAL_MARKER) {
        skip(reader, 3);
        return "";
    }
    return read_string(reader);
}

/* Some paragraphs of the original are wrapped across two controls. */
static bool ends_sentence(const char *text, size_t length)
{
    return length == 0 || strchr(".!?:", text[length - 1]) != NULL;
}

static void append_paragraph_text(char *out, size_t size, const char *text)
{
    size_t used = strlen(out);
    const char *separator = used == 0 ? "" : ends_sentence(out, used) ? "\n\n" : " ";
    size_t length = strlen(separator) + strlen(text);

    if (length >= size - used) {
        length = size - used - 1;
    }
    snprintf(out + used, length + 1, "%s%s", separator, text);
}

bool assets_dialog_text(const Assets *assets, const char *name, char *out, size_t size)
{
    enum {
        DS_SETFONT = 0x40,
        PREDEFINED_CLASS_FLAG = 0x80,
        STATIC_CLASS = 0x82,
        ITEM_GEOMETRY_SIZE = 14 /* x, y, cx, cy, id and a 32-bit style */
    };
    TemplateReader reader;
    NeBlob blob;
    uint8_t style, count, i;

    if (size == 0 || !assets_resource_named(assets, NE_RT_DIALOG, name, &blob)) {
        return false;
    }
    out[0] = '\0';
    reader.data = blob.data;
    reader.size = blob.size;
    reader.position = 0;
    reader.ok = true;

    style = read_byte(&reader); /* the low byte of the style holds DS_SETFONT */
    skip(&reader, 3);
    count = read_byte(&reader);
    skip(&reader, 8); /* x, y, cx, cy */
    read_name_or_ordinal(&reader); /* menu */
    read_string(&reader);          /* class */
    read_string(&reader);          /* caption */
    if (style & DS_SETFONT) {
        skip(&reader, 2);
        read_string(&reader);
    }

    for (i = 0; i < count && reader.ok; i++) {
        bool is_static;
        const char *text;

        skip(&reader, ITEM_GEOMETRY_SIZE);
        if (reader.ok && reader.position < reader.size
            && (reader.data[reader.position] & PREDEFINED_CLASS_FLAG)) {
            is_static = read_byte(&reader) == STATIC_CLASS;
        }
        else {
            is_static = false;
            read_string(&reader);
        }
        text = read_name_or_ordinal(&reader);
        skip(&reader, read_byte(&reader)); /* creation data */
        if (reader.ok && is_static && text[0] != '\0') {
            append_paragraph_text(out, size, text);
        }
    }
    return reader.ok;
}
