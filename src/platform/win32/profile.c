#include "profile.h"

#include "fs.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define FILE_NAME "SERVANT.INI"
#define SECTION "Jewel Thief"

/* The original's buffer sizes: option values are read one character long. */
#define OPTION_BUFFER 2
#define SCORES_BUFFER 100
#define NAMES_BUFFER 200
#define WRITE_BUFFER 600

static char path[FS_MAX_PATH];

bool profile_init(const char *dir)
{
    return fs_join_path(path, sizeof(path), dir, FILE_NAME);
}

static void read_value(const char *key, const char *fallback, char *out, DWORD size)
{
    GetPrivateProfileStringA(SECTION, key, fallback, out, size, path);
}

GameSettings profile_load_settings(void)
{
    GameSettings settings;
    char value[OPTION_BUFFER];

    read_value("Speed", "", value, sizeof(value));
    settings.speed = settings_parse_speed(value);
    read_value("LevelWait", "0", value, sizeof(value));
    settings.level_wait = settings_parse_level_wait(value);
    read_value("Cheat", "", value, sizeof(value));
    settings.cheat = settings_parse_cheat(value);
    return settings;
}

void profile_save_settings(const GameSettings *settings)
{
    WritePrivateProfileStringA(SECTION, "LevelWait", settings_flag_value(settings->level_wait), path);
    WritePrivateProfileStringA(SECTION, "Cheat", settings_flag_value(settings->cheat), path);
    WritePrivateProfileStringA(SECTION, "Speed", settings_speed_value(settings->speed), path);
}

void profile_load_high_scores(HighScores *table, const Assets *assets)
{
    char scores[SCORES_BUFFER];
    char names[NAMES_BUFFER];

    read_value("Scores", assets_text(assets, TEXT_DEFAULT_SCORES), scores, sizeof(scores));
    read_value("Names", assets_text(assets, TEXT_DEFAULT_NAMES), names, sizeof(names));
    high_scores_parse(table, scores, names);
}

void profile_save_high_scores(const HighScores *table)
{
    char scores[WRITE_BUFFER];
    char names[WRITE_BUFFER];

    high_scores_format(table, scores, sizeof(scores), names, sizeof(names));
    WritePrivateProfileStringA(SECTION, "Scores", scores, path);
    WritePrivateProfileStringA(SECTION, "NAMES", names, path);
}
