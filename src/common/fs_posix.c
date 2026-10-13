#include "fs.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool fs_executable_dir(char *out, size_t capacity)
{
    ssize_t length;
    char *last_separator;

    if (capacity == 0) {
        return false;
    }
    length = readlink("/proc/self/exe", out, capacity - 1);
    if (length <= 0) {
        return false;
    }
    out[length] = '\0';

    last_separator = strrchr(out, '/');
    if (last_separator == NULL) {
        return false;
    }
    last_separator[last_separator == out ? 1 : 0] = '\0';
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
