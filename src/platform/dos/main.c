/*
 * MS-DOS entry point: the game flow around the menu and the dialogs, as in
 * the Windows port. The rules live in src/core and the drawing in
 * src/render; bitmaps, cursors, dialogs, menu and texts are loaded at
 * runtime from the user's original JWLTHIEF.EXE. The Windows 3.1 screen
 * around them is redrawn by this port.
 *
 * Command line: jthief [--debug] [folder of the original JWLTHIEF.EXE]
 *
 * --debug enables keys the original never had: Home toggles god mode, and
 * Page Up / Page Down change the level. The title bar says when it is on.
 */

#include "assets.h"
#include "dialogs.h"
#include "framebuffer.h"
#include "fs.h"
#include "game.h"
#include "hardware.h"
#include "highscores.h"
#include "menu.h"
#include "profile.h"
#include "render.h"
#include "settings.h"
#include "ui.h"
#include "window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLIENT_WIDTH 498
#define CLIENT_HEIGHT 260
#define STATUS_BAR_HEIGHT 32
#define GAME_TITLE "Jewel Thief"
#define STATUS_TEXT_COLOR FB_RGB(0, 0, 0)

/* Menu commands of the original's JWLTHIEFMENU. */
enum {
    CMD_NEW_GAME = 110,
    CMD_RESTART = 115, /* not in the menu: issued after "Play again?" */
    CMD_PAUSE = 120,
    CMD_OPTIONS = 130,
    CMD_HIGH_SCORES = 140,
    CMD_EXIT = 190,
    CMD_HELP_CONTENTS = 210,
    CMD_HELP_SEARCH = 220,
    CMD_HELP_ON_HELP = 230,
    CMD_ABOUT = 240
};

/* Windows virtual key codes of F1-F12, as accelerators store them. */
#define VK_F1 0x70
#define VK_F12 0x7B
/* Alt-F4 closes a Windows window without being in its accelerator table. */
#define CLOSE_KEY_NUMBER 4

#define HIGH_SCORE_NAME_INPUT 26
#define ENDING_MESSAGE_SIZE 512

static Assets assets;
static Renderer renderer;
static Game game;
static Framebuffer frame;
static Font system_font;
static Font small_font;
static MenuTemplate menu_template;
static AcceleratorTable accelerators;
static Cursor cursor_right;
static Cursor cursor_left;
static Cursor cursor_wait;
static Cursor cursor_arrow_shape;
static bool in_tick;
static bool debug_mode;
static bool running = true;
static int pending_command;
static const char *original_dir;

static void show_error(const char *message)
{
    fprintf(stderr, "Jewel Thief: %s\n", message);
}

static void show_missing(const char *kind, const char *name)
{
    char message[128];

    snprintf(message, sizeof(message), "%s %s is missing from JWLTHIEF.EXE.", kind, name);
    show_error(message);
}

static const char *text(AssetsText id)
{
    return assets_text(&assets, id);
}

/* Arguments come in any order; anything but --debug names the original's folder. */
static void parse_command_line(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
        }
        else if (original_dir == NULL) {
            original_dir = argv[i];
        }
    }
}

static bool open_assets(void)
{
    char exe_dir[FS_MAX_PATH];
    const char *dirs[2];

    dirs[0] = original_dir;
    dirs[1] = fs_executable_dir(exe_dir, sizeof(exe_dir)) ? exe_dir : NULL;
    switch (assets_open(&assets, dirs, 2)) {
    case ASSETS_OK:
        return true;
    case ASSETS_ERR_NOT_FOUND:
        show_error("the original Jewel Thief v1.4 (JWLTHIEF.EXE) was not found.\n"
                   "Copy it next to this program, or pass the folder that holds it "
                   "on the command line.");
        return false;
    case ASSETS_ERR_UNREADABLE:
        show_error("the original JWLTHIEF.EXE was found but could not be read.");
        return false;
    case ASSETS_ERR_MALFORMED:
        show_error("the original JWLTHIEF.EXE could not be parsed.");
        return false;
    }
    return false;
}

static bool init_renderer(void)
{
    const char *missing = NULL;

    switch (render_init(&renderer, &assets, CLIENT_WIDTH, CLIENT_HEIGHT, &missing)) {
    case RENDER_OK:
        return true;
    case RENDER_ERR_MISSING_BITMAP:
        show_missing("Bitmap", missing);
        return false;
    case RENDER_ERR_OUT_OF_MEMORY:
        show_error("out of memory.");
        return false;
    }
    return false;
}

static bool load_cursor(const char *name, Cursor *cursor)
{
    NeBlob blob;

    if (!assets_cursor(&assets, name, &blob) || !cursor_from_resource(cursor, &blob)) {
        show_missing("Cursor", name);
        return false;
    }
    return true;
}

static void rename_item(uint16_t command, const char *label)
{
    int i, j;

    for (i = 0; i < menu_template.popup_count; i++) {
        MenuPopup *popup = &menu_template.popups[i];
        for (j = 0; j < popup->item_count; j++) {
            if (popup->items[j].command == command) {
                snprintf(popup->items[j].text, sizeof(popup->items[j].text), "%s", label);
            }
        }
    }
}

/*
 * The original's menu and accelerators. Its Help commands opened a WinHelp
 * file, which DOS cannot show, so they open the instructions, hints and
 * story boxes that v1.4 still carries.
 */
static bool load_menu(void)
{
    NeBlob blob;

    if (!assets_resource_named(&assets, NE_RT_MENU, "JWLTHIEFMENU", &blob)
        || !templates_read_menu(blob.data, blob.size, &menu_template)) {
        show_missing("Menu", "JWLTHIEFMENU");
        return false;
    }
    rename_item(CMD_HELP_CONTENTS, "&Instructions");
    rename_item(CMD_HELP_SEARCH, "&Hints");
    rename_item(CMD_HELP_ON_HELP, "Our &Story");
    if (assets_resource_named(&assets, NE_RT_ACCELERATOR, "JWLTHIEFMENU", &blob)) {
        templates_read_accelerators(blob.data, blob.size, &accelerators);
    }
    return true;
}

static bool load_fonts(void)
{
    if (!font_load(&system_font, 16) || !font_load(&small_font, 14)) {
        show_error("the VGA BIOS does not provide its ROM fonts; a VGA card is needed.");
        return false;
    }
    return true;
}

static void draw_status_text(const RenderText *status_text)
{
    font_draw_rgb(&system_font, &frame, status_text->x, status_text->y, status_text->text,
                  STATUS_TEXT_COLOR);
}

static void render(void)
{
    RenderStatusText status;

    render_frame(&renderer, &frame, &game);
    render_status_text(&game, &status);
    draw_status_text(&status.level_name);
    draw_status_text(&status.lives);
    draw_status_text(&status.score);
    ui_frame_changed();
}

static void redraw(void)
{
    render();
    ui_present();
}

static void init_game(void)
{
    GameRect playfield = { 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT - STATUS_BAR_HEIGHT };
    GameSettings settings = profile_load_settings();
    GameSprites sizes;

    render_sprite_sizes(&renderer, &sizes);
    /* Like srand(time(NULL)): only the low 16 bits of the time are used. */
    game_init(&game, playfield, &sizes, (uint16_t)time(NULL), settings.speed, settings.cheat);
    game_set_focus(&game, true);
}

static void wait_if_enabled(void)
{
    if (profile_load_settings().level_wait) {
        dialogs_wait();
    }
}

/* Settings are read again, as the original reads SERVANT.INI each time. */
static void new_game(void)
{
    GameSettings settings = profile_load_settings();

    game_new(&game, settings.speed, settings.cheat);
    redraw();
    wait_if_enabled();
}

/* Asks for a name if the score makes the table, then shows the table. */
static void submit_high_score(void)
{
    long score = (long)game.score * 100;
    char name[HIGH_SCORE_NAME_INPUT];
    HighScores table;
    int rank;

    profile_load_high_scores(&table, &assets);
    rank = high_scores_rank(&table, score);
    if (rank >= 0) {
        dialogs_new_high_score(score, name, sizeof(name));
        high_scores_insert(&table, rank, name, score);
    }
    profile_save_high_scores(&table);
    dialogs_high_scores(&table);
}

static void show_ending(void)
{
    char message[ENDING_MESSAGE_SIZE];

    if (profile_load_settings().speed != GAME_SPEED_EXPERT) {
        dialogs_message(text(TEXT_TRY_EXPERT), text(TEXT_TITLE), MESSAGE_OK);
        return;
    }
    /* The format interleaves the lines of the parable with CR LF pairs. */
    snprintf(message, sizeof(message), text(TEXT_ENDING_FORMAT), text(TEXT_PARABLE_1), '\r',
             '\n', text(TEXT_PARABLE_2), '\r', '\n', text(TEXT_PARABLE_3), '\r', '\n',
             text(TEXT_PARABLE_SOURCE), '\r', '\n', '\r', '\n', text(TEXT_GREAT_PRICE));
    dialogs_message(message, text(TEXT_TITLE), MESSAGE_OK);
}

static void finish_game(unsigned events)
{
    redraw();
    if (events & GAME_EVENT_GAME_COMPLETED) {
        show_ending();
    }
    submit_high_score();
    if (dialogs_message(text(TEXT_PLAY_AGAIN), text(TEXT_TITLE), MESSAGE_YES_NO) == ID_YES) {
        pending_command = CMD_RESTART;
    }
    else {
        pending_command = CMD_EXIT;
    }
}

static void handle_events(unsigned events)
{
    if (events & GAME_EVENT_LIFE_LOST) {
        speaker_beep();
    }
    if (events & (GAME_EVENT_GAME_OVER | GAME_EVENT_GAME_COMPLETED)) {
        finish_game(events);
        return;
    }
    /* As in the original: an hourglass while the next level is set up. */
    if (events & GAME_EVENT_LEVEL_UP) {
        ui_set_cursor(&cursor_wait);
    }
    if (events != 0) {
        redraw();
    }
    if (events & GAME_EVENT_LEVEL_UP) {
        /* A new scenery comes every second level; the original may pause there. */
        if (game.level % 2 == 0) {
            wait_if_enabled();
        }
        ui_set_cursor(&cursor_right);
    }
}

static void handle_tick(void)
{
    /* Dialogs opened during a tick keep the clock running; skip those ticks. */
    if (in_tick) {
        return;
    }
    in_tick = true;
    handle_events(game_tick(&game));
    render();
    in_tick = false;
}

static void on_focus(bool focused)
{
    game_set_focus(&game, focused);
}

static void show_options(void)
{
    GameSettings settings = profile_load_settings();

    if (dialogs_options(&settings)) {
        profile_save_settings(&settings);
        new_game();
    }
}

static void show_high_scores(void)
{
    HighScores table;

    profile_load_high_scores(&table, &assets);
    dialogs_high_scores(&table);
}

static void handle_command(int command)
{
    switch (command) {
    case CMD_NEW_GAME:
        if (dialogs_message(text(TEXT_TRADE_JEWELS), text(TEXT_TITLE), MESSAGE_YES_NO) == ID_YES) {
            new_game();
        }
        break;
    case CMD_RESTART:
        new_game();
        break;
    case CMD_PAUSE:
        dialogs_pause();
        break;
    case CMD_OPTIONS:
        show_options();
        break;
    case CMD_HIGH_SCORES:
        show_high_scores();
        break;
    case CMD_EXIT:
        running = false;
        break;
    case CMD_HELP_CONTENTS:
        dialogs_instructions();
        break;
    case CMD_HELP_SEARCH:
        dialogs_hints();
        break;
    case CMD_HELP_ON_HELP:
        dialogs_story();
        break;
    case CMD_ABOUT:
        dialogs_about();
        break;
    }
}

static void update_title(void)
{
    const char *title = GAME_TITLE;

    if (debug_mode) {
        title = game.god_mode ? GAME_TITLE " [DEBUG: god mode]" : GAME_TITLE " [DEBUG]";
    }
    window_set_title(title);
}

static bool handle_debug_key(const Key *key)
{
    switch (key->scan) {
    case SCAN_HOME:
        game.god_mode = !game.god_mode;
        update_title();
        return true;
    case SCAN_PAGE_UP:
    case SCAN_PAGE_DOWN:
        game_debug_set_level(&game, game.level + (key->scan == SCAN_PAGE_UP ? 1 : -1));
        redraw();
        return true;
    }
    return false;
}

/* The command of the accelerator `key` stands for, or 0. Only function keys are used. */
static int accelerator_command(const Key *key)
{
    int number = keyboard_function_number(key);
    int i;

    for (i = 0; i < accelerators.count && number != 0; i++) {
        const Accelerator *entry = &accelerators.entries[i];
        bool wants_shift = (entry->flags & ACCEL_SHIFT) != 0;
        bool wants_ctrl = (entry->flags & ACCEL_CONTROL) != 0;
        bool wants_alt = (entry->flags & ACCEL_ALT) != 0;

        if ((entry->flags & ACCEL_VIRTUAL_KEY) && entry->key >= VK_F1 && entry->key <= VK_F12
            && entry->key - VK_F1 + 1 == number && wants_shift == key->shift
            && wants_ctrl == key->ctrl && wants_alt == key->alt) {
            return entry->command;
        }
    }
    return 0;
}

static void handle_key(const Key *key)
{
    int command;
    int item;

    if (key->alt && keyboard_function_number(key) == CLOSE_KEY_NUMBER) {
        handle_command(CMD_EXIT);
        return;
    }
    if ((command = accelerator_command(key)) != 0) {
        handle_command(command);
        return;
    }
    if (key->scan == SCAN_F10 && !key->alt && !key->shift && !key->ctrl) {
        handle_command(menu_track(0, false, false));
        return;
    }
    if (key->alt && (item = menu_bar_item_for(keyboard_letter(key))) >= 0) {
        handle_command(menu_track(item, true, false));
        return;
    }
    if (debug_mode) {
        handle_debug_key(key);
    }
}

static void update_cursor(int x, int y)
{
    Rect client = window_client();

    if (!rect_contains(client, x, y)) {
        ui_set_cursor(&cursor_arrow_shape);
        return;
    }
    /* The class cursor faces right; inside the playfield it turns with the thief. */
    if (game_in_playfield(&game, x - client.left, y - client.top) && game.facing_left) {
        ui_set_cursor(&cursor_left);
    }
    else {
        ui_set_cursor(&cursor_right);
    }
}

/* Only moves over the client area reach the game, as WM_MOUSEMOVE did. */
static void handle_mouse_move(int x, int y)
{
    Rect client = window_client();
    unsigned events;

    if (!rect_contains(client, x, y)) {
        update_cursor(x, y);
        return;
    }
    events = game_mouse_move(&game, x - client.left, y - client.top);
    /* The original turns the cursor before handling a jewel or a level-up. */
    update_cursor(x, y);
    handle_events(events);
}

static void handle_mouse_down(int x, int y)
{
    int item = menu_bar_item_at(x, y);

    if (item >= 0) {
        handle_command(menu_track(item, true, true));
    }
}

static void run_main_loop(void)
{
    Event event;

    while (running) {
        if (pending_command != 0) {
            int command = pending_command;
            pending_command = 0;
            handle_command(command);
            continue;
        }
        ui_wait_event(&event);
        switch (event.type) {
        case EVENT_TICK:
            break;
        case EVENT_MOUSE_MOVE:
            handle_mouse_move(event.x, event.y);
            break;
        case EVENT_MOUSE_DOWN:
            handle_mouse_down(event.x, event.y);
            break;
        case EVENT_MOUSE_UP:
            break;
        case EVENT_KEY:
            handle_key(&event.key);
            break;
        case EVENT_ALT:
            handle_command(menu_track(0, false, false));
            break;
        }
    }
}

/*
 * Startup as in the original, minus the shareware About screen: the game is
 * dealt before the window shows, and the "Get ready" box may follow.
 */
static int run(void)
{
    int x, y;

    init_game();
    render();
    window_open(&system_font, GAME_TITLE);
    menu_open(&menu_template, &system_font);
    dialogs_init(&assets, &system_font, &small_font);
    dialog_set_owner_focus_handler(on_focus);
    if (!ui_open(window_paint, &frame, window_client())) {
        show_error("out of memory.");
        return EXIT_FAILURE;
    }
    update_title();
    ui_mouse(&x, &y);
    update_cursor(x, y);
    ui_set_tick_handler(handle_tick);
    ui_present();
    wait_if_enabled();
    run_main_loop();
    ui_set_tick_handler(NULL);
    ui_close();
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    int exit_code = EXIT_FAILURE;

    parse_command_line(argc, argv);
    if (!open_assets()) {
        return EXIT_FAILURE;
    }
    if (!mouse_open(CANVAS_WIDTH, CANVAS_HEIGHT)) {
        show_error("no mouse driver is loaded. Load one, such as CTMOUSE or MOUSE.COM, "
                   "and try again.");
    }
    else if (!profile_init(assets.dir)) {
        show_error("could not initialize the game.");
    }
    else if (!fb_create(&frame, CLIENT_WIDTH, CLIENT_HEIGHT)) {
        show_error("out of memory.");
    }
    else if (load_fonts() && init_renderer() && load_menu()
             && load_cursor("RIGHTCUR", &cursor_right) && load_cursor("LEFTCUR", &cursor_left)) {
        cursor_hourglass(&cursor_wait);
        cursor_arrow(&cursor_arrow_shape);
        exit_code = run();
    }
    render_destroy(&renderer);
    fb_destroy(&frame);
    assets_close(&assets);
    return exit_code;
}
