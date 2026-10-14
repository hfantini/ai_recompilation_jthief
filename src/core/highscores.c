#include "highscores.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCORE_DELIMITERS " ,\t"
#define NAME_DELIMITERS ","

/*
 * Like strtok: skips leading delimiters, returns the length of the next
 * token at *cursor (0 at the end), and moves *cursor past it.
 */
static size_t next_token(const char **cursor, const char *delimiters, const char **token)
{
    size_t length;

    *cursor += strspn(*cursor, delimiters);
    *token = *cursor;
    length = strcspn(*cursor, delimiters);
    *cursor += length;
    return length;
}

static void copy_name(char *out, const char *name, size_t length)
{
    if (length >= HIGH_SCORE_NAME_SIZE) {
        length = HIGH_SCORE_NAME_SIZE - 1;
    }
    memcpy(out, name, length);
    out[length] = '\0';
}

void high_scores_parse(HighScores *table, const char *scores, const char *names)
{
    const char *cursor, *token;
    char number[32];
    size_t length;
    int i;

    memset(table, 0, sizeof(*table));

    cursor = scores;
    for (i = 0; i < HIGH_SCORES && (length = next_token(&cursor, SCORE_DELIMITERS, &token)) > 0; i++) {
        copy_name(number, token, length < sizeof(number) ? length : sizeof(number) - 1);
        table->entries[i].score = atol(number);
    }

    cursor = names;
    for (i = 0; i < HIGH_SCORES && (length = next_token(&cursor, NAME_DELIMITERS, &token)) > 0; i++) {
        copy_name(table->entries[i].name, token, length);
    }
}

void high_scores_format(const HighScores *table, char *scores, size_t scores_size, char *names,
                        size_t names_size)
{
    size_t scores_used = 0, names_used = 0;
    int i;

    scores[0] = '\0';
    names[0] = '\0';
    for (i = 0; i < HIGH_SCORES; i++) {
        const char *separator = i > 0 ? "," : "";
        int written;

        written = snprintf(scores + scores_used, scores_size - scores_used, "%s%ld", separator,
                           table->entries[i].score);
        if (written > 0 && (size_t)written < scores_size - scores_used) {
            scores_used += (size_t)written;
        }
        written = snprintf(names + names_used, names_size - names_used, "%s%s", separator,
                           table->entries[i].name);
        if (written > 0 && (size_t)written < names_size - names_used) {
            names_used += (size_t)written;
        }
    }
}

int high_scores_rank(const HighScores *table, long score)
{
    int rank;

    for (rank = 0; rank < HIGH_SCORES; rank++) {
        if (table->entries[rank].score < score) {
            return rank;
        }
    }
    return -1;
}

void high_scores_insert(HighScores *table, int rank, const char *name, long score)
{
    if (rank < 0 || rank >= HIGH_SCORES) {
        return;
    }
    memmove(&table->entries[rank + 1], &table->entries[rank],
            (size_t)(HIGH_SCORES - 1 - rank) * sizeof(table->entries[0]));
    copy_name(table->entries[rank].name, name, strlen(name));
    table->entries[rank].score = score;
}
