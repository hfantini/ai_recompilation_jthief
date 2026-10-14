#include "ini.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void ini_read(const char *path, const char *section, const char *key, const char *fallback, char *out,
              size_t size)
{
    if (size == 0) {
        return;
    }
    if (size > MAXDWORD) {
        size = MAXDWORD;
    }
    GetPrivateProfileStringA(section, key, fallback, out, (DWORD)size, path);
}

bool ini_write(const char *path, const char *section, const char *key, const char *value)
{
    return WritePrivateProfileStringA(section, key, value, path) != 0;
}
