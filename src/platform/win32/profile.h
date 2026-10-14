#ifndef JT_WIN32_PROFILE_H
#define JT_WIN32_PROFILE_H

/*
 * SERVANT.INI, read and written like the original: the file sits next to
 * the original JWLTHIEF.EXE, so an existing installation keeps its options
 * and high scores.
 */

#include "assets.h"
#include "highscores.h"
#include "settings.h"

#include <stdbool.h>

bool profile_init(const char *dir);

GameSettings profile_load_settings(void);

void profile_save_settings(const GameSettings *settings);

/* Missing values default to the original's: zero scores and "-" names. */
void profile_load_high_scores(HighScores *table, const Assets *assets);

void profile_save_high_scores(const HighScores *table);

#endif
