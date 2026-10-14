#include "profile.h"

#include "fs.h"
#include "ini.h"

#include <ctype.h>
#include <string.h>

#define FILE_NAME "SERVANT.INI"
#define SECTION "Jewel Thief"

/* The original's buffer sizes: option values are read one character long. */
#define OPTION_BUFFER 2
#define SCORES_BUFFER 100
#define NAMES_BUFFER 200
#define WRITE_BUFFER 600

static char path[FS_MAX_PATH];

static bool equals_ignore_case(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static const char *file_name(const char *candidate)
{
    const char *slash = strrchr(candidate, '/');
    const char *backslash = strrchr(candidate, '\\');
    const char *separator = slash > backslash ? slash : backslash;

    return separator != NULL ? separator + 1 : candidate;
}

static bool find_existing(const char *candidate, void *context)
{
    bool *found = context;

    if (!equals_ignore_case(file_name(candidate), FILE_NAME)) {
        return true;
    }
    strcpy(path, candidate);
    *found = true;
    return false;
}

/* Case-sensitive file systems may hold an existing installation's file as servant.ini. */
bool profile_init(const char *dir)
{
    bool found = false;

    fs_list_files(dir, find_existing, &found);
    return found || fs_join_path(path, sizeof(path), dir, FILE_NAME);
}

static void read_value(const char *key, const char *fallback, char *out, size_t size)
{
    ini_read(path, SECTION, key, fallback, out, size);
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
    ini_write(path, SECTION, "LevelWait", settings_flag_value(settings->level_wait));
    ini_write(path, SECTION, "Cheat", settings_flag_value(settings->cheat));
    ini_write(path, SECTION, "Speed", settings_speed_value(settings->speed));
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
    ini_write(path, SECTION, "Scores", scores);
    ini_write(path, SECTION, "NAMES", names);
}
