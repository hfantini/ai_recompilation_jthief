/*
 * The WebAssembly side of the browser port: a flat API the TypeScript layer
 * calls. The browser owns the loop, so instead of driving the game like the
 * Win32 and GTK entry points do, this file only answers calls: open the
 * original, feed ticks and mouse moves, render a frame.
 *
 * Gameplay, rendering, settings parsing and high scores stay in the
 * portable C code, so the web port plays exactly like the native ones.
 * Strings cross the boundary in the original's Windows-1252.
 */

#include "assets.h"
#include "framebuffer.h"
#include "game.h"
#include "highscores.h"
#include "icon.h"
#include "render.h"
#include "settings.h"

#include <emscripten/emscripten.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The Win32 port's client area, which is the original's Windows 3.1 one. */
#define CLIENT_WIDTH 498
#define CLIENT_HEIGHT 260
#define STATUS_BAR_HEIGHT 32

/* A Windows 3.x cursor resource starts with its hotspot, then the image. */
#define CURSOR_HOTSPOT_SIZE 4

#define ENDING_MESSAGE_SIZE 512
#define DIALOG_TEXT_SIZE 1024

/* The original's buffer sizes, as in profile.c: values longer than these are cut. */
#define OPTION_BUFFER 2
#define SCORES_BUFFER 100
#define NAMES_BUFFER 200
#define WRITE_BUFFER 600

#define JT_EXPORT EMSCRIPTEN_KEEPALIVE

typedef enum {
    JT_OK = 0,
    JT_ERR_NOT_ORIGINAL,
    JT_ERR_MALFORMED,
    JT_ERR_MISSING_BITMAP,
    JT_ERR_MISSING_CURSOR,
    JT_ERR_OUT_OF_MEMORY
} JtStatus;

enum { CURSOR_RIGHT, CURSOR_LEFT, CURSOR_COUNT };

typedef struct {
    IconImage image;
    int hotspot_x;
    int hotspot_y;
} Cursor;

static const char *const CURSOR_NAMES[CURSOR_COUNT] = { "RIGHTCUR", "LEFTCUR" };

static Assets assets;
static Renderer renderer;
static Framebuffer frame;
static Game game;
static Cursor cursors[CURSOR_COUNT];
static HighScores high_scores;
static RenderStatusText status_text;

/* Canvas ImageData wants R, G, B, A bytes; the framebuffer holds 0x00RRGGBB words. */
static uint8_t frame_rgba[CLIENT_WIDTH * CLIENT_HEIGHT * 4];

static char missing_name[64];
static char ending_message[ENDING_MESSAGE_SIZE];
static char dialog_text[DIALOG_TEXT_SIZE];
static char scores_value[WRITE_BUFFER];
static char names_value[WRITE_BUFFER];

/* Cuts `value` the way GetPrivateProfileString fills a buffer of `size` bytes. */
static const char *read_as_original(const char *value, char *buffer, size_t size)
{
    snprintf(buffer, size, "%s", value);
    return buffer;
}

static void set_missing(const char *name)
{
    snprintf(missing_name, sizeof(missing_name), "%s", name);
}

static bool load_cursor(int index)
{
    Cursor *cursor = &cursors[index];
    NeBlob blob;

    if (!assets_cursor(&assets, CURSOR_NAMES[index], &blob) || blob.size <= CURSOR_HOTSPOT_SIZE
        || !icon_decode(blob.data + CURSOR_HOTSPOT_SIZE, blob.size - CURSOR_HOTSPOT_SIZE,
                        &cursor->image)) {
        set_missing(CURSOR_NAMES[index]);
        return false;
    }
    cursor->hotspot_x = blob.data[0] | (blob.data[1] << 8);
    cursor->hotspot_y = blob.data[2] | (blob.data[3] << 8);
    return true;
}

static JtStatus init_renderer(void)
{
    const char *missing = NULL;

    switch (render_init(&renderer, &assets, CLIENT_WIDTH, CLIENT_HEIGHT, &missing)) {
    case RENDER_OK:
        return JT_OK;
    case RENDER_ERR_MISSING_BITMAP:
        set_missing(missing);
        return JT_ERR_MISSING_BITMAP;
    case RENDER_ERR_OUT_OF_MEMORY:
        return JT_ERR_OUT_OF_MEMORY;
    }
    return JT_ERR_OUT_OF_MEMORY;
}

/* --- Startup ------------------------------------------------------------- */

/*
 * Opens the original from a buffer allocated with malloc() by the caller,
 * which this call owns from then on. Called once.
 */
JT_EXPORT int jt_open(uint8_t *data, size_t size)
{
    JtStatus status;
    int i;

    switch (assets_open_memory(&assets, data, size)) {
    case ASSETS_OK:
        break;
    case ASSETS_ERR_NOT_FOUND:
        return JT_ERR_NOT_ORIGINAL;
    case ASSETS_ERR_UNREADABLE:
    case ASSETS_ERR_MALFORMED:
        return JT_ERR_MALFORMED;
    }
    if (!fb_create(&frame, CLIENT_WIDTH, CLIENT_HEIGHT)) {
        return JT_ERR_OUT_OF_MEMORY;
    }
    status = init_renderer();
    if (status != JT_OK) {
        return status;
    }
    for (i = 0; i < CURSOR_COUNT; i++) {
        if (!load_cursor(i)) {
            return JT_ERR_MISSING_CURSOR;
        }
    }
    return JT_OK;
}

/* The bitmap or cursor a failed jt_open() did not find. */
JT_EXPORT const char *jt_missing_name(void)
{
    return missing_name;
}

JT_EXPORT int jt_width(void)
{
    return CLIENT_WIDTH;
}

JT_EXPORT int jt_height(void)
{
    return CLIENT_HEIGHT;
}

/* --- Game ---------------------------------------------------------------- */

/* `seed` plays the part of srand(time(NULL)): only its low 16 bits count. */
JT_EXPORT void jt_start(unsigned seed, int speed, bool cheat)
{
    GameRect playfield = { 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT - STATUS_BAR_HEIGHT };
    GameSprites sizes;

    render_sprite_sizes(&renderer, &sizes);
    game_init(&game, playfield, &sizes, (uint16_t)seed, (GameSpeed)speed, cheat);
}

JT_EXPORT void jt_new_game(int speed, bool cheat)
{
    game_new(&game, (GameSpeed)speed, cheat);
}

/* Both return GAME_EVENT_* flags. */
JT_EXPORT unsigned jt_tick(void)
{
    return game_tick(&game);
}

JT_EXPORT unsigned jt_mouse_move(int x, int y)
{
    return game_mouse_move(&game, x, y);
}

JT_EXPORT void jt_set_focus(bool focused)
{
    game_set_focus(&game, focused);
}

JT_EXPORT int jt_level(void)
{
    return game.level;
}

/* The score as displayed, which is what the high scores hold. */
JT_EXPORT long jt_final_score(void)
{
    return (long)game.score * 100;
}

/* The original turns the cursor left only inside the playfield. */
JT_EXPORT bool jt_cursor_faces_left(int x, int y)
{
    return game_in_playfield(&game, x, y) && game.facing_left;
}

JT_EXPORT bool jt_toggle_god_mode(void)
{
    game.god_mode = !game.god_mode;
    return game.god_mode;
}

JT_EXPORT void jt_debug_change_level(int delta)
{
    game_debug_set_level(&game, game.level + delta);
}

/* --- Frame --------------------------------------------------------------- */

/*
 * Draws the frame and returns it as RGBA bytes, jt_width() x jt_height().
 * The status texts are left to the caller, which owns the fonts.
 */
JT_EXPORT const uint8_t *jt_render(void)
{
    size_t i, count = (size_t)CLIENT_WIDTH * CLIENT_HEIGHT;

    render_frame(&renderer, &frame, &game);
    for (i = 0; i < count; i++) {
        uint32_t pixel = frame.pixels[i];
        uint8_t *out = &frame_rgba[i * 4];

        out[0] = (uint8_t)(pixel >> 16);
        out[1] = (uint8_t)(pixel >> 8);
        out[2] = (uint8_t)pixel;
        out[3] = 0xFF;
    }
    render_status_text(&game, &status_text);
    return frame_rgba;
}

static const RenderText *status_entry(int index)
{
    switch (index) {
    case 0:
        return &status_text.level_name;
    case 1:
        return &status_text.lives;
    default:
        return &status_text.score;
    }
}

/* The three status texts of the last jt_render(): 0 level, 1 lives, 2 score. */
JT_EXPORT const char *jt_status_text(int index)
{
    return status_entry(index)->text;
}

JT_EXPORT int jt_status_x(int index)
{
    return status_entry(index)->x;
}

JT_EXPORT int jt_status_y(int index)
{
    return status_entry(index)->y;
}

/* --- Cursors: 0 facing right, 1 facing left ------------------------------ */

JT_EXPORT const uint8_t *jt_cursor_rgba(int index)
{
    return cursors[index].image.rgba;
}

JT_EXPORT int jt_cursor_width(int index)
{
    return cursors[index].image.width;
}

JT_EXPORT int jt_cursor_height(int index)
{
    return cursors[index].image.height;
}

JT_EXPORT int jt_cursor_hotspot_x(int index)
{
    return cursors[index].hotspot_x;
}

JT_EXPORT int jt_cursor_hotspot_y(int index)
{
    return cursors[index].hotspot_y;
}

/* --- Texts --------------------------------------------------------------- */

/* `id` is an AssetsText offset. */
JT_EXPORT const char *jt_text(int id)
{
    return assets_text(&assets, (AssetsText)id);
}

/* The STATIC texts of a dialog of the original, such as "INSTRUCTIONS". */
JT_EXPORT const char *jt_dialog_text(const char *name)
{
    if (!assets_dialog_text(&assets, name, dialog_text, sizeof(dialog_text))) {
        dialog_text[0] = '\0';
    }
    return dialog_text;
}

/* The expert ending: the format interleaves the parable's lines with CR LF pairs. */
JT_EXPORT const char *jt_ending_message(void)
{
    snprintf(ending_message, sizeof(ending_message), assets_text(&assets, TEXT_ENDING_FORMAT),
             assets_text(&assets, TEXT_PARABLE_1), '\r', '\n', assets_text(&assets, TEXT_PARABLE_2),
             '\r', '\n', assets_text(&assets, TEXT_PARABLE_3), '\r', '\n',
             assets_text(&assets, TEXT_PARABLE_SOURCE), '\r', '\n', '\r', '\n',
             assets_text(&assets, TEXT_GREAT_PRICE));
    return ending_message;
}

/* --- Settings: the SERVANT.INI values, parsed as the original does ------- */

/* NULL values, for missing keys, take the original's defaults. */
JT_EXPORT int jt_parse_speed(const char *value)
{
    char buffer[OPTION_BUFFER];

    return (int)settings_parse_speed(read_as_original(value != NULL ? value : "", buffer,
                                                      sizeof(buffer)));
}

JT_EXPORT bool jt_parse_level_wait(const char *value)
{
    char buffer[OPTION_BUFFER];

    return settings_parse_level_wait(read_as_original(value != NULL ? value : "0", buffer,
                                                      sizeof(buffer)));
}

JT_EXPORT bool jt_parse_cheat(const char *value)
{
    char buffer[OPTION_BUFFER];

    return settings_parse_cheat(read_as_original(value != NULL ? value : "", buffer,
                                                 sizeof(buffer)));
}

/* The values to store for the options, as the options dialog writes them. */
JT_EXPORT const char *jt_speed_value(int speed)
{
    return settings_speed_value((GameSpeed)speed);
}

JT_EXPORT const char *jt_flag_value(bool flag)
{
    return settings_flag_value(flag);
}

/* --- High scores: the "Scores" and "Names" values of SERVANT.INI --------- */

/* NULL values, for missing keys, take the original's defaults. */
JT_EXPORT void jt_scores_load(const char *scores, const char *names)
{
    char scores_buffer[SCORES_BUFFER];
    char names_buffer[NAMES_BUFFER];

    high_scores_parse(
        &high_scores,
        read_as_original(scores != NULL ? scores : assets_text(&assets, TEXT_DEFAULT_SCORES),
                         scores_buffer, sizeof(scores_buffer)),
        read_as_original(names != NULL ? names : assets_text(&assets, TEXT_DEFAULT_NAMES),
                         names_buffer, sizeof(names_buffer)));
}

JT_EXPORT int jt_scores_rank(long score)
{
    return high_scores_rank(&high_scores, score);
}

JT_EXPORT void jt_scores_insert(int rank, const char *name, long score)
{
    high_scores_insert(&high_scores, rank, name, score);
}

JT_EXPORT const char *jt_scores_name(int index)
{
    return high_scores.entries[index].name;
}

JT_EXPORT long jt_scores_score(int index)
{
    return high_scores.entries[index].score;
}

/* The values to store back, formatted as the original writes them. */
JT_EXPORT const char *jt_scores_value(void)
{
    high_scores_format(&high_scores, scores_value, sizeof(scores_value), names_value,
                       sizeof(names_value));
    return scores_value;
}

/* Valid after jt_scores_value(). */
JT_EXPORT const char *jt_names_value(void)
{
    return names_value;
}
