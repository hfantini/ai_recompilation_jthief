#include "fs.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

bool fs_executable_dir(char *out, size_t capacity)
{
    DWORD length;
    char *last_separator;

    if (capacity == 0 || capacity > MAXDWORD) {
        return false;
    }
    length = GetModuleFileNameA(NULL, out, (DWORD)capacity);
    if (length == 0 || length >= capacity) {
        return false;
    }

    last_separator = strrchr(out, '\\');
    if (last_separator == NULL) {
        return false;
    }
    *last_separator = '\0';
    return true;
}

bool fs_list_files(const char *dir, FsVisitFile visit, void *context)
{
    char pattern[FS_MAX_PATH];
    char path[FS_MAX_PATH];
    WIN32_FIND_DATAA entry;
    HANDLE handle;

    if (!fs_join_path(pattern, sizeof(pattern), dir, "*")) {
        return false;
    }
    handle = FindFirstFileA(pattern, &entry);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    do {
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        if (!fs_join_path(path, sizeof(path), dir, entry.cFileName)) {
            continue;
        }
        if (!visit(path, context)) {
            break;
        }
    } while (FindNextFileA(handle, &entry));
    FindClose(handle);
    return true;
}

bool fs_file_size(const char *path, uint64_t *size)
{
    WIN32_FILE_ATTRIBUTE_DATA info;

    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info)) {
        return false;
    }
    if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }
    *size = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;
    return true;
}
