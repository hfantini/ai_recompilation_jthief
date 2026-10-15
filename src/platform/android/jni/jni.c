/*
 * The native side of the Android port: the JNI functions behind the Kotlin
 * object io.github.hfantini.jthief.Native. As in the browser port, the
 * platform owns the loop and this file only answers calls: open the
 * original, feed ticks and moves, render a frame.
 *
 * Android has a POSIX file system, so the original is found by its hash in
 * the app's private directory, and SERVANT.INI lives next to it, read and
 * written by the same profile code as the Linux port.
 *
 * Strings cross JNI as byte arrays in the original's Windows-1252: JNI
 * strings are modified UTF-8 and would mangle its accented characters.
 * Every call comes from the app's main thread.
 */

#include "assets.h"
#include "framebuffer.h"
#include "game.h"
#include "highscores.h"
#include "icon.h"
#include "profile.h"
#include "render.h"
#include "settings.h"

#include <android/bitmap.h>
#include <jni.h>

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
#define NAME_SIZE HIGH_SCORE_NAME_SIZE

#define JNI_NAME(name) Java_io_github_hfantini_jthief_Native_##name

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
static char missing_name[64];

static jbyteArray to_bytes(JNIEnv *env, const char *text)
{
    jsize length = (jsize)strlen(text);
    jbyteArray bytes = (*env)->NewByteArray(env, length);

    if (bytes != NULL) {
        (*env)->SetByteArrayRegion(env, bytes, 0, length, (const jbyte *)text);
    }
    return bytes;
}

/* Copies `bytes` into `out` as a C string, cut to fit. */
static void from_bytes(JNIEnv *env, jbyteArray bytes, char *out, size_t size)
{
    jsize length = (*env)->GetArrayLength(env, bytes);

    if ((size_t)length >= size) {
        length = (jsize)(size - 1);
    }
    (*env)->GetByteArrayRegion(env, bytes, 0, length, (jbyte *)out);
    out[length] = '\0';
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

/* Fills an RGBA_8888 bitmap of the given size from 0x00RRGGBB or RGBA pixels. */
static bool fill_bitmap(JNIEnv *env, jobject bitmap, int width, int height,
                        const uint32_t *words, const uint8_t *rgba)
{
    AndroidBitmapInfo info;
    uint8_t *pixels;
    int x, y;

    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS
        || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 || (int)info.width != width
        || (int)info.height != height
        || AndroidBitmap_lockPixels(env, bitmap, (void **)&pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
        return false;
    }
    for (y = 0; y < height; y++) {
        uint8_t *row = pixels + (size_t)y * info.stride;

        for (x = 0; x < width; x++) {
            size_t i = (size_t)y * (size_t)width + (size_t)x;
            uint8_t *out = row + (size_t)x * 4;

            if (words != NULL) {
                out[0] = (uint8_t)(words[i] >> 16);
                out[1] = (uint8_t)(words[i] >> 8);
                out[2] = (uint8_t)words[i];
                out[3] = 0xFF;
            }
            else {
                /* Android bitmaps hold premultiplied alpha. */
                const uint8_t *in = rgba + i * 4;

                out[0] = (uint8_t)(in[0] * in[3] / 255);
                out[1] = (uint8_t)(in[1] * in[3] / 255);
                out[2] = (uint8_t)(in[2] * in[3] / 255);
                out[3] = in[3];
            }
        }
    }
    AndroidBitmap_unlockPixels(env, bitmap);
    return true;
}

/* --- Startup ------------------------------------------------------------- */

/* Opens the original found in `dir`, whose SERVANT.INI then holds the settings. Called once. */
JNIEXPORT jint JNICALL JNI_NAME(open)(JNIEnv *env, jclass type, jstring dir)
{
    const char *path = (*env)->GetStringUTFChars(env, dir, NULL);
    const char *dirs[1];
    AssetsStatus opened;
    JtStatus status;
    int i;

    (void)type;
    if (path == NULL) {
        return JT_ERR_OUT_OF_MEMORY;
    }
    dirs[0] = path;
    opened = assets_open(&assets, dirs, 1);
    (*env)->ReleaseStringUTFChars(env, dir, path);
    switch (opened) {
    case ASSETS_OK:
        break;
    case ASSETS_ERR_NOT_FOUND:
        return JT_ERR_NOT_ORIGINAL;
    case ASSETS_ERR_UNREADABLE:
    case ASSETS_ERR_MALFORMED:
        return JT_ERR_MALFORMED;
    }
    if (!profile_init(assets.dir) || !fb_create(&frame, CLIENT_WIDTH, CLIENT_HEIGHT)) {
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

/* The bitmap or cursor a failed open() did not find. */
JNIEXPORT jstring JNICALL JNI_NAME(missingName)(JNIEnv *env, jclass type)
{
    (void)type;
    return (*env)->NewStringUTF(env, missing_name);
}

JNIEXPORT jint JNICALL JNI_NAME(width)(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    return CLIENT_WIDTH;
}

JNIEXPORT jint JNICALL JNI_NAME(height)(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    return CLIENT_HEIGHT;
}

/* --- Game ---------------------------------------------------------------- */

/* `seed` plays the part of srand(time(NULL)): only its low 16 bits count. */
JNIEXPORT void JNICALL JNI_NAME(start)(JNIEnv *env, jclass type, jint seed)
{
    GameRect playfield = { 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT - STATUS_BAR_HEIGHT };
    GameSettings settings = profile_load_settings();
    GameSprites sizes;

    (void)env;
    (void)type;
    render_sprite_sizes(&renderer, &sizes);
    game_init(&game, playfield, &sizes, (uint16_t)seed, settings.speed, settings.cheat);
}

/* Settings are read again, as the original reads SERVANT.INI each time. */
JNIEXPORT void JNICALL JNI_NAME(newGame)(JNIEnv *env, jclass type)
{
    GameSettings settings = profile_load_settings();

    (void)env;
    (void)type;
    game_new(&game, settings.speed, settings.cheat);
}

/* Both return GAME_EVENT_* flags. */
JNIEXPORT jint JNICALL JNI_NAME(tick)(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    return (jint)game_tick(&game);
}

JNIEXPORT jint JNICALL JNI_NAME(mouseMove)(JNIEnv *env, jclass type, jint x, jint y)
{
    (void)env;
    (void)type;
    return (jint)game_mouse_move(&game, x, y);
}

JNIEXPORT void JNICALL JNI_NAME(setFocus)(JNIEnv *env, jclass type, jboolean focused)
{
    (void)env;
    (void)type;
    game_set_focus(&game, focused);
}

JNIEXPORT jint JNICALL JNI_NAME(level)(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    return game.level;
}

/* The score as displayed, which is what the high scores hold. */
JNIEXPORT jint JNICALL JNI_NAME(finalScore)(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    return (jint)((long)game.score * 100);
}

/* The original turns the cursor left only inside the playfield. */
JNIEXPORT jboolean JNICALL JNI_NAME(cursorFacesLeft)(JNIEnv *env, jclass type, jint x, jint y)
{
    (void)env;
    (void)type;
    return game_in_playfield(&game, x, y) && game.facing_left;
}

JNIEXPORT jboolean JNICALL JNI_NAME(toggleGodMode)(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    game.god_mode = !game.god_mode;
    return game.god_mode;
}

JNIEXPORT void JNICALL JNI_NAME(changeLevel)(JNIEnv *env, jclass type, jint delta)
{
    (void)env;
    (void)type;
    game_debug_set_level(&game, game.level + delta);
}

/* --- Frame --------------------------------------------------------------- */

/*
 * Draws the frame into an RGBA_8888 bitmap of width() x height(). The status
 * texts are left to the caller, which owns the fonts.
 */
JNIEXPORT jboolean JNICALL JNI_NAME(render)(JNIEnv *env, jclass type, jobject bitmap)
{
    (void)type;
    render_frame(&renderer, &frame, &game);
    render_status_text(&game, &status_text);
    return fill_bitmap(env, bitmap, CLIENT_WIDTH, CLIENT_HEIGHT, frame.pixels, NULL);
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

/* The three status texts of the last render(): 0 level, 1 lives, 2 score. */
JNIEXPORT jbyteArray JNICALL JNI_NAME(statusText)(JNIEnv *env, jclass type, jint index)
{
    (void)type;
    return to_bytes(env, status_entry(index)->text);
}

JNIEXPORT jint JNICALL JNI_NAME(statusX)(JNIEnv *env, jclass type, jint index)
{
    (void)env;
    (void)type;
    return status_entry(index)->x;
}

JNIEXPORT jint JNICALL JNI_NAME(statusY)(JNIEnv *env, jclass type, jint index)
{
    (void)env;
    (void)type;
    return status_entry(index)->y;
}

/* --- Cursors: 0 facing right, 1 facing left ------------------------------ */

JNIEXPORT jint JNICALL JNI_NAME(cursorWidth)(JNIEnv *env, jclass type, jint index)
{
    (void)env;
    (void)type;
    return cursors[index].image.width;
}

JNIEXPORT jint JNICALL JNI_NAME(cursorHeight)(JNIEnv *env, jclass type, jint index)
{
    (void)env;
    (void)type;
    return cursors[index].image.height;
}

JNIEXPORT jint JNICALL JNI_NAME(cursorHotspotX)(JNIEnv *env, jclass type, jint index)
{
    (void)env;
    (void)type;
    return cursors[index].hotspot_x;
}

JNIEXPORT jint JNICALL JNI_NAME(cursorHotspotY)(JNIEnv *env, jclass type, jint index)
{
    (void)env;
    (void)type;
    return cursors[index].hotspot_y;
}

/* Draws the cursor into an RGBA_8888 bitmap of cursorWidth() x cursorHeight(). */
JNIEXPORT jboolean JNICALL JNI_NAME(cursorImage)(JNIEnv *env, jclass type, jint index,
                                                 jobject bitmap)
{
    const IconImage *image = &cursors[index].image;

    (void)type;
    return fill_bitmap(env, bitmap, image->width, image->height, NULL, image->rgba);
}

/* --- Texts --------------------------------------------------------------- */

/* `id` is an AssetsText offset. */
JNIEXPORT jbyteArray JNICALL JNI_NAME(text)(JNIEnv *env, jclass type, jint id)
{
    (void)type;
    return to_bytes(env, assets_text(&assets, (AssetsText)id));
}

/* The STATIC texts of a dialog of the original, such as "INSTRBOX"; empty if missing. */
JNIEXPORT jbyteArray JNICALL JNI_NAME(dialogText)(JNIEnv *env, jclass type, jstring name)
{
    char text[DIALOG_TEXT_SIZE];
    const char *template_name = (*env)->GetStringUTFChars(env, name, NULL);
    bool found;

    (void)type;
    if (template_name == NULL) {
        return NULL;
    }
    found = assets_dialog_text(&assets, template_name, text, sizeof(text));
    (*env)->ReleaseStringUTFChars(env, name, template_name);
    return to_bytes(env, found ? text : "");
}

/* The expert ending: the format interleaves the parable's lines with CR LF pairs. */
JNIEXPORT jbyteArray JNICALL JNI_NAME(endingMessage)(JNIEnv *env, jclass type)
{
    char message[ENDING_MESSAGE_SIZE];

    (void)type;
    snprintf(message, sizeof(message), assets_text(&assets, TEXT_ENDING_FORMAT),
             assets_text(&assets, TEXT_PARABLE_1), '\r', '\n', assets_text(&assets, TEXT_PARABLE_2),
             '\r', '\n', assets_text(&assets, TEXT_PARABLE_3), '\r', '\n',
             assets_text(&assets, TEXT_PARABLE_SOURCE), '\r', '\n', '\r', '\n',
             assets_text(&assets, TEXT_GREAT_PRICE));
    return to_bytes(env, message);
}

/* --- Settings in SERVANT.INI --------------------------------------------- */

/* Speed, level wait (0 or 1) and cheat (0 or 1). */
JNIEXPORT jintArray JNICALL JNI_NAME(loadSettings)(JNIEnv *env, jclass type)
{
    GameSettings settings = profile_load_settings();
    jint values[3];
    jintArray array = (*env)->NewIntArray(env, 3);

    (void)type;
    values[0] = (jint)settings.speed;
    values[1] = settings.level_wait;
    values[2] = settings.cheat;
    if (array != NULL) {
        (*env)->SetIntArrayRegion(env, array, 0, 3, values);
    }
    return array;
}

JNIEXPORT void JNICALL JNI_NAME(saveSettings)(JNIEnv *env, jclass type, jint speed,
                                              jboolean level_wait, jboolean cheat)
{
    GameSettings settings;

    (void)env;
    (void)type;
    settings.speed = (GameSpeed)speed;
    settings.level_wait = level_wait;
    settings.cheat = cheat;
    profile_save_settings(&settings);
}

/* --- High scores in SERVANT.INI ------------------------------------------ */

JNIEXPORT void JNICALL JNI_NAME(loadHighScores)(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    profile_load_high_scores(&high_scores, &assets);
}

/* Saved even when a score misses the table, as the original does. */
JNIEXPORT void JNICALL JNI_NAME(saveHighScores)(JNIEnv *env, jclass type)
{
    (void)env;
    (void)type;
    profile_save_high_scores(&high_scores);
}

/* Place the score would take in the loaded table (0 is the best), or -1. */
JNIEXPORT jint JNICALL JNI_NAME(highScoreRank)(JNIEnv *env, jclass type, jint score)
{
    (void)env;
    (void)type;
    return high_scores_rank(&high_scores, score);
}

JNIEXPORT void JNICALL JNI_NAME(insertHighScore)(JNIEnv *env, jclass type, jint rank,
                                                 jbyteArray name, jint score)
{
    char text[NAME_SIZE];

    (void)type;
    from_bytes(env, name, text, sizeof(text));
    high_scores_insert(&high_scores, rank, text, score);
}

JNIEXPORT jbyteArray JNICALL JNI_NAME(highScoreName)(JNIEnv *env, jclass type, jint index)
{
    (void)type;
    return to_bytes(env, high_scores.entries[index].name);
}

JNIEXPORT jint JNICALL JNI_NAME(highScoreScore)(JNIEnv *env, jclass type, jint index)
{
    (void)env;
    (void)type;
    return (jint)high_scores.entries[index].score;
}
