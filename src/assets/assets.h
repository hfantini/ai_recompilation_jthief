#ifndef JT_ASSETS_H
#define JT_ASSETS_H

/*
 * Game assets, read at runtime from the user's own original JWLTHIEF.EXE
 * v1.4: bitmaps, cursors, dialogs, menus and texts. Nothing copyrighted
 * ships with this project.
 */

#include "dib.h"
#include "fs.h"
#include "neres.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ASSETS_OK = 0,
    ASSETS_ERR_NOT_FOUND,
    ASSETS_ERR_UNREADABLE,
    ASSETS_ERR_MALFORMED
} AssetsStatus;

/*
 * Texts in the original's data segment, by offset. The executable is
 * identified by its hash, so the offsets are fixed.
 */
typedef enum {
    TEXT_TITLE = 0x01D3,
    TEXT_TRADE_JEWELS = 0x01DF,
    TEXT_PLAY_AGAIN = 0x026A,
    TEXT_TRY_EXPERT = 0x0308,
    TEXT_PARABLE_1 = 0x033C,
    TEXT_PARABLE_2 = 0x037E,
    TEXT_PARABLE_3 = 0x03C1,
    TEXT_PARABLE_SOURCE = 0x03D0,
    TEXT_GREAT_PRICE = 0x0413,
    TEXT_ENDING_FORMAT = 0x0449,
    TEXT_DEFAULT_SCORES = 0x08CE,
    TEXT_DEFAULT_NAMES = 0x08F5,
    TEXT_HIGH_SCORE_FORMAT = 0x0994,
    TEXT_HIGH_SCORE_10_FORMAT = 0x09AD
} AssetsText;

typedef struct {
    uint8_t *exe_data;
    size_t exe_size;
    NeImage image;
    NeBlob data_segment;
    /* The directory the original was found in; its INI and .bmp files live there. */
    char dir[FS_MAX_PATH];
} Assets;

/* Searches `dirs` in order; NULL entries are skipped. */
AssetsStatus assets_open(Assets *assets, const char *const *dirs, size_t dir_count);

void assets_close(Assets *assets);

/* Finds a BITMAP resource by name; the result borrows from `assets`. */
bool assets_bitmap(const Assets *assets, const char *name, Dib *out);

/*
 * Finds the first image of a named cursor group. A Windows 3.x cursor is
 * the hotspot (two little-endian words) followed by a DIB holding the XOR
 * and AND masks: the layout Win32 CreateIconFromResourceEx accepts.
 */
bool assets_cursor(const Assets *assets, const char *name, NeBlob *out);

/* Same for a named icon group; an icon resource is a DIB with XOR and AND masks. */
bool assets_icon(const Assets *assets, const char *name, NeBlob *out);

/* Raw resources, such as dialog and menu templates in Windows 3.x format. */
bool assets_resource_named(const Assets *assets, uint16_t type, const char *name, NeBlob *out);
bool assets_resource_id(const Assets *assets, uint16_t type, uint16_t id, NeBlob *out);

/* A text of the original, in Windows-1252; empty if the offset is invalid. */
const char *assets_text(const Assets *assets, AssetsText text);

#endif
