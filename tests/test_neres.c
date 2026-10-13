#include "fs.h"
#include "neres.h"
#include "original.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

/* Every bitmap of v1.4, by the name used in its resource table. */
static const char *const BITMAP_NAMES[] = {
    "STATUSBR", "APACHE", "BAT", "BATTLESHIP", "CAMEL", "CLIPPER", "CLOUD", "HELICOPTER",
    "MASK", "NAZGUL", "OGRE", "RAINDROP", "SOLDIER", "SPIDER", "WIZARD", "EMERALD",
    "AMETHYST", "GOLD", "ONYX", "SAPPHIRE", "RUBY", "PEARL", "OPAL", "PERIDOT", "GARNET",
    "CITRINE", "AQUAMARINE", "TOPAZ", "DIAMOND", "STAR", "SPHINX", "WATER", "CAVE",
    "CASTLE", "CITY",
};

#define BITMAP_INFO_HEADER_SIZE 40

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void check_rejects_non_ne_data(void)
{
    static const uint8_t not_an_exe[64] = { 'P', 'K' };
    NeImage image;

    CHECK(ne_image_open(&image, not_an_exe, sizeof(not_an_exe)) == NE_ERR_NOT_NE);
    CHECK(ne_image_open(&image, not_an_exe, 2) == NE_ERR_NOT_NE);
}

static bool count_resource(const NeResource *resource, void *context)
{
    (void)resource;
    (*(int *)context)++;
    return true;
}

static void check_original_resources(const NeImage *image)
{
    NeBlob blob;
    size_t i;
    int count = 0;

    CHECK(ne_image_for_each_resource(image, count_resource, &count) == NE_OK);
    CHECK(count == 58);

    for (i = 0; i < sizeof(BITMAP_NAMES) / sizeof(BITMAP_NAMES[0]); i++) {
        CHECK(ne_image_find_named(image, NE_RT_BITMAP, BITMAP_NAMES[i], &blob) == NE_OK);
        CHECK(blob.size >= BITMAP_INFO_HEADER_SIZE);
        CHECK(read_le32(blob.data) == BITMAP_INFO_HEADER_SIZE);
    }

    CHECK(ne_image_find_named(image, NE_RT_BITMAP, "cave", &blob) == NE_OK);
    CHECK(ne_image_find_named(image, NE_RT_MENU, "JWLTHIEFMENU", &blob) == NE_OK);
    CHECK(ne_image_find_named(image, NE_RT_DIALOG, "PAUSEBOX", &blob) == NE_OK);
    CHECK(ne_image_find_id(image, NE_RT_DIALOG, 700, &blob) == NE_OK);
    CHECK(ne_image_find_id(image, NE_RT_ICON, 1, &blob) == NE_OK);
    CHECK(ne_image_find_named(image, NE_RT_BITMAP, "UNICORN", &blob) == NE_ERR_NOT_FOUND);
}

int main(void)
{
    char path[FS_MAX_PATH];
    NeImage image;
    uint8_t *data;
    size_t size;

    check_rejects_non_ne_data();

    if (!original_find_in_dir(JT_ORIGINAL_DIR, path, sizeof(path))) {
        fprintf(stderr, "original v1.4 executable not found in %s; skipping\n", JT_ORIGINAL_DIR);
        return test_failures == 0 ? TEST_SKIP_CODE : 1;
    }
    CHECK(fs_read_file(path, &data, &size));
    if (test_failures == 0) {
        CHECK(ne_image_open(&image, data, size) == NE_OK);
        check_original_resources(&image);
        free(data);
    }
    return TEST_RESULT();
}
