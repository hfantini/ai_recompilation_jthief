#ifndef JT_COMMON_FS_H
#define JT_COMMON_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FS_MAX_PATH 4096

/* Called for each regular file in a directory. Return false to stop listing. */
typedef bool (*FsVisitFile)(const char *path, void *context);

/* Writes the directory that contains the running executable. */
bool fs_executable_dir(char *out, size_t capacity);

/* Visits every regular file directly inside `dir` (not recursive). */
bool fs_list_files(const char *dir, FsVisitFile visit, void *context);

bool fs_file_size(const char *path, uint64_t *size);

/* Reads a whole file into a malloc'ed buffer owned by the caller. */
bool fs_read_file(const char *path, uint8_t **data, size_t *size);

bool fs_join_path(char *out, size_t capacity, const char *dir, const char *name);

#endif
