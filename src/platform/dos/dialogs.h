#ifndef JT_DOS_DIALOGS_H
#define JT_DOS_DIALOGS_H

/*
 * The original's dialogs, laid out from its own Windows 3.x templates, with
 * the behavior of its dialog procedures, and its message boxes.
 */

#include "assets.h"
#include "dialog.h"
#include "highscores.h"
#include "settings.h"

#include <stdbool.h>
#include <stddef.h>

void dialogs_init(const Assets *assets, const Font *system_font, const Font *small_font);

void dialogs_pause(void);

/* "Get ready...", closing itself after 3 seconds. */
void dialogs_wait(void);

/* Returns true, with `settings` updated, when the player presses OK. */
bool dialogs_options(GameSettings *settings);

void dialogs_high_scores(const HighScores *table);

/* Asks for the name of a new high scorer; empty if the player cancels. */
void dialogs_new_high_score(long score, char *name, size_t name_size);

void dialogs_about(void);

/* The instructions, hints and story boxes, unused by v1.4 but kept in it. */
void dialogs_instructions(void);
void dialogs_hints(void);
void dialogs_story(void);

/* Returns ID_OK, ID_YES or ID_NO. */
int dialogs_message(const char *text, const char *caption, MessageButtons buttons);

#endif
