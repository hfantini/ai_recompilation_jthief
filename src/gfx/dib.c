#include "dib.h"

#define FILE_HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define PALETTE_ENTRY_SIZE 4
#define BI_RGB 0

static uint16_t read_u16(const uint8_t *data, size_t offset)
{
    return (uint16_t)(data[offset] | (data[offset + 1] << 8));
}

static uint32_t read_u32(const uint8_t *data, size_t offset)
{
    return (uint32_t)read_u16(data, offset) | ((uint32_t)read_u16(data, offset + 2) << 16);
}

#define TRUE_COLOR_BITS 24

static bool is_supported_depth(uint16_t bits_per_pixel)
{
    return bits_per_pixel == 1 || bits_per_pixel == 4 || bits_per_pixel == 8
        || bits_per_pixel == TRUE_COLOR_BITS;
}

/*
 * Validates the header and the palette. The pixels start at `pixels_offset`
 * from `data`, or right after the palette when it is 0.
 */
static bool parse(const uint8_t *data, size_t size, size_t pixels_offset, Dib *dib)
{
    int32_t width, height;
    uint16_t planes, bits_per_pixel;
    uint32_t compression, palette_entries;
    uint64_t row_stride, info_size, pixels_size;

    if (data == NULL || dib == NULL || size < INFO_HEADER_SIZE
        || read_u32(data, 0) != INFO_HEADER_SIZE) {
        return false;
    }
    width = (int32_t)read_u32(data, 4);
    height = (int32_t)read_u32(data, 8);
    planes = read_u16(data, 12);
    bits_per_pixel = read_u16(data, 14);
    compression = read_u32(data, 16);
    palette_entries = read_u32(data, 32);

    /*
     * Windows 3.x resources are always bottom-up, uncompressed and palettized;
     * .bmp files, such as the About box's pictures, may also be 24-bit.
     */
    if (width <= 0 || height <= 0 || planes != 1 || !is_supported_depth(bits_per_pixel)
        || compression != BI_RGB) {
        return false;
    }
    if (bits_per_pixel == TRUE_COLOR_BITS) {
        /* An optional palette may precede the pixels; true color does not use it. */
        info_size = INFO_HEADER_SIZE + (uint64_t)palette_entries * PALETTE_ENTRY_SIZE;
        palette_entries = 0;
    }
    else {
        uint32_t max_entries = 1u << bits_per_pixel;

        if (palette_entries == 0) {
            palette_entries = max_entries;
        }
        if (palette_entries > max_entries) {
            return false;
        }
        info_size = INFO_HEADER_SIZE + (uint64_t)palette_entries * PALETTE_ENTRY_SIZE;
    }
    if (pixels_offset == 0) {
        pixels_offset = (size_t)info_size;
    }
    else if (palette_entries != 0 && pixels_offset < info_size) {
        return false;
    }

    row_stride = (((uint64_t)width * bits_per_pixel + 31) / 32) * 4;
    pixels_size = row_stride * (uint64_t)height;
    if (pixels_offset < INFO_HEADER_SIZE || pixels_offset > size
        || pixels_size > size - pixels_offset) {
        return false;
    }

    dib->info = data;
    dib->info_size = INFO_HEADER_SIZE + (size_t)palette_entries * PALETTE_ENTRY_SIZE;
    dib->pixels = data + pixels_offset;
    dib->row_stride = (size_t)row_stride;
    dib->width = width;
    dib->height = height;
    dib->bits_per_pixel = bits_per_pixel;
    dib->palette_entries = palette_entries;
    return true;
}

bool dib_parse(const uint8_t *data, size_t size, Dib *dib)
{
    return parse(data, size, 0, dib);
}

bool dib_parse_file(const uint8_t *data, size_t size, Dib *dib)
{
    uint32_t pixels_offset;

    if (data == NULL || size < FILE_HEADER_SIZE || data[0] != 'B' || data[1] != 'M') {
        return false;
    }
    /* Where the pixels start, from the file header: palette sizes are not always truthful. */
    pixels_offset = read_u32(data, 10);
    if (pixels_offset <= FILE_HEADER_SIZE) {
        return false;
    }
    return parse(data + FILE_HEADER_SIZE, size - FILE_HEADER_SIZE, pixels_offset - FILE_HEADER_SIZE,
                 dib);
}

uint8_t dib_pixel_index(const Dib *dib, int32_t x, int32_t y)
{
    /* Rows are stored bottom-up; within a byte, the leftmost pixel is in the high bits. */
    const uint8_t *row = dib->pixels + (size_t)(dib->height - 1 - y) * dib->row_stride;
    uint32_t bit = (uint32_t)x * dib->bits_per_pixel;
    uint8_t byte = row[bit / 8];
    unsigned shift = 8 - dib->bits_per_pixel - bit % 8;

    return (uint8_t)((byte >> shift) & ((1u << dib->bits_per_pixel) - 1));
}

uint32_t dib_palette_color(const Dib *dib, uint8_t index)
{
    /* RGBQUAD is blue, green, red, reserved: little-endian 0x00RRGGBB. */
    if (index >= dib->palette_entries) {
        return 0;
    }
    return read_u32(dib->info, INFO_HEADER_SIZE + (size_t)index * PALETTE_ENTRY_SIZE) & 0xFFFFFFu;
}

uint32_t dib_pixel_color(const Dib *dib, int32_t x, int32_t y)
{
    const uint8_t *pixel;

    if (dib->bits_per_pixel != TRUE_COLOR_BITS) {
        return dib_palette_color(dib, dib_pixel_index(dib, x, y));
    }
    /* Blue, green, red: little-endian 0x00RRGGBB, like a palette entry. */
    pixel = dib->pixels + (size_t)(dib->height - 1 - y) * dib->row_stride + (size_t)x * 3;
    return (uint32_t)pixel[0] | ((uint32_t)pixel[1] << 8) | ((uint32_t)pixel[2] << 16);
}
