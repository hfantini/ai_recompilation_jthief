#include "settings.h"

#include <string.h>

GameSpeed settings_parse_speed(const char *value)
{
    if (strcmp(value, "2") == 0) {
        return GAME_SPEED_EXPERT;
    }
    if (strcmp(value, "0") == 0) {
        return GAME_SPEED_BEGINNER;
    }
    return GAME_SPEED_INTERMEDIATE;
}

bool settings_parse_level_wait(const char *value)
{
    return strcmp(value, "0") != 0;
}

bool settings_parse_cheat(const char *value)
{
    return strcmp(value, "1") == 0;
}

const char *settings_speed_value(GameSpeed speed)
{
    switch (speed) {
    case GAME_SPEED_BEGINNER:
        return "0";
    case GAME_SPEED_EXPERT:
        return "2";
    case GAME_SPEED_INTERMEDIATE:
        break;
    }
    return "1";
}

const char *settings_flag_value(bool flag)
{
    return flag ? "1" : "0";
}
