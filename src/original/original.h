#ifndef JT_ORIGINAL_H
#define JT_ORIGINAL_H

/*
 * Locates the user-supplied original Jewel Thief v1.4 executable.
 *
 * The game's assets are copyrighted and never shipped with this project;
 * they are loaded at runtime from the user's own copy, which is identified
 * by its SHA-256 (file names are ignored, since copies are often renamed).
 */

#include <stdbool.h>
#include <stddef.h>

#define ORIGINAL_EXE_SIZE 131072u
#define ORIGINAL_EXE_SHA256_HEX "c764beb9d16d5a4a2870d794b65219b4e92649ceac1bffbd13f31254ee6d21f2"

bool original_is_supported_exe(const char *path);

/* Scans the files directly inside `dir` and writes the first match to `out`. */
bool original_find_in_dir(const char *dir, char *out, size_t capacity);

#endif
