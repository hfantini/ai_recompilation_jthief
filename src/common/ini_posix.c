#include "ini.h"

#include "fs.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *begin;
    const char *end;
} Span;

typedef struct {
    Span content; /* without the line break */
    const char *next;
} Line;

typedef struct {
    bool section_found;
    const char *section_end; /* just past the section's last non-blank line */
    bool key_found;
    Line key_line;
    Span value;
} Lookup;

typedef struct {
    char *data;
    size_t used;
} Buffer;

static bool next_line(const char **cursor, const char *end, Line *line)
{
    const char *newline;

    if (*cursor >= end) {
        return false;
    }
    newline = memchr(*cursor, '\n', (size_t)(end - *cursor));
    line->content.begin = *cursor;
    line->content.end = newline != NULL ? newline : end;
    line->next = newline != NULL ? newline + 1 : end;
    if (line->content.end > line->content.begin && line->content.end[-1] == '\r') {
        line->content.end--;
    }
    *cursor = line->next;
    return true;
}

static Span trim(Span span)
{
    while (span.begin < span.end && isspace((unsigned char)*span.begin)) {
        span.begin++;
    }
    while (span.end > span.begin && isspace((unsigned char)span.end[-1])) {
        span.end--;
    }
    return span;
}

static bool is_blank(Span span)
{
    span = trim(span);
    return span.begin == span.end;
}

/* Section and key names are case-insensitive, as in the Windows profile API. */
static bool span_equals(Span span, const char *text)
{
    size_t length = strlen(text);
    size_t i;

    if ((size_t)(span.end - span.begin) != length) {
        return false;
    }
    for (i = 0; i < length; i++) {
        if (toupper((unsigned char)span.begin[i]) != toupper((unsigned char)text[i])) {
            return false;
        }
    }
    return true;
}

static bool parse_section(Span line, Span *name)
{
    const char *close;

    line = trim(line);
    if (line.begin == line.end || *line.begin != '[') {
        return false;
    }
    close = memchr(line.begin, ']', (size_t)(line.end - line.begin));
    if (close == NULL) {
        return false;
    }
    name->begin = line.begin + 1;
    name->end = close;
    *name = trim(*name);
    return true;
}

/* Only a ';' that starts the line makes a comment, so values may contain one. */
static bool parse_entry(Span line, Span *key, Span *value)
{
    const char *equals;

    line = trim(line);
    if (line.begin == line.end || *line.begin == ';') {
        return false;
    }
    equals = memchr(line.begin, '=', (size_t)(line.end - line.begin));
    if (equals == NULL) {
        return false;
    }
    key->begin = line.begin;
    key->end = equals;
    *key = trim(*key);
    value->begin = equals + 1;
    value->end = line.end;
    *value = trim(*value);
    return true;
}

/* Only the first section with the name counts, as on Windows. */
static void find_entry(const char *text, const char *end, const char *section, const char *key,
                       Lookup *lookup)
{
    const char *cursor = text;
    bool inside = false;
    Span name, entry_key, entry_value;
    Line line;

    memset(lookup, 0, sizeof(*lookup));
    while (next_line(&cursor, end, &line)) {
        if (parse_section(line.content, &name)) {
            if (inside) {
                return;
            }
            inside = span_equals(name, section);
            if (inside) {
                lookup->section_found = true;
                lookup->section_end = line.next;
            }
            continue;
        }
        if (!inside) {
            continue;
        }
        if (!is_blank(line.content)) {
            lookup->section_end = line.next;
        }
        if (parse_entry(line.content, &entry_key, &entry_value) && span_equals(entry_key, key)) {
            lookup->key_found = true;
            lookup->key_line = line;
            lookup->value = entry_value;
            return;
        }
    }
}

static Span unquote(Span value)
{
    if (value.end - value.begin >= 2 && (*value.begin == '"' || *value.begin == '\'')
        && value.end[-1] == *value.begin) {
        value.begin++;
        value.end--;
    }
    return value;
}

void ini_read(const char *path, const char *section, const char *key, const char *fallback, char *out,
              size_t size)
{
    uint8_t *data = NULL;
    size_t data_size, length;
    Span value;
    Lookup lookup;

    if (size == 0) {
        return;
    }
    value.begin = fallback;
    value.end = fallback + strlen(fallback);
    if (fs_read_file(path, &data, &data_size)) {
        const char *text = (const char *)data;

        find_entry(text, text + data_size, section, key, &lookup);
        if (lookup.key_found) {
            value = unquote(lookup.value);
        }
    }

    length = (size_t)(value.end - value.begin);
    if (length >= size) {
        length = size - 1;
    }
    memcpy(out, value.begin, length);
    out[length] = '\0';
    free(data);
}

static void append(Buffer *buffer, const char *text, size_t length)
{
    memcpy(buffer->data + buffer->used, text, length);
    buffer->used += length;
}

static void append_string(Buffer *buffer, const char *text)
{
    append(buffer, text, strlen(text));
}

static void append_entry(Buffer *buffer, const char *key, const char *value, const char *newline)
{
    append_string(buffer, key);
    append_string(buffer, "=");
    append_string(buffer, value);
    append_string(buffer, newline);
}

/* Keeps the file's line breaks; a new file gets CRLF so Windows tools read it too. */
static const char *line_break(const char *text, const char *end)
{
    const char *newline = memchr(text, '\n', (size_t)(end - text));

    return newline != NULL && (newline == text || newline[-1] != '\r') ? "\n" : "\r\n";
}

/*
 * The file name with its extension swapped for .tmp. DOS names have a single
 * extension, so appending one could truncate back to the INI itself.
 */
static bool temporary_path(const char *path, char *out, size_t size)
{
    const char *dot = strrchr(path, '.');
    const char *separator = strpbrk(dot != NULL ? dot : path, "/\\");
    size_t stem = dot != NULL && separator == NULL ? (size_t)(dot - path) : strlen(path);
    int written = snprintf(out, size, "%.*s.tmp", (int)stem, path);

    return written >= 0 && (size_t)written < size;
}

/* Writes a temporary file first, so a crash never leaves a half-written INI. */
static bool replace_file(const char *path, const char *data, size_t size)
{
    char temporary[FS_MAX_PATH];
    FILE *file;
    bool ok;

    if (!temporary_path(path, temporary, sizeof(temporary))) {
        return false;
    }
    file = fopen(temporary, "wb");
    if (file == NULL) {
        return false;
    }
    ok = fwrite(data, 1, size, file) == size;
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temporary, path) != 0) {
        remove(temporary);
        return false;
    }
    return true;
}

bool ini_write(const char *path, const char *section, const char *key, const char *value)
{
    uint8_t *data = NULL;
    size_t size = 0;
    const char *text, *end, *newline;
    Buffer buffer;
    Lookup lookup;
    bool ok;

    if (!fs_read_file(path, &data, &size)) {
        uint64_t existing;

        /* A file that exists but cannot be read must not be replaced by one entry. */
        if (fs_file_size(path, &existing)) {
            return false;
        }
        size = 0;
    }
    text = data != NULL ? (const char *)data : "";
    end = text + size;
    newline = line_break(text, end);

    buffer.data = malloc(size + strlen(section) + strlen(key) + strlen(value) + 16);
    if (buffer.data == NULL) {
        free(data);
        return false;
    }
    buffer.used = 0;

    find_entry(text, end, section, key, &lookup);
    if (lookup.key_found) {
        /* Replaces only the content, so the line keeps its own line break. */
        append(&buffer, text, (size_t)(lookup.key_line.content.begin - text));
        append_string(&buffer, key);
        append_string(&buffer, "=");
        append_string(&buffer, value);
        append(&buffer, lookup.key_line.content.end, (size_t)(end - lookup.key_line.content.end));
    } else if (lookup.section_found) {
        append(&buffer, text, (size_t)(lookup.section_end - text));
        if (lookup.section_end[-1] != '\n') {
            append_string(&buffer, newline);
        }
        append_entry(&buffer, key, value, newline);
        append(&buffer, lookup.section_end, (size_t)(end - lookup.section_end));
    } else {
        append(&buffer, text, size);
        if (size > 0 && end[-1] != '\n') {
            append_string(&buffer, newline);
        }
        append_string(&buffer, "[");
        append_string(&buffer, section);
        append_string(&buffer, "]");
        append_string(&buffer, newline);
        append_entry(&buffer, key, value, newline);
    }

    ok = replace_file(path, buffer.data, buffer.used);
    free(buffer.data);
    free(data);
    return ok;
}
