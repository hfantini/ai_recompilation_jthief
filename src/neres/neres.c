#include "neres.h"

#include <ctype.h>
#include <string.h>

#define MZ_HEADER_SIZE 0x40
#define MZ_NE_OFFSET_FIELD 0x3C
#define NE_HEADER_SIZE 0x40
#define NE_RESOURCE_TABLE_FIELD 0x24
#define NE_RESIDENT_NAMES_FIELD 0x26
#define TYPE_INFO_SIZE 8
#define NAME_INFO_SIZE 12
#define INTEGER_ID_FLAG 0x8000

static bool read_u16(const NeImage *image, size_t offset, uint16_t *value)
{
    if (offset > image->size || image->size - offset < 2) {
        return false;
    }
    *value = (uint16_t)(image->data[offset] | (image->data[offset + 1] << 8));
    return true;
}

static bool read_u32(const NeImage *image, size_t offset, uint32_t *value)
{
    uint16_t low, high;

    if (!read_u16(image, offset, &low) || !read_u16(image, offset + 2, &high)) {
        return false;
    }
    *value = ((uint32_t)high << 16) | low;
    return true;
}

/* Copies a length-prefixed string stored at `table + relative_offset`. */
static bool read_pascal_string(const NeImage *image, size_t table, uint16_t relative_offset, char out[256])
{
    size_t offset = table + relative_offset;
    uint8_t length;

    if (offset >= image->size) {
        return false;
    }
    length = image->data[offset];
    if (image->size - offset - 1 < length) {
        return false;
    }
    memcpy(out, image->data + offset + 1, length);
    out[length] = '\0';
    return true;
}

/* Integer ids have the high bit set; otherwise the value points at a name. */
static bool read_identifier(const NeImage *image, uint16_t raw, uint16_t *id, char name[256])
{
    if (raw & INTEGER_ID_FLAG) {
        *id = raw & (uint16_t)~INTEGER_ID_FLAG;
        name[0] = '\0';
        return true;
    }
    *id = 0;
    return read_pascal_string(image, image->resource_table_offset, raw, name);
}

static bool equals_ignore_case(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

NeStatus ne_image_open(NeImage *image, const uint8_t *data, size_t size)
{
    uint32_t header_offset;
    uint16_t resource_table;
    uint16_t resident_names;

    if (image == NULL || data == NULL) {
        return NE_ERR_INVALID_ARGUMENT;
    }
    image->data = data;
    image->size = size;

    if (size < MZ_HEADER_SIZE || data[0] != 'M' || data[1] != 'Z') {
        return NE_ERR_NOT_NE;
    }
    if (!read_u32(image, MZ_NE_OFFSET_FIELD, &header_offset)
        || header_offset > size || size - header_offset < NE_HEADER_SIZE) {
        return NE_ERR_NOT_NE;
    }
    if (data[header_offset] != 'N' || data[header_offset + 1] != 'E') {
        return NE_ERR_NOT_NE;
    }
    image->header_offset = header_offset;

    if (!read_u16(image, header_offset + NE_RESOURCE_TABLE_FIELD, &resource_table)
        || !read_u16(image, header_offset + NE_RESIDENT_NAMES_FIELD, &resident_names)) {
        return NE_ERR_MALFORMED;
    }
    /* An empty resource table is marked by both offsets being equal. */
    image->resource_table_offset = resource_table == resident_names ? 0 : header_offset + resource_table;
    return NE_OK;
}

NeStatus ne_image_for_each_resource(const NeImage *image, NeVisitResource visit, void *context)
{
    NeResource resource;
    uint16_t alignment_shift;
    size_t cursor;

    if (image == NULL || visit == NULL) {
        return NE_ERR_INVALID_ARGUMENT;
    }
    if (image->resource_table_offset == 0) {
        return NE_OK;
    }
    if (!read_u16(image, image->resource_table_offset, &alignment_shift) || alignment_shift > 16) {
        return NE_ERR_MALFORMED;
    }

    cursor = image->resource_table_offset + 2;
    for (;;) {
        uint16_t raw_type, count, index;

        if (!read_u16(image, cursor, &raw_type)) {
            return NE_ERR_MALFORMED;
        }
        if (raw_type == 0) {
            return NE_OK;
        }
        if (!read_u16(image, cursor + 2, &count)
            || !read_identifier(image, raw_type, &resource.type_id, resource.type_name)) {
            return NE_ERR_MALFORMED;
        }
        cursor += TYPE_INFO_SIZE;

        for (index = 0; index < count; index++, cursor += NAME_INFO_SIZE) {
            uint16_t sector, length, raw_name;
            size_t offset, size;

            if (!read_u16(image, cursor, &sector)
                || !read_u16(image, cursor + 2, &length)
                || !read_u16(image, cursor + 6, &raw_name)
                || !read_identifier(image, raw_name, &resource.name_id, resource.name)) {
                return NE_ERR_MALFORMED;
            }
            offset = (size_t)sector << alignment_shift;
            size = (size_t)length << alignment_shift;
            if (offset > image->size || image->size - offset < size) {
                return NE_ERR_MALFORMED;
            }
            resource.blob.data = image->data + offset;
            resource.blob.size = size;

            if (!visit(&resource, context)) {
                return NE_OK;
            }
        }
    }
}

typedef struct {
    uint16_t type_id;
    const char *name;
    uint16_t name_id;
    NeBlob *out;
    bool found;
} FindQuery;

static bool match_resource(const NeResource *resource, void *context)
{
    FindQuery *query = context;

    if (resource->type_id != query->type_id) {
        return true;
    }
    if (query->name != NULL
        ? resource->name_id == 0 && equals_ignore_case(resource->name, query->name)
        : resource->name_id == query->name_id) {
        *query->out = resource->blob;
        query->found = true;
        return false;
    }
    return true;
}

static NeStatus find_resource(const NeImage *image, FindQuery *query)
{
    NeStatus status;

    if (image == NULL || query->out == NULL) {
        return NE_ERR_INVALID_ARGUMENT;
    }
    status = ne_image_for_each_resource(image, match_resource, query);
    if (status != NE_OK) {
        return status;
    }
    return query->found ? NE_OK : NE_ERR_NOT_FOUND;
}

NeStatus ne_image_find_named(const NeImage *image, uint16_t type_id, const char *name, NeBlob *out)
{
    FindQuery query = { type_id, name, 0, out, false };

    if (name == NULL) {
        return NE_ERR_INVALID_ARGUMENT;
    }
    return find_resource(image, &query);
}

NeStatus ne_image_find_id(const NeImage *image, uint16_t type_id, uint16_t name_id, NeBlob *out)
{
    FindQuery query = { type_id, NULL, name_id, out, false };

    return find_resource(image, &query);
}

NeStatus ne_image_auto_data_segment(const NeImage *image, NeBlob *out)
{
    enum {
        AUTO_DATA_FIELD = 0x0E,
        SEGMENT_COUNT_FIELD = 0x1C,
        SEGMENT_TABLE_FIELD = 0x22,
        ALIGNMENT_SHIFT_FIELD = 0x32,
        SEGMENT_ENTRY_SIZE = 8
    };
    uint16_t number, count, table, shift, sector, length;
    size_t entry, offset, size;

    if (image == NULL || out == NULL) {
        return NE_ERR_INVALID_ARGUMENT;
    }
    if (!read_u16(image, image->header_offset + AUTO_DATA_FIELD, &number)
        || !read_u16(image, image->header_offset + SEGMENT_COUNT_FIELD, &count)
        || !read_u16(image, image->header_offset + SEGMENT_TABLE_FIELD, &table)
        || !read_u16(image, image->header_offset + ALIGNMENT_SHIFT_FIELD, &shift)) {
        return NE_ERR_MALFORMED;
    }
    /* Segment numbers are 1-based; 0 means the module has no data segment. */
    if (number == 0 || number > count || shift > 16) {
        return NE_ERR_NOT_FOUND;
    }
    entry = image->header_offset + table + (size_t)(number - 1) * SEGMENT_ENTRY_SIZE;
    if (!read_u16(image, entry, &sector) || !read_u16(image, entry + 2, &length)) {
        return NE_ERR_MALFORMED;
    }
    if (sector == 0) {
        return NE_ERR_NOT_FOUND;
    }
    offset = (size_t)sector << shift;
    size = length == 0 ? 0x10000 : length; /* a length of 0 means 64 KB */
    if (offset > image->size || image->size - offset < size) {
        return NE_ERR_MALFORMED;
    }
    out->data = image->data + offset;
    out->size = size;
    return NE_OK;
}

const char *ne_status_message(NeStatus status)
{
    switch (status) {
    case NE_OK: return "ok";
    case NE_ERR_INVALID_ARGUMENT: return "invalid argument";
    case NE_ERR_NOT_NE: return "not a 16-bit NE executable";
    case NE_ERR_MALFORMED: return "malformed NE executable";
    case NE_ERR_NOT_FOUND: return "resource not found";
    }
    return "unknown error";
}
