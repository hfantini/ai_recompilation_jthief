#ifndef JT_CORE_HIGHSCORES_H
#define JT_CORE_HIGHSCORES_H

/*
 * The top ten table, stored in SERVANT.INI as two values: "Scores", ten
 * numbers, and "Names", ten comma-separated names. Scores are the displayed
 * ones (the game score times 100).
 */

#include <stddef.h>

#define HIGH_SCORES 10
#define HIGH_SCORE_NAME_SIZE 50

typedef struct {
    char name[HIGH_SCORE_NAME_SIZE];
    long score;
} HighScore;

typedef struct {
    HighScore entries[HIGH_SCORES];
} HighScores;

/*
 * Parses the INI values as the original does: scores separated by spaces,
 * commas or tabs, names by commas, empty fields skipped, extras ignored.
 */
void high_scores_parse(HighScores *table, const char *scores, const char *names);

/* Formats the INI values the way the original writes them. */
void high_scores_format(const HighScores *table, char *scores, size_t scores_size, char *names,
                        size_t names_size);

/* Place the score would take (0 is the best), or -1 if it misses the table. */
int high_scores_rank(const HighScores *table, long score);

/* Inserts at `rank`, pushing the entries below down and the last one out. */
void high_scores_insert(HighScores *table, int rank, const char *name, long score);

#endif
