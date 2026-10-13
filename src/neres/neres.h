#ifndef JT_NERES_H
#define JT_NERES_H

/*
 * Read-only access to the resources of a 16-bit Windows NE executable.
 *
 * Modern Windows cannot load NE modules (not even as data files), so the
 * resource table is parsed directly from the file bytes.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Predefined resource type ids (RT_* in windows.h). */
enum {
    NE_RT_CURSOR = 1,
    NE_RT_BITMAP = 2,
    NE_RT_ICON = 3,
    NE_RT_MENU = 4,
    NE_RT_DIALOG = 5,
    NE_RT_STRING = 6,
    NE_RT_ACCELERATOR = 9,
    NE_RT_RCDATA = 10,
    NE_RT_GROUP_CURSOR = 12,
    NE_RT_GROUP_ICON = 14
};

typedef enum {
    NE_OK = 0,
    NE_ERR_INVALID_ARGUMENT,
    NE_ERR_NOT_NE,
    NE_ERR_MALFORMED,
    NE_ERR_NOT_FOUND
} NeStatus;

/* A view into an image buffer; valid while the image buffer lives. */
typedef struct {
    const uint8_t *data;
    size_t size;
} NeBlob;

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t header_offset;
    size_t resource_table_offset;
} NeImage;

typedef struct {
    uint16_t type_id;       /* 0 when the type is named */
    char type_name[256];    /* empty when the type is an integer id */
    uint16_t name_id;       /* 0 when the resource is named */
    char name[256];         /* empty when the resource is an integer id */
    NeBlob blob;
} NeResource;

/* Called for each resource. Return false to stop iterating. */
typedef bool (*NeVisitResource)(const NeResource *resource, void *context);

/* Validates the headers. The image borrows `data`; it does not copy it. */
NeStatus ne_image_open(NeImage *image, const uint8_t *data, size_t size);

NeStatus ne_image_for_each_resource(const NeImage *image, NeVisitResource visit, void *context);

/* Name comparison is case-insensitive, like FindResource. */
NeStatus ne_image_find_named(const NeImage *image, uint16_t type_id, const char *name, NeBlob *out);

NeStatus ne_image_find_id(const NeImage *image, uint16_t type_id, uint16_t name_id, NeBlob *out);

const char *ne_status_message(NeStatus status);

#endif
