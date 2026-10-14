/*
 * GTK 4 entry point: window, menu, input, timing, text and the game flow
 * around the dialogs. The rules live in src/core and the drawing in
 * src/render; bitmaps, cursors and texts are loaded at runtime from the
 * user's original JWLTHIEF.EXE.
 *
 * Only the gameplay follows the original: pace, input, cursors and what the
 * guards do while a dialog is open. Menus and dialogs are native GTK, and
 * since GTK dialogs do not block, the flows the Win32 port runs in one go
 * (such as game over, high score and "Play again?") are chained callbacks.
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
#include "highscores.h"
#include "icon.h"
#include "profile.h"
#include "render.h"
#include "settings.h"
#include "ticker.h"

#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The Win32 port's client area, which is the original's Windows 3.1 one. */
#define CLIENT_WIDTH 498
#define CLIENT_HEIGHT 260
#define STATUS_BAR_HEIGHT 32
#define GAME_TITLE "Jewel Thief"

/*
 * Windows 3.1 timers ran on the PC timer chip at 1193182 / 65536 Hz, so the
 * game's 5 ms timer really fired every ~54.9 ms; that is the game's pace.
 */
#define TICK_SECONDS (65536.0 / 1193182.0)

/* Close to the Windows System font the original prints the status bar with. */
#define STATUS_FONT "Sans Bold"
#define STATUS_FONT_PIXELS 12

/* A Windows 3.x cursor resource starts with its hotspot, then the image. */
#define CURSOR_HOTSPOT_SIZE 4

#define ENDING_MESSAGE_SIZE 512

static Assets assets;
static Renderer renderer;
static Game game;
static GtkApplication *application;
static GtkWindow *main_window;
static GtkWidget *game_area;
static GdkCursor *cursor_right;
static GdkCursor *cursor_left;
static GdkCursor *cursor_wait;
static bool debug_mode;
static const char *original_dir;
static bool assets_loaded;
static bool renderer_ready;
static bool ticker_running;

/* The frame is a cairo image, so the status text can be drawn into it too. */
static cairo_surface_t *frame_surface;
static Framebuffer frame;

static const char *text(AssetsText id)
{
    return assets_text(&assets, id);
}

static void quit_after_error(void)
{
    g_application_release(G_APPLICATION(application));
}

/* For errors the game cannot go on after: it ends once the message is closed. */
static void show_error(const char *message)
{
    fprintf(stderr, "jthief: %s\n", message);
    g_application_hold(G_APPLICATION(application));
    dialogs_message(message, quit_after_error);
}

static void show_missing(const char *kind, const char *name)
{
    char message[128];

    snprintf(message, sizeof(message), "%s %s is missing from JWLTHIEF.EXE.", kind, name);
    show_error(message);
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
        assets_loaded = true;
        return true;
    case ASSETS_ERR_NOT_FOUND:
        show_error("The original Jewel Thief v1.4 (JWLTHIEF.EXE) was not found.\n\n"
                   "Copy it next to this program, or pass the folder that holds it "
                   "on the command line.");
        return false;
    case ASSETS_ERR_UNREADABLE:
        show_error("The original JWLTHIEF.EXE was found but could not be read.");
        return false;
    case ASSETS_ERR_MALFORMED:
        show_error("The original JWLTHIEF.EXE could not be parsed.");
        return false;
    }
    return false;
}

static bool init_renderer(void)
{
    const char *missing = NULL;

    switch (render_init(&renderer, &assets, CLIENT_WIDTH, CLIENT_HEIGHT, &missing)) {
    case RENDER_OK:
        renderer_ready = true;
        return true;
    case RENDER_ERR_MISSING_BITMAP:
        show_missing("Bitmap", missing);
        return false;
    case RENDER_ERR_OUT_OF_MEMORY:
        show_error("Out of memory.");
        return false;
    }
    return false;
}

static GdkCursor *load_cursor(const char *name)
{
    GdkCursor *cursor = NULL;
    NeBlob blob;
    IconImage image;

    if (!assets_cursor(&assets, name, &blob)) {
        show_missing("Cursor", name);
        return NULL;
    }
    if (blob.size > CURSOR_HOTSPOT_SIZE
        && icon_decode(blob.data + CURSOR_HOTSPOT_SIZE, blob.size - CURSOR_HOTSPOT_SIZE, &image)) {
        int hotspot_x = blob.data[0] | (blob.data[1] << 8);
        int hotspot_y = blob.data[2] | (blob.data[3] << 8);
        GBytes *bytes = g_bytes_new(image.rgba, (gsize)image.width * (gsize)image.height * 4);
        GdkTexture *texture = gdk_memory_texture_new(image.width, image.height,
                                                     GDK_MEMORY_R8G8B8A8, bytes,
                                                     (gsize)image.width * 4);

        cursor = gdk_cursor_new_from_texture(texture, hotspot_x, hotspot_y, NULL);
        g_object_unref(texture);
        g_bytes_unref(bytes);
        icon_free(&image);
    }
    if (cursor == NULL) {
        show_missing("Usable cursor", name);
    }
    return cursor;
}

static bool create_frame(void)
{
    frame_surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, CLIENT_WIDTH, CLIENT_HEIGHT);
    /* RGB24 pixels are 0x00RRGGBB words, the framebuffer's own format. */
    if (cairo_surface_status(frame_surface) != CAIRO_STATUS_SUCCESS
        || cairo_image_surface_get_stride(frame_surface) != CLIENT_WIDTH * 4) {
        return false;
    }
    fb_wrap(&frame, (uint32_t *)cairo_image_surface_get_data(frame_surface), CLIENT_WIDTH,
            CLIENT_HEIGHT);
    return true;
}

static void destroy_frame(void)
{
    fb_destroy(&frame);
    if (frame_surface != NULL) {
        cairo_surface_destroy(frame_surface);
        frame_surface = NULL;
    }
}

static void draw_text(cairo_t *cr, PangoLayout *layout, const RenderText *status_text)
{
    pango_layout_set_text(layout, status_text->text, -1);
    cairo_move_to(cr, status_text->x, status_text->y);
    pango_cairo_show_layout(cr, layout);
}

static void render(void)
{
    RenderStatusText status;
    PangoFontDescription *font;
    PangoLayout *layout;
    cairo_t *cr;

    cairo_surface_flush(frame_surface);
    render_frame(&renderer, &frame, &game);
    cairo_surface_mark_dirty(frame_surface);

    render_status_text(&game, &status);
    cr = cairo_create(frame_surface);
    layout = pango_cairo_create_layout(cr);
    font = pango_font_description_from_string(STATUS_FONT);
    pango_font_description_set_absolute_size(font, STATUS_FONT_PIXELS * PANGO_SCALE);
    pango_layout_set_font_description(layout, font);
    cairo_set_source_rgb(cr, 0, 0, 0);
    draw_text(cr, layout, &status.level_name);
    draw_text(cr, layout, &status.lives);
    draw_text(cr, layout, &status.score);
    pango_font_description_free(font);
    g_object_unref(layout);
    cairo_destroy(cr);
}

static void redraw(void)
{
    if (game_area == NULL) {
        return;
    }
    render();
    gtk_widget_queue_draw(game_area);
}

static void draw_game(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer unused)
{
    (void)area;
    (void)width;
    (void)height;
    (void)unused;
    cairo_set_source_surface(cr, frame_surface, 0, 0);
    /* Scaled displays must not blur the pixel art. */
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
}

static void init_game(void)
{
    GameRect playfield = { 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT - STATUS_BAR_HEIGHT };
    GameSettings settings = profile_load_settings();
    GameSprites sizes;

    render_sprite_sizes(&renderer, &sizes);
    /* Like srand(time(NULL)): only the low 16 bits of the time are used. */
    game_init(&game, playfield, &sizes, (uint16_t)time(NULL), settings.speed, settings.cheat);
}

static void wait_if_enabled(void)
{
    if (profile_load_settings().level_wait) {
        dialogs_wait(NULL);
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

static void on_play_again(bool yes)
{
    if (yes) {
        new_game();
    }
    else {
        gtk_window_destroy(main_window);
    }
}

static void ask_play_again(void)
{
    dialogs_question(text(TEXT_PLAY_AGAIN), on_play_again);
}

static long final_score(void)
{
    return (long)game.score * 100;
}

/* Saved even when the score misses the table, as the original does. */
static void on_high_score_name(const char *name)
{
    HighScores table;
    int rank;

    profile_load_high_scores(&table, &assets);
    rank = high_scores_rank(&table, final_score());
    high_scores_insert(&table, rank, name, final_score());
    profile_save_high_scores(&table);
    dialogs_high_scores(&table, ask_play_again);
}

/* Asks for a name if the score makes the table, then shows the table. */
static void submit_high_score(void)
{
    HighScores table;

    profile_load_high_scores(&table, &assets);
    if (high_scores_rank(&table, final_score()) >= 0) {
        dialogs_new_high_score(final_score(), on_high_score_name);
        return;
    }
    profile_save_high_scores(&table);
    dialogs_high_scores(&table, ask_play_again);
}

static void show_ending(void)
{
    char message[ENDING_MESSAGE_SIZE];

    if (profile_load_settings().speed != GAME_SPEED_EXPERT) {
        dialogs_message(text(TEXT_TRY_EXPERT), submit_high_score);
        return;
    }
    /* The format interleaves the lines of the parable with CR LF pairs. */
    snprintf(message, sizeof(message), text(TEXT_ENDING_FORMAT), text(TEXT_PARABLE_1), '\r',
             '\n', text(TEXT_PARABLE_2), '\r', '\n', text(TEXT_PARABLE_3), '\r', '\n',
             text(TEXT_PARABLE_SOURCE), '\r', '\n', '\r', '\n', text(TEXT_GREAT_PRICE));
    dialogs_message(message, submit_high_score);
}

static void finish_game(unsigned events)
{
    redraw();
    if (events & GAME_EVENT_GAME_COMPLETED) {
        show_ending();
    }
    else {
        submit_high_score();
    }
}

/* As in the original: an hourglass while the "Get ready" box is up. */
static void on_level_wait_done(void)
{
    gtk_widget_set_cursor(game_area, cursor_right);
}

static void handle_events(unsigned events)
{
    if (events & GAME_EVENT_LIFE_LOST) {
        gdk_display_beep(gtk_widget_get_display(game_area));
    }
    if (events & (GAME_EVENT_GAME_OVER | GAME_EVENT_GAME_COMPLETED)) {
        finish_game(events);
        return;
    }
    if (events != 0) {
        redraw();
    }
    /* A new scenery comes every second level; the original may pause there. */
    if ((events & GAME_EVENT_LEVEL_UP) && game.level % 2 == 0
        && profile_load_settings().level_wait) {
        gtk_widget_set_cursor(game_area, cursor_wait);
        dialogs_wait(on_level_wait_done);
    }
}

/*
 * Windows restored the class cursor (facing right) on every move and the
 * original turned it inside the playfield only; GTK keeps the last cursor.
 */
static void update_cursor(int x, int y)
{
    bool facing_left = game_in_playfield(&game, x, y) && game.facing_left;

    gtk_widget_set_cursor(game_area, facing_left ? cursor_left : cursor_right);
}

static void handle_mouse_move(double x, double y)
{
    int ix = (int)x, iy = (int)y;
    unsigned events;

    /* The original's main window gets no mouse input under a modal dialog. */
    if (dialogs_open()) {
        return;
    }
    events = game_mouse_move(&game, ix, iy);
    /* The original turns the cursor before handling a jewel or a level-up. */
    update_cursor(ix, iy);
    handle_events(events);
}

static void on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer unused)
{
    (void)controller;
    (void)unused;
    handle_mouse_move(x, y);
}

static void handle_tick(void)
{
    ticker_acknowledge();
    /* A tick queued just before the window closed. */
    if (game_area == NULL) {
        return;
    }
    handle_events(game_tick(&game));
    redraw();
}

static void on_active_changed(GObject *window, GParamSpec *property, gpointer unused)
{
    (void)property;
    (void)unused;
    game_set_focus(&game, gtk_window_is_active(GTK_WINDOW(window)));
}

static void update_title(void)
{
    const char *title = GAME_TITLE;

    if (debug_mode) {
        title = game.god_mode ? GAME_TITLE " [DEBUG: god mode]" : GAME_TITLE " [DEBUG]";
    }
    gtk_window_set_title(main_window, title);
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint key, guint code,
                               GdkModifierType modifiers, gpointer unused)
{
    (void)controller;
    (void)code;
    (void)modifiers;
    (void)unused;
    if (!debug_mode || dialogs_open()) {
        return FALSE;
    }
    switch (key) {
    case GDK_KEY_Home:
        game.god_mode = !game.god_mode;
        update_title();
        return TRUE;
    case GDK_KEY_Page_Up:
    case GDK_KEY_Page_Down:
        game_debug_set_level(&game, game.level + (key == GDK_KEY_Page_Up ? 1 : -1));
        redraw();
        return TRUE;
    }
    return FALSE;
}

static void on_trade_jewels(bool yes)
{
    if (yes) {
        new_game();
    }
}

static void on_options_done(const GameSettings *settings)
{
    if (settings != NULL) {
        profile_save_settings(settings);
        new_game();
    }
}

static void show_high_scores(void)
{
    HighScores table;

    profile_load_high_scores(&table, &assets);
    dialogs_high_scores(&table, NULL);
}

static void run_action(GSimpleAction *action, GVariant *parameter, gpointer unused)
{
    const char *name = g_action_get_name(G_ACTION(action));
    GameSettings settings;

    (void)parameter;
    (void)unused;
    /* Menus of a window under a modal dialog do not respond on Windows either. */
    if (dialogs_open()) {
        return;
    }
    if (strcmp(name, "new-game") == 0) {
        dialogs_question(text(TEXT_TRADE_JEWELS), on_trade_jewels);
    }
    else if (strcmp(name, "pause") == 0) {
        dialogs_pause();
    }
    else if (strcmp(name, "options") == 0) {
        settings = profile_load_settings();
        dialogs_options(&settings, on_options_done);
    }
    else if (strcmp(name, "high-scores") == 0) {
        show_high_scores();
    }
    else if (strcmp(name, "quit") == 0) {
        gtk_window_destroy(main_window);
    }
    else if (strcmp(name, "instructions") == 0) {
        dialogs_instructions();
    }
    else if (strcmp(name, "hints") == 0) {
        dialogs_hints();
    }
    else if (strcmp(name, "story") == 0) {
        dialogs_story();
    }
    else if (strcmp(name, "about") == 0) {
        dialogs_about();
    }
}

static void add_menu_item(GMenu *section, const char *label, const char *action)
{
    char detailed[64];

    snprintf(detailed, sizeof(detailed), "app.%s", action);
    g_menu_append(section, label, detailed);
}

/* The original's menu, with the Help commands its v1.4 boxes can still answer. */
static void create_menu(void)
{
    static const char *const ACTIONS[] = { "new-game", "pause",        "options", "high-scores",
                                           "quit",     "instructions", "hints",   "story",
                                           "about" };
    static const char *const NEW_GAME_KEYS[] = { "F2", NULL };
    static const char *const PAUSE_KEYS[] = { "F3", NULL };
    static const char *const QUIT_KEYS[] = { "<Alt>F4", NULL };
    GMenu *bar = g_menu_new();
    GMenu *game_menu = g_menu_new();
    GMenu *help_menu = g_menu_new();
    GMenu *section;
    size_t i;

    for (i = 0; i < sizeof(ACTIONS) / sizeof(ACTIONS[0]); i++) {
        GSimpleAction *action = g_simple_action_new(ACTIONS[i], NULL);

        g_signal_connect(action, "activate", G_CALLBACK(run_action), NULL);
        g_action_map_add_action(G_ACTION_MAP(application), G_ACTION(action));
        g_object_unref(action);
    }
    gtk_application_set_accels_for_action(application, "app.new-game", NEW_GAME_KEYS);
    gtk_application_set_accels_for_action(application, "app.pause", PAUSE_KEYS);
    gtk_application_set_accels_for_action(application, "app.quit", QUIT_KEYS);

    section = g_menu_new();
    add_menu_item(section, "_New...", "new-game");
    add_menu_item(section, "_Pause", "pause");
    g_menu_append_section(game_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    add_menu_item(section, "_Options...", "options");
    add_menu_item(section, "High _Scores", "high-scores");
    g_menu_append_section(game_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    add_menu_item(section, "E_xit", "quit");
    g_menu_append_section(game_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    add_menu_item(section, "_Instructions", "instructions");
    add_menu_item(section, "_Hints", "hints");
    add_menu_item(section, "Our _Story", "story");
    g_menu_append_section(help_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    add_menu_item(section, "_About Jewel Thief...", "about");
    g_menu_append_section(help_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    g_menu_append_submenu(bar, "_Game", G_MENU_MODEL(game_menu));
    g_menu_append_submenu(bar, "_Help", G_MENU_MODEL(help_menu));
    gtk_application_set_menubar(application, G_MENU_MODEL(bar));
    g_object_unref(game_menu);
    g_object_unref(help_menu);
    g_object_unref(bar);
}

static void on_main_window_destroy(GtkWidget *window, gpointer unused)
{
    (void)window;
    (void)unused;
    if (ticker_running) {
        ticker_stop();
        ticker_running = false;
    }
    game_area = NULL;
}

static void create_main_window(void)
{
    GtkWidget *window = gtk_application_window_new(application);
    GtkEventController *motion = gtk_event_controller_motion_new();
    GtkEventController *keys = gtk_event_controller_key_new();

    main_window = GTK_WINDOW(window);
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window), TRUE);
    gtk_window_set_resizable(main_window, FALSE);

    game_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(game_area), CLIENT_WIDTH);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(game_area), CLIENT_HEIGHT);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(game_area), draw_game, NULL, NULL);
    gtk_widget_set_cursor(game_area, cursor_right);
    /* Windows sends a mouse move when the cursor enters the window, too. */
    g_signal_connect(motion, "enter", G_CALLBACK(on_motion), NULL);
    g_signal_connect(motion, "motion", G_CALLBACK(on_motion), NULL);
    gtk_widget_add_controller(game_area, motion);
    gtk_window_set_child(main_window, game_area);

    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), NULL);
    gtk_widget_add_controller(window, keys);
    g_signal_connect(window, "notify::is-active", G_CALLBACK(on_active_changed), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(on_main_window_destroy), NULL);

    dialogs_init(main_window, &assets);
    update_title();
    gtk_window_present(main_window);
}

/*
 * Startup as in the original, minus the shareware About screen: the game is
 * dealt before the window shows, and the "Get ready" box may follow.
 */
static void on_activate(GApplication *app, gpointer unused)
{
    (void)app;
    (void)unused;
    dialogs_init(NULL, &assets);
    if (!open_assets()) {
        return;
    }
    if (!profile_init(assets.dir)) {
        show_error("Could not initialize the game.");
        return;
    }
    if (!create_frame()) {
        show_error("Could not create the drawing surface.");
        return;
    }
    if (!init_renderer() || (cursor_right = load_cursor("RIGHTCUR")) == NULL
        || (cursor_left = load_cursor("LEFTCUR")) == NULL) {
        return;
    }
    cursor_wait = gdk_cursor_new_from_name("wait", NULL);

    init_game();
    render();
    create_menu();
    create_main_window();
    if (!ticker_start(handle_tick, TICK_SECONDS)) {
        gtk_window_destroy(main_window);
        show_error("Could not start the game clock.");
        return;
    }
    ticker_running = true;
    wait_if_enabled();
}

static void on_shutdown(GApplication *app, gpointer unused)
{
    (void)app;
    (void)unused;
    if (ticker_running) {
        ticker_stop();
    }
    g_clear_object(&cursor_wait);
    g_clear_object(&cursor_left);
    g_clear_object(&cursor_right);
    if (renderer_ready) {
        render_destroy(&renderer);
    }
    destroy_frame();
    if (assets_loaded) {
        assets_close(&assets);
    }
}

int main(int argc, char **argv)
{
    int exit_code;

    parse_command_line(argc, argv);
    application = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(application, "shutdown", G_CALLBACK(on_shutdown), NULL);
    /* The arguments are the game's own, not GTK's. */
    exit_code = g_application_run(G_APPLICATION(application), 1, argv);
    g_object_unref(application);
    return exit_code;
}
