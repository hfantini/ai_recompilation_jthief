#ifndef JT_WIN32_WIN16RES_H
#define JT_WIN32_WIN16RES_H

/*
 * Converts Windows 3.x dialog, menu and accelerator resources into their
 * Win32 forms, so the original's own dialogs and menu can be shown. The
 * layouts differ mainly in string encoding (ANSI vs UTF-16) and alignment.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>
#include <stdint.h>

/* Return a malloc'ed template for DialogBoxIndirect / LoadMenuIndirect, or NULL. */
void *win16_dialog_to_win32(const uint8_t *data, size_t size);
void *win16_menu_to_win32(const uint8_t *data, size_t size);

HACCEL win16_create_accelerators(const uint8_t *data, size_t size);

#endif
