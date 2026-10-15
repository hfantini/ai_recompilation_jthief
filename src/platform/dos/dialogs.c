#include "dialogs.h"

#include "fs.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Control ids in the original's templates. */
enum {
    ID_HELP = 3,
    ID_BEGINNER = 401,
    ID_INTERMEDIATE = 402,
    ID_EXPERT = 403,
    ID_LEVEL_WAIT = 404,
    ID_CHEAT = 406,
    ID_FIRST_HIGH_SCORE = 701,
    ID_NAME = 801,
    ID_SCORE = 802,
    ID_REGISTER = 1
};

enum {
    DIALOG_OPTIONS = 400,
    DIALOG_HIGH_SCORES = 700,
    DIALOG_NEW_HIGH_SCORE = 800,
    DIALOG_ABOUT = 900
};

/* SetTimer(3000) fired on the 55th tick of the 54.9 ms Windows 3.1 clock. */
#define WAIT_TICKS 55
#define NAME_LENGTH 25
#define HIGH_SCORE_LINE 96

static const Assets *app_assets;
static DialogFont system_units;
static DialogFont small_units;

/* One at a time is enough, except for help opened from the options. */
static Dialog main_dialog;
static Dialog help_dialog;

static bool load_template(const char *name, uint16_t id, DialogTemplate *out)
{
    NeBlob blob;
    bool found = name != NULL ? assets_resource_named(app_assets, NE_RT_DIALOG, name, &blob)
                              : assets_resource_id(app_assets, NE_RT_DIALOG, id, &blob);

    return found && templates_read_dialog(blob.data, blob.size, out);
}

static bool open_dialog(Dialog *dialog, const char *name, uint16_t id)
{
    static DialogTemplate dialog_template;

    if (!load_template(name, id, &dialog_template)) {
        return false;
    }
    return dialog_from_template(dialog, &dialog_template, &system_units, &small_units,
                                window_client());
}

void dialogs_init(const Assets *assets, const Font *system_font, const Font *small_font)
{
    app_assets = assets;
    /* The VGA system font's base units; 8-point faces get their own from our smaller font. */
    system_units.font = system_font;
    system_units.unit_x = 8;
    system_units.unit_y = 16;
    small_units.font = small_font;
    small_units.unit_x = small_font->average_width;
    small_units.unit_y = small_font->height;
}

void dialogs_pause(void)
{
    if (open_dialog(&main_dialog, "PAUSEBOX", 0)) {
        dialog_run(&main_dialog);
    }
}

/* Escape does nothing here: the original's procedure ignores it. */
static bool ignore_commands(Dialog *dialog, uint16_t id)
{
    (void)dialog;
    (void)id;
    return false;
}

void dialogs_wait(void)
{
    if (open_dialog(&main_dialog, "WAITBOX", 0)) {
        main_dialog.on_command = ignore_commands;
        main_dialog.timeout_ticks = WAIT_TICKS;
        dialog_run(&main_dialog);
    }
}

static uint16_t speed_button(GameSpeed speed)
{
    switch (speed) {
    case GAME_SPEED_BEGINNER:
        return ID_BEGINNER;
    case GAME_SPEED_EXPERT:
        return ID_EXPERT;
    case GAME_SPEED_INTERMEDIATE:
        break;
    }
    return ID_INTERMEDIATE;
}

static GameSpeed checked_speed(Dialog *dialog)
{
    if (dialog_is_checked(dialog, ID_BEGINNER)) {
        return GAME_SPEED_BEGINNER;
    }
    if (dialog_is_checked(dialog, ID_EXPERT)) {
        return GAME_SPEED_EXPERT;
    }
    return GAME_SPEED_INTERMEDIATE;
}

static void show_help_box(const char *name)
{
    if (open_dialog(&help_dialog, name, 0)) {
        dialog_run(&help_dialog);
    }
}

static bool options_command(Dialog *dialog, uint16_t id)
{
    GameSettings *settings = dialog->context;

    switch (id) {
    case ID_OK:
        settings->speed = checked_speed(dialog);
        settings->level_wait = dialog_is_checked(dialog, ID_LEVEL_WAIT);
        settings->cheat = dialog_is_checked(dialog, ID_CHEAT);
        return true;
    case ID_CANCEL:
        return true;
    case ID_HELP:
        /* The original opened its WinHelp file, which DOS cannot show. */
        show_help_box("INSTRBOX");
        return false;
    }
    return false;
}

bool dialogs_options(GameSettings *settings)
{
    if (!open_dialog(&main_dialog, NULL, DIALOG_OPTIONS)) {
        return false;
    }
    main_dialog.on_command = options_command;
    main_dialog.context = settings;
    dialog_check(&main_dialog, speed_button(settings->speed), true);
    dialog_check(&main_dialog, ID_LEVEL_WAIT, settings->level_wait);
    dialog_check(&main_dialog, ID_CHEAT, settings->cheat);
    return dialog_run(&main_dialog) == ID_OK;
}

void dialogs_high_scores(const HighScores *table)
{
    char line[HIGH_SCORE_LINE];
    int i;

    if (!open_dialog(&main_dialog, NULL, DIALOG_HIGH_SCORES)) {
        return;
    }
    for (i = 0; i < HIGH_SCORES; i++) {
        /* Line 10 has its own format, with less padding after "10.". */
        AssetsText format = i < HIGH_SCORES - 1 ? TEXT_HIGH_SCORE_FORMAT : TEXT_HIGH_SCORE_10_FORMAT;
        snprintf(line, sizeof(line), assets_text(app_assets, format), i + 1,
                 table->entries[i].score, table->entries[i].name);
        dialog_set_text(&main_dialog, (uint16_t)(ID_FIRST_HIGH_SCORE + i), line);
    }
    dialog_run(&main_dialog);
}

void dialogs_new_high_score(long score, char *name, size_t name_size)
{
    char text[32];
    Control *field;

    name[0] = '\0';
    if (!open_dialog(&main_dialog, NULL, DIALOG_NEW_HIGH_SCORE)) {
        return;
    }
    snprintf(text, sizeof(text), "%ld", score);
    dialog_set_text(&main_dialog, ID_SCORE, text);
    field = dialog_control(&main_dialog, ID_NAME);
    if (field != NULL) {
        field->max_length = NAME_LENGTH;
    }
    if (dialog_run(&main_dialog) == ID_OK && field != NULL) {
        snprintf(name, name_size, "%s", field->text);
    }
}

/*
 * The About box's SPLASH controls paint a .bmp file from the game directory,
 * chosen by the control's style: 0 is winlogo.bmp, 1 jwlthief.bmp. Returns
 * the file's bytes, which the picture borrows.
 */
static uint8_t *load_splash(Control *control)
{
    static const char *const FILES[] = { "winlogo.bmp", "jwlthief.bmp" };
    size_t index = (size_t)(control->style & 0xFFFF);
    char path[FS_MAX_PATH];
    uint8_t *data;
    size_t size;

    if (index >= sizeof(FILES) / sizeof(FILES[0])
        || !fs_join_path(path, sizeof(path), app_assets->dir, FILES[index])
        || !fs_read_file(path, &data, &size)) {
        return NULL;
    }
    control->has_image = dib_parse_file(data, size, &control->image);
    return data;
}

void dialogs_about(void)
{
    uint8_t *files[DIALOG_MAX_CONTROLS];
    int count = 0, i;

    if (!open_dialog(&main_dialog, NULL, DIALOG_ABOUT)) {
        return;
    }
    for (i = 0; i < main_dialog.control_count; i++) {
        if (main_dialog.controls[i].kind == CONTROL_IMAGE) {
            files[count++] = load_splash(&main_dialog.controls[i]);
        }
    }
    /* Shareware registration does not apply to this port. */
    dialog_enable(&main_dialog, ID_REGISTER, false);
    dialog_run(&main_dialog);
    for (i = 0; i < count; i++) {
        free(files[i]);
    }
}

void dialogs_instructions(void)
{
    show_help_box("INSTRBOX");
}

void dialogs_hints(void)
{
    show_help_box("HINTSBOX");
}

void dialogs_story(void)
{
    show_help_box("STORYBOX");
}

int dialogs_message(const char *text, const char *caption, MessageButtons buttons)
{
    return message_box(&system_units, text, caption, buttons);
}
