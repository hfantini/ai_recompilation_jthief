#ifndef JT_CORE_SETTINGS_H
#define JT_CORE_SETTINGS_H

/*
 * The options stored in SERVANT.INI, section [Jewel Thief], and how the
 * original reads their values. Missing keys mean intermediate speed, no
 * pause between levels and no cheat.
 */

#include "game.h"

#include <stdbool.h>

typedef struct {
    GameSpeed speed;
    bool level_wait;
    bool cheat;
} GameSettings;

/* "2" is Expert and "0" Beginner; anything else is Intermediate. */
GameSpeed settings_parse_speed(const char *value);

/* Anything but "0" turns it on. */
bool settings_parse_level_wait(const char *value);

/* Only "1" turns it on. */
bool settings_parse_cheat(const char *value);

/* "0", "1" or "2", as the options dialog writes. */
const char *settings_speed_value(GameSpeed speed);

/* "0" or "1". */
const char *settings_flag_value(bool flag);

#endif
