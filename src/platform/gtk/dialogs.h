#ifndef JT_GTK_DIALOGS_H
#define JT_GTK_DIALOGS_H

/*
 * The game's dialogs as native GTK windows. Only the gameplay is kept
 * faithful, so their layout is this port's own; the long texts (help boxes,
 * messages, high score lines) still come from the original at runtime.
 *
 * GTK dialogs do not block: each function returns at once and calls its
 * callback, if any, when the dialog closes. The game clock keeps running
 * meanwhile, as it did under the original's modal dialogs.
 */

#include "assets.h"
#include "highscores.h"
#include "settings.h"

#include <gtk/gtk.h>

#include <stdbool.h>

typedef void (*DialogsDone)(void);
typedef void (*DialogsAnswered)(bool yes);
/* `settings` is NULL when the player cancels. */
typedef void (*DialogsOptionsDone)(const GameSettings *settings);
/* `name` is in Windows-1252, like the INI file; empty if the player cancels. */
typedef void (*DialogsNameDone)(const char *name);

/* `owner` may be NULL until the main window exists. */
void dialogs_init(GtkWindow *owner, const Assets *assets);

/* True while any dialog is open: the main window then ignores input. */
bool dialogs_open(void);

/* Texts below are in Windows-1252, as stored in the original. */
void dialogs_message(const char *text, DialogsDone done);
void dialogs_question(const char *text, DialogsAnswered answered);

void dialogs_pause(void);

/* "Get ready...", closing itself after 3 seconds. */
void dialogs_wait(DialogsDone done);

void dialogs_options(const GameSettings *settings, DialogsOptionsDone done);

void dialogs_high_scores(const HighScores *table, DialogsDone done);

void dialogs_new_high_score(long score, DialogsNameDone done);

void dialogs_about(void);

/* The instructions, hints and story boxes, unused by v1.4 but kept in it. */
void dialogs_instructions(void);
void dialogs_hints(void);
void dialogs_story(void);

#endif
