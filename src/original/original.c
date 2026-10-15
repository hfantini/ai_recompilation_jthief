#include "original.h"

#include "fs.h"
#include "sha256.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t ORIGINAL_EXE_SHA256[SHA256_DIGEST_SIZE] = {
    0xc7, 0x64, 0xbe, 0xb9, 0xd1, 0x6d, 0x5a, 0x4a, 0x28, 0x70, 0xd7, 0x94, 0xb6, 0x52, 0x19, 0xb4,
    0xe9, 0x26, 0x49, 0xce, 0xac, 0x1b, 0xff, 0xbd, 0x13, 0xf3, 0x12, 0x54, 0xee, 0x6d, 0x21, 0xf2,
};

typedef struct {
    char *out;
    size_t capacity;
    bool found;
} FindContext;

bool original_is_supported_data(const uint8_t *data, size_t size)
{
    uint8_t digest[SHA256_DIGEST_SIZE];

    if (data == NULL || size != ORIGINAL_EXE_SIZE) {
        return false;
    }
    sha256_digest(data, size, digest);
    return memcmp(digest, ORIGINAL_EXE_SHA256, sizeof(digest)) == 0;
}

bool original_is_supported_exe(const char *path)
{
    uint64_t size;
    uint8_t *data;
    size_t length;
    bool supported;

    /* Cheap size check first, so unrelated files are never read. */
    if (!fs_file_size(path, &size) || size != ORIGINAL_EXE_SIZE) {
        return false;
    }
    if (!fs_read_file(path, &data, &length)) {
        return false;
    }
    supported = original_is_supported_data(data, length);
    free(data);
    return supported;
}

static bool check_candidate(const char *path, void *context)
{
    FindContext *find = context;
    size_t length = strlen(path);

    if (length >= find->capacity || !original_is_supported_exe(path)) {
        return true;
    }
    memcpy(find->out, path, length + 1);
    find->found = true;
    return false;
}

bool original_find_in_dir(const char *dir, char *out, size_t capacity)
{
    FindContext find = { out, capacity, false };

    if (dir == NULL || out == NULL || capacity == 0) {
        return false;
    }
    return fs_list_files(dir, check_candidate, &find) && find.found;
}
