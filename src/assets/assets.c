#include "assets.h"

#include "fs.h"
#include "original.h"

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

AssetsStatus assets_open(Assets *assets, const char *const *dirs, size_t dir_count)
{
    char path[FS_MAX_PATH];
    int found;

    memset(assets, 0, sizeof(*assets));
    found = find_original(dirs, dir_count, path, sizeof(path));
    if (found < 0 || strlen(dirs[found]) >= sizeof(assets->dir)) {
        return ASSETS_ERR_NOT_FOUND;
    }
    strcpy(assets->dir, dirs[found]);
    if (!fs_read_file(path, &assets->exe_data, &assets->exe_size)) {
        return ASSETS_ERR_UNREADABLE;
    }
    if (ne_image_open(&assets->image, assets->exe_data, assets->exe_size) != NE_OK
        || ne_image_auto_data_segment(&assets->image, &assets->data_segment) != NE_OK) {
        assets_close(assets);
        return ASSETS_ERR_MALFORMED;
    }
    return ASSETS_OK;
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
