#include "icon.h"

#include "dib.h"

#include <stdlib.h>
#include <string.h>

#define INFO_HEADER_SIZE 40
#define HEIGHT_FIELD 8

static bool mask_bit(const uint8_t *mask, size_t stride, int height, int x, int y)
{
    const uint8_t *row = mask + (size_t)(height - 1 - y) * stride;

    return (row[x / 8] >> (7 - x % 8)) & 1;
}

bool icon_decode(const uint8_t *data, size_t size, IconImage *out)
{
    uint8_t *copy;
    const uint8_t *mask;
    size_t mask_stride, color_size;
    int32_t height;
    Dib color;
    int x, y;

    memset(out, 0, sizeof(*out));
    if (size < INFO_HEADER_SIZE) {
        return false;
    }
    /* The DIB parser sees the color image alone once the height is halved. */
    copy = malloc(size);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, data, size);
    height = (int32_t)(data[HEIGHT_FIELD] | (data[HEIGHT_FIELD + 1] << 8)
                       | ((uint32_t)data[HEIGHT_FIELD + 2] << 16)
                       | ((uint32_t)data[HEIGHT_FIELD + 3] << 24));
    height /= 2;
    copy[HEIGHT_FIELD] = (uint8_t)height;
    copy[HEIGHT_FIELD + 1] = (uint8_t)(height >> 8);
    copy[HEIGHT_FIELD + 2] = (uint8_t)(height >> 16);
    copy[HEIGHT_FIELD + 3] = (uint8_t)(height >> 24);

    if (height <= 0 || !dib_parse(copy, size, &color)) {
        free(copy);
        return false;
    }
    mask_stride = (((size_t)color.width + 31) / 32) * 4;
    color_size = (size_t)(color.pixels - copy) + color.row_stride * (size_t)color.height;
    if (color_size > size || mask_stride * (size_t)color.height > size - color_size) {
        free(copy);
        return false;
    }
    mask = copy + color_size;

    out->rgba = malloc((size_t)color.width * (size_t)color.height * 4);
    if (out->rgba == NULL) {
        free(copy);
        return false;
    }
    out->width = color.width;
    out->height = color.height;
    for (y = 0; y < color.height; y++) {
        for (x = 0; x < color.width; x++) {
            uint8_t *pixel = out->rgba + ((size_t)y * (size_t)color.width + (size_t)x) * 4;
            uint32_t rgb = dib_pixel_color(&color, x, y);
            bool keeps_screen = mask_bit(mask, mask_stride, color.height, x, y);

            pixel[0] = (uint8_t)(rgb >> 16);
            pixel[1] = (uint8_t)(rgb >> 8);
            pixel[2] = (uint8_t)rgb;
            pixel[3] = 0xFF;
            if (keeps_screen) {
                if (rgb == 0) {
                    pixel[3] = 0;
                }
                else {
                    pixel[0] = pixel[1] = pixel[2] = 0;
                }
            }
        }
    }
    free(copy);
    return true;
}

void icon_free(IconImage *image)
{
    free(image->rgba);
    memset(image, 0, sizeof(*image));
}
