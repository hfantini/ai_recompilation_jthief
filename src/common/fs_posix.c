#include "fs.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __DJGPP__
#include <crt0.h>
#endif

static bool executable_path(char *out, size_t capacity)
{
#ifdef __DJGPP__
    /* DOS has no /proc: the startup code keeps the path DOS started us from. */
    size_t length = strlen(__dos_argv0);

    if (length >= capacity) {
        return false;
    }
    memcpy(out, __dos_argv0, length + 1);
#else
    ssize_t length = readlink("/proc/self/exe", out, capacity - 1);

    if (length <= 0) {
        return false;
    }
    out[length] = '\0';
#endif
    return true;
}

static char *last_separator(char *path)
{
    char *slash = strrchr(path, '/');
#ifdef __DJGPP__
    char *backslash = strrchr(path, '\\');

    if (slash == NULL || (backslash != NULL && backslash > slash)) {
        slash = backslash;
    }
#endif
    return slash;
}

bool fs_executable_dir(char *out, size_t capacity)
{
    char *separator;
    bool is_root;

    if (capacity == 0 || !executable_path(out, capacity)) {
        return false;
    }
    separator = last_separator(out);
    if (separator == NULL) {
        return false;
    }
    /* Roots such as "/" and "C:\" keep their separator. */
    is_root = separator == out || separator[-1] == ':';
    separator[is_root ? 1 : 0] = '\0';
    return true;
}

bool fs_list_files(const char *dir, FsVisitFile visit, void *context)
{
    char path[FS_MAX_PATH];
    struct dirent *entry;
    struct stat info;
    DIR *handle = opendir(dir);

    if (handle == NULL) {
        return false;
    }
    while ((entry = readdir(handle)) != NULL) {
        if (!fs_join_path(path, sizeof(path), dir, entry->d_name)) {
            continue;
        }
        if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
            continue;
        }
        if (!visit(path, context)) {
            break;
        }
    }
    closedir(handle);
    return true;
}

bool fs_file_size(const char *path, uint64_t *size)
{
    struct stat info;

    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return false;
    }
    *size = (uint64_t)info.st_size;
    return true;
}
