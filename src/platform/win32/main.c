/*
 * Win32 entry point: window, menu, input, timing, text and the game flow
 * around the dialogs. The rules live in src/core and the drawing in
 * src/render; bitmaps, cursors, dialogs, menu and texts are loaded at
 * runtime from the user's original JWLTHIEF.EXE.
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
#include "profile.h"
#include "render.h"
#include "settings.h"
#include "ticker.h"
#include "win16res.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * The original asks for a 500x300 window, frame included, so its playfield
 * is whatever the Windows 3.1 frame left over. Modern frames are thicker,
 * so the client area is fixed instead, at its Windows 3.1 size: the floor
 * polygon ends at y 228, the bottom of a 260-pixel client area minus the
 * status bar, and a 1-pixel border on each side leaves 498 of the 500.
 */
#define CLIENT_WIDTH 498
#define CLIENT_HEIGHT 260
#define STATUS_BAR_HEIGHT 32

#define WINDOW_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
/* Where the original's CreateWindow puts the window. */
#define WINDOW_X 60
#define WINDOW_Y 30
#define WINDOW_CLASS "JewelThief"
#define GAME_TITLE "Jewel Thief"

/*
 * Windows 3.1 timers ran on the PC timer chip at 1193182 / 65536 Hz, so the
 * game's 5 ms timer really fired every ~54.9 ms; that is the game's pace.
 */
#define TICK_SECONDS (65536.0 / 1193182.0)
#define WM_GAME_TICK (WM_APP + 1)

/* Menu commands of the original's JWLTHIEFMENU. */
enum {
    CMD_NEW_GAME = 110,
    CMD_RESTART = 115, /* not in the menu: posted after "Play again?" */
    CMD_PAUSE = 120,
    CMD_OPTIONS = 130,
    CMD_HIGH_SCORES = 140,
    CMD_EXIT = 190,
    CMD_HELP_CONTENTS = 210,
    CMD_HELP_SEARCH = 220,
    CMD_HELP_ON_HELP = 230,
    CMD_ABOUT = 240
};

#define HIGH_SCORE_NAME_INPUT 26
#define ENDING_MESSAGE_SIZE 512

static Assets assets;
static Renderer renderer;
static Game game;
static HINSTANCE app_instance;
static HWND main_window;
static HMENU main_menu;
static HACCEL accelerators;
static HICON app_icon;
static HCURSOR cursor_right;
static HCURSOR cursor_left;
static HCURSOR cursor_wait;
static bool in_tick;
static bool debug_mode;
static const char *original_dir;

/* The frame lives in a DIB section, so GDI can draw the text into it too. */
static HDC frame_dc;
static HBITMAP frame_bitmap;
static HGDIOBJ frame_previous_bitmap;
static Framebuffer frame;

static void show_error(const char *message)
{
    MessageBoxA(NULL, message, GAME_TITLE, MB_OK | MB_ICONERROR);
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
static void parse_command_line(void)
{
    int i;

    for (i = 1; i < __argc; i++) {
        if (strcmp(__argv[i], "--debug") == 0) {
            debug_mode = true;
        }
        else if (original_dir == NULL) {
            original_dir = __argv[i];
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

/* Windows 3.x icons and cursors use the resource layout Win32 still accepts. */
static HICON create_icon(const NeBlob *blob, BOOL is_icon)
{
    if (blob->size > MAXDWORD) {
        return NULL;
    }
    /* The API takes a non-const pointer but only reads the resource. */
    return CreateIconFromResourceEx((PBYTE)blob->data, (DWORD)blob->size, is_icon, 0x00030000,
                                    0, 0, LR_DEFAULTCOLOR);
}

static HCURSOR load_cursor(const char *name)
{
    NeBlob blob;
    HCURSOR cursor;

    if (!assets_cursor(&assets, name, &blob)) {
        show_missing("Cursor", name);
        return NULL;
    }
    cursor = (HCURSOR)create_icon(&blob, FALSE);
    if (cursor == NULL) {
        show_missing("Usable cursor", name);
    }
    return cursor;
}

/* Without it the window just gets the default icon, so it is not fatal. */
static HICON load_icon(const char *name)
{
    NeBlob blob;

    return assets_icon(&assets, name, &blob) ? create_icon(&blob, TRUE) : NULL;
}

/*
 * The original's menu and accelerators. Its Help commands opened a WinHelp
 * file, which modern Windows cannot show, so they open the instructions,
 * hints and story boxes that v1.4 still carries.
 */
static bool load_menu(void)
{
    NeBlob blob;
    void *menu_template;

    if (!assets_resource_named(&assets, NE_RT_MENU, "JWLTHIEFMENU", &blob)
        || (menu_template = win16_menu_to_win32(blob.data, blob.size)) == NULL) {
        show_missing("Menu", "JWLTHIEFMENU");
        return false;
    }
    main_menu = LoadMenuIndirectW(menu_template);
    free(menu_template);
    if (main_menu == NULL) {
        show_missing("Usable menu", "JWLTHIEFMENU");
        return false;
    }
    ModifyMenuA(main_menu, CMD_HELP_CONTENTS, MF_BYCOMMAND | MF_STRING, CMD_HELP_CONTENTS,
                "&Instructions");
    ModifyMenuA(main_menu, CMD_HELP_SEARCH, MF_BYCOMMAND | MF_STRING, CMD_HELP_SEARCH, "&Hints");
    ModifyMenuA(main_menu, CMD_HELP_ON_HELP, MF_BYCOMMAND | MF_STRING, CMD_HELP_ON_HELP,
                "Our &Story");

    if (assets_resource_named(&assets, NE_RT_ACCELERATOR, "JWLTHIEFMENU", &blob)) {
        accelerators = win16_create_accelerators(blob.data, blob.size);
    }
    return true;
}

static bool create_frame(void)
{
    BITMAPINFO info;
    void *pixels = NULL;

    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = CLIENT_WIDTH;
    info.bmiHeader.biHeight = -CLIENT_HEIGHT; /* negative: rows are top-down */
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    frame_dc = CreateCompatibleDC(NULL);
    frame_bitmap = CreateDIBSection(frame_dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (frame_dc == NULL || frame_bitmap == NULL) {
        return false;
    }
    frame_previous_bitmap = SelectObject(frame_dc, frame_bitmap);
    SelectObject(frame_dc, GetStockObject(SYSTEM_FONT));
    SetBkMode(frame_dc, TRANSPARENT);
    SetTextColor(frame_dc, RGB(0, 0, 0));
    fb_wrap(&frame, pixels, CLIENT_WIDTH, CLIENT_HEIGHT);
    return true;
}

static void destroy_frame(void)
{
    fb_destroy(&frame);
    if (frame_dc != NULL) {
        SelectObject(frame_dc, frame_previous_bitmap);
        DeleteDC(frame_dc);
    }
    if (frame_bitmap != NULL) {
        DeleteObject(frame_bitmap);
    }
}

static void draw_text(const RenderText *status_text)
{
    TextOutA(frame_dc, status_text->x, status_text->y, status_text->text,
             (int)strlen(status_text->text));
}

static void render(void)
{
    RenderStatusText status;

    /* GDI may still be writing to the pixels; finish before touching them. */
    GdiFlush();
    render_frame(&renderer, &frame, &game);
    render_status_text(&game, &status);
    draw_text(&status.level_name);
    draw_text(&status.lives);
    draw_text(&status.score);
    GdiFlush();
}

static void present(HDC dc)
{
    BitBlt(dc, 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT, frame_dc, 0, 0, SRCCOPY);
}

static void redraw(void)
{
    HDC dc;

    if (!IsWindow(main_window)) {
        return;
    }
    render();
    dc = GetDC(main_window);
    present(dc);
    ReleaseDC(main_window, dc);
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
        dialogs_wait(main_window);
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
        dialogs_new_high_score(main_window, score, name, sizeof(name));
        high_scores_insert(&table, rank, name, score);
    }
    profile_save_high_scores(&table);
    dialogs_high_scores(main_window, &table);
}

static void show_ending(void)
{
    char message[ENDING_MESSAGE_SIZE];

    if (profile_load_settings().speed != GAME_SPEED_EXPERT) {
        MessageBoxA(main_window, text(TEXT_TRY_EXPERT), text(TEXT_TITLE), MB_OK);
        return;
    }
    /* The format interleaves the lines of the parable with CR LF pairs. */
    snprintf(message, sizeof(message), text(TEXT_ENDING_FORMAT), text(TEXT_PARABLE_1), '\r',
             '\n', text(TEXT_PARABLE_2), '\r', '\n', text(TEXT_PARABLE_3), '\r', '\n',
             text(TEXT_PARABLE_SOURCE), '\r', '\n', '\r', '\n', text(TEXT_GREAT_PRICE));
    MessageBoxA(main_window, message, text(TEXT_TITLE), MB_OK);
}

static void finish_game(unsigned events)
{
    redraw();
    if (events & GAME_EVENT_GAME_COMPLETED) {
        show_ending();
    }
    submit_high_score();
    if (MessageBoxA(main_window, text(TEXT_PLAY_AGAIN), text(TEXT_TITLE), MB_YESNO) == IDYES) {
        PostMessageA(main_window, WM_COMMAND, CMD_RESTART, 0);
    }
    else {
        PostMessageA(main_window, WM_COMMAND, CMD_EXIT, 0);
    }
}

static void handle_events(unsigned events)
{
    if (events & GAME_EVENT_LIFE_LOST) {
        MessageBeep(0);
    }
    if (events & (GAME_EVENT_GAME_OVER | GAME_EVENT_GAME_COMPLETED)) {
        finish_game(events);
        return;
    }
    /* As in the original: an hourglass while the next level is set up. */
    if (events & GAME_EVENT_LEVEL_UP) {
        SetCursor(cursor_wait);
    }
    if (events != 0) {
        redraw();
    }
    if (events & GAME_EVENT_LEVEL_UP) {
        /* A new scenery comes every second level; the original may pause there. */
        if (game.level % 2 == 0) {
            wait_if_enabled();
        }
        SetCursor(cursor_right);
    }
}

static void update_cursor(int x, int y)
{
    /* Outside the playfield the class cursor (facing right) stays, as in the original. */
    if (game_in_playfield(&game, x, y)) {
        SetCursor(game.facing_left ? cursor_left : cursor_right);
    }
}

static void handle_tick(void)
{
    ticker_acknowledge();
    /* Dialogs opened during a tick keep dispatching ticks; skip those. */
    if (in_tick) {
        return;
    }
    in_tick = true;
    handle_events(game_tick(&game));
    redraw();
    in_tick = false;
}

static void show_options(void)
{
    GameSettings settings = profile_load_settings();

    if (dialogs_options(main_window, &settings)) {
        profile_save_settings(&settings);
        new_game();
    }
}

static void show_high_scores(void)
{
    HighScores table;

    profile_load_high_scores(&table, &assets);
    dialogs_high_scores(main_window, &table);
}

static void handle_command(WORD command)
{
    switch (command) {
    case CMD_NEW_GAME:
        if (MessageBoxA(main_window, text(TEXT_TRADE_JEWELS), text(TEXT_TITLE), MB_YESNO) == IDYES) {
            new_game();
        }
        break;
    case CMD_RESTART:
        new_game();
        break;
    case CMD_PAUSE:
        dialogs_pause(main_window);
        break;
    case CMD_OPTIONS:
        show_options();
        break;
    case CMD_HIGH_SCORES:
        show_high_scores();
        break;
    case CMD_EXIT:
        DestroyWindow(main_window);
        break;
    case CMD_HELP_CONTENTS:
        dialogs_instructions(main_window);
        break;
    case CMD_HELP_SEARCH:
        dialogs_hints(main_window);
        break;
    case CMD_HELP_ON_HELP:
        dialogs_story(main_window);
        break;
    case CMD_ABOUT:
        dialogs_about(main_window);
        break;
    }
}

static void update_title(void)
{
    const char *title = GAME_TITLE;

    if (debug_mode) {
        title = game.god_mode ? GAME_TITLE " [DEBUG: god mode]" : GAME_TITLE " [DEBUG]";
    }
    SetWindowTextA(main_window, title);
}

static void handle_debug_key(WPARAM key)
{
    switch (key) {
    case VK_HOME:
        game.god_mode = !game.god_mode;
        update_title();
        break;
    case VK_PRIOR:
    case VK_NEXT:
        game_debug_set_level(&game, game.level + (key == VK_PRIOR ? 1 : -1));
        redraw();
        break;
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    PAINTSTRUCT paint;
    unsigned events;
    int x, y;

    switch (message) {
    case WM_PAINT:
        BeginPaint(window, &paint);
        present(paint.hdc);
        EndPaint(window, &paint);
        return 0;
    case WM_GAME_TICK:
        handle_tick();
        return 0;
    case WM_MOUSEMOVE:
        x = (short)LOWORD(lparam);
        y = (short)HIWORD(lparam);
        events = game_mouse_move(&game, x, y);
        /* The original turns the cursor before handling a jewel or a level-up. */
        update_cursor(x, y);
        handle_events(events);
        return 0;
    case WM_COMMAND:
        handle_command(LOWORD(wparam));
        return 0;
    case WM_SETFOCUS:
        game_set_focus(&game, true);
        return 0;
    case WM_KILLFOCUS:
        game_set_focus(&game, false);
        return 0;
    case WM_KEYDOWN:
        if (debug_mode) {
            handle_debug_key(wparam);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

static HWND create_main_window(int show)
{
    WNDCLASSA window_class;
    RECT frame_rect = { 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT };
    HWND window;

    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = app_instance;
    window_class.hIcon = app_icon;
    window_class.hCursor = cursor_right;
    window_class.lpszClassName = WINDOW_CLASS;
    if (!RegisterClassA(&window_class)) {
        return NULL;
    }

    AdjustWindowRect(&frame_rect, WINDOW_STYLE, TRUE);
    window = CreateWindowA(WINDOW_CLASS, GAME_TITLE, WINDOW_STYLE, WINDOW_X, WINDOW_Y,
                           frame_rect.right - frame_rect.left, frame_rect.bottom - frame_rect.top,
                           NULL, main_menu, app_instance, NULL);
    if (window != NULL) {
        ShowWindow(window, show);
        UpdateWindow(window);
    }
    return window;
}

static int run_message_loop(void)
{
    MSG message;

    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        if (accelerators != NULL && TranslateAcceleratorA(main_window, accelerators, &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return (int)message.wParam;
}

/*
 * Startup as in the original, minus the shareware About screen: the game is
 * dealt before the window shows, and the "Get ready" box may follow.
 */
static int run(int show)
{
    int exit_code;

    init_game();
    render();
    main_window = create_main_window(show);
    if (main_window == NULL) {
        show_error("Could not create the main window.");
        return EXIT_FAILURE;
    }
    update_title();
    if (!ticker_start(main_window, WM_GAME_TICK, TICK_SECONDS)) {
        show_error("Could not start the game clock.");
        DestroyWindow(main_window);
        return EXIT_FAILURE;
    }
    wait_if_enabled();
    exit_code = run_message_loop();
    ticker_stop();
    return exit_code;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    int exit_code = EXIT_FAILURE;

    (void)previous;
    (void)command_line;
    app_instance = instance;
    parse_command_line();

    if (!open_assets()) {
        return EXIT_FAILURE;
    }
    if (!profile_init(assets.dir) || !dialogs_init(instance, &assets)) {
        show_error("Could not initialize the game.");
    }
    else if (!create_frame()) {
        show_error("Could not create the drawing surface.");
    }
    else if (init_renderer() && load_menu() && (cursor_right = load_cursor("RIGHTCUR")) != NULL
             && (cursor_left = load_cursor("LEFTCUR")) != NULL) {
        app_icon = load_icon("FUNICON");
        cursor_wait = LoadCursor(NULL, IDC_WAIT);
        exit_code = run(show);
    }

    if (app_icon != NULL) {
        DestroyIcon(app_icon);
    }
    if (accelerators != NULL) {
        DestroyAcceleratorTable(accelerators);
    }
    if (cursor_left != NULL) {
        DestroyCursor(cursor_left);
    }
    if (cursor_right != NULL) {
        DestroyCursor(cursor_right);
    }
    render_destroy(&renderer);
    destroy_frame();
    assets_close(&assets);
    return exit_code;
}
