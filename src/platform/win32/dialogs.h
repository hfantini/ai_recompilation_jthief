#ifndef JT_WIN32_DIALOGS_H
#define JT_WIN32_DIALOGS_H

/*
 * The original's dialogs, built from its own Windows 3.x templates at
 * runtime, with the behavior of its dialog procedures.
 */

#include "assets.h"
#include "highscores.h"
#include "settings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdbool.h>

/* Registers the SPLASH control class used by the About dialog. */
bool dialogs_init(HINSTANCE instance, const Assets *assets);

void dialogs_pause(HWND owner);

/* "Get ready...", closing itself after 3 seconds. */
void dialogs_wait(HWND owner);

/* Returns true, with `settings` updated, when the player presses OK. */
bool dialogs_options(HWND owner, GameSettings *settings);

void dialogs_high_scores(HWND owner, const HighScores *table);

/* Asks for the name of a new high scorer; empty if the player cancels. */
void dialogs_new_high_score(HWND owner, long score, char *name, size_t name_size);

void dialogs_about(HWND owner);

/* The instructions, hints and story boxes, unused by v1.4 but kept in it. */
void dialogs_instructions(HWND owner);
void dialogs_hints(HWND owner);
void dialogs_story(HWND owner);

#endif
