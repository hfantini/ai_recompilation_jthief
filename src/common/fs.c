#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

bool fs_read_file(const char *path, uint8_t **data, size_t *size)
{
    uint64_t file_size;
    uint8_t *buffer;
    FILE *file;
    bool ok;

    if (!fs_file_size(path, &file_size) || file_size > SIZE_MAX) {
        return false;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }

    /* malloc(0) may return NULL, so always reserve at least one byte. */
    buffer = malloc(file_size > 0 ? (size_t)file_size : 1);
    ok = buffer != NULL && fread(buffer, 1, (size_t)file_size, file) == (size_t)file_size;
    fclose(file);

    if (!ok) {
        free(buffer);
        return false;
    }
    *data = buffer;
    *size = (size_t)file_size;
    return true;
}

bool fs_join_path(char *out, size_t capacity, const char *dir, const char *name)
{
    size_t dir_length = strlen(dir);
    bool needs_separator = dir_length > 0 && dir[dir_length - 1] != '/' && dir[dir_length - 1] != '\\';
    int written = needs_separator
        ? snprintf(out, capacity, "%s%c%s", dir, PATH_SEPARATOR, name)
        : snprintf(out, capacity, "%s%s", dir, name);

    return written >= 0 && (size_t)written < capacity;
}
