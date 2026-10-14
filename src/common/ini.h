#ifndef JT_COMMON_INI_H
#define JT_COMMON_INI_H

/*
 * INI file access with the semantics of the Windows profile API the original
 * uses. On Windows it is that API (ini_win32.c); elsewhere it is reimplemented
 * (ini_posix.c), so an INI file shared with a Windows installation reads the
 * same on both.
 */

#include <stdbool.h>
#include <stddef.h>

/*
 * Like GetPrivateProfileString: copies the value, or `fallback` when the file,
 * section or key is missing, truncated to `size - 1` characters.
 */
void ini_read(const char *path, const char *section, const char *key, const char *fallback, char *out,
              size_t size);

/* Like WritePrivateProfileString: creates the file, section or key as needed. */
bool ini_write(const char *path, const char *section, const char *key, const char *value);

#endif
