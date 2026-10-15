#include "cursor.h"

#include "icon.h"

#include <string.h>

#define HOTSPOT_SIZE 4
#define KEEP_SCREEN 0x0F

/* 'X' is black, 'o' white and ' ' leaves the screen as it is. */
static const char *const ARROW[] = {
    "X           ",
    "XX          ",
    "XoX         ",
    "XooX        ",
    "XoooX       ",
    "XooooX      ",
    "XoooooX     ",
    "XooooooX    ",
    "XoooooooX   ",
    "XooooooooX  ",
    "XoooooXXXXX ",
    "XooXooX     ",
    "XoX XooX    ",
    "XX  XooX    ",
    "X    XooX   ",
    "     XooX   ",
    "      XooX  ",
    "      XooX  ",
    "       XX   ",
};

static const char *const HOURGLASS[] = {
    "XXXXXXXXXXXXXXXX",
    "XooooooooooooooX",
    "XXXXXXXXXXXXXXXX",
    " XooooooooooooX ",
    " XoXXXXXXXXXXoX ",
    "  XoXXXXXXXXoX  ",
    "   XoXXXXXXoX   ",
    "    XoXXXXoX    ",
    "     XoXXoX     ",
    "      XooX      ",
    "      XooX      ",
    "     XooooX     ",
    "    XoooXooX    ",
    "   XoooXXXooX   ",
    "  XoooXXXXXooX  ",
    " XoooXXXXXXXooX ",
    " XooXXXXXXXXXoX ",
    "XXXXXXXXXXXXXXXX",
    "XooooooooooooooX",
    "XXXXXXXXXXXXXXXX",
};

#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

bool cursor_from_resource(Cursor *cursor, const NeBlob *blob)
{
    IconLayers layers;
    int i;

    if (blob->size < HOTSPOT_SIZE
        || !icon_decode_layers(blob->data + HOTSPOT_SIZE, blob->size - HOTSPOT_SIZE, &layers)) {
        return false;
    }
    if (layers.width > CURSOR_MAX_SIZE || layers.height > CURSOR_MAX_SIZE) {
        icon_free_layers(&layers);
        return false;
    }
    memset(cursor, 0, sizeof(*cursor));
    cursor->width = layers.width;
    cursor->height = layers.height;
    cursor->hot_x = blob->data[0] | (blob->data[1] << 8);
    cursor->hot_y = blob->data[2] | (blob->data[3] << 8);
    for (i = 0; i < layers.width * layers.height; i++) {
        cursor->and_mask[i] = layers.keeps_screen[i] ? KEEP_SCREEN : 0;
        cursor->xor_index[i] = (uint8_t)fb_nearest_vga_index(layers.colors[i]);
    }
    icon_free_layers(&layers);
    return true;
}

static void from_art(Cursor *cursor, const char *const *rows, int height, int hot_x, int hot_y)
{
    int x, y;

    memset(cursor, 0, sizeof(*cursor));
    cursor->width = (int)strlen(rows[0]);
    cursor->height = height;
    cursor->hot_x = hot_x;
    cursor->hot_y = hot_y;
    for (y = 0; y < height; y++) {
        for (x = 0; x < cursor->width; x++) {
            int i = y * cursor->width + x;

            cursor->and_mask[i] = rows[y][x] == ' ' ? KEEP_SCREEN : 0;
            cursor->xor_index[i] = rows[y][x] == 'o' ? COLOR_WHITE : COLOR_BLACK;
        }
    }
}

void cursor_arrow(Cursor *cursor)
{
    from_art(cursor, ARROW, COUNT_OF(ARROW), 0, 0);
}

void cursor_hourglass(Cursor *cursor)
{
    from_art(cursor, HOURGLASS, COUNT_OF(HOURGLASS), 8, 10);
}

void cursor_draw(const Cursor *cursor, Canvas *canvas, int x, int y, CursorBackground *saved)
{
    Rect screen = rect_make(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
    int left = x - cursor->hot_x, top = y - cursor->hot_y;
    int row, column;

    saved->area = rect_intersect(rect_make(left, top, cursor->width, cursor->height), screen);
    for (row = saved->area.top; row < saved->area.bottom; row++) {
        for (column = saved->area.left; column < saved->area.right; column++) {
            uint8_t *pixel = &canvas->pixels[row * CANVAS_WIDTH + column];
            int i = (row - top) * cursor->width + (column - left);

            saved->pixels[(row - saved->area.top) * CURSOR_MAX_SIZE + (column - saved->area.left)] =
                *pixel;
            *pixel = (uint8_t)((*pixel & cursor->and_mask[i]) ^ cursor->xor_index[i]);
        }
    }
}

void cursor_erase(Canvas *canvas, const CursorBackground *saved)
{
    int row;

    for (row = saved->area.top; row < saved->area.bottom; row++) {
        memcpy(canvas->pixels + row * CANVAS_WIDTH + saved->area.left,
               saved->pixels + (row - saved->area.top) * CURSOR_MAX_SIZE,
               (size_t)(saved->area.right - saved->area.left));
    }
}
