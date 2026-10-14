#include "dialogs.h"

#include "win16res.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Control ids in the original's templates. */
enum {
    ID_OK = 1,
    ID_CANCEL = 2,
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

#define WAIT_MILLISECONDS 3000
#define WAIT_TIMER 1
#define NAME_LENGTH 25
#define HIGH_SCORE_LINE 96

static HINSTANCE app_instance;
static const Assets *app_assets;

typedef struct {
    long score;
    char *name;
    size_t name_size;
} NewHighScore;

static INT_PTR run_template(HWND owner, const NeBlob *blob, DLGPROC procedure, LPARAM param)
{
    void *dialog = win16_dialog_to_win32(blob->data, blob->size);
    INT_PTR result;

    if (dialog == NULL) {
        return -1;
    }
    result = DialogBoxIndirectParamW(app_instance, dialog, owner, procedure, param);
    free(dialog);
    return result;
}

static INT_PTR run_named(HWND owner, const char *name, DLGPROC procedure, LPARAM param)
{
    NeBlob blob;

    if (!assets_resource_named(app_assets, NE_RT_DIALOG, name, &blob)) {
        return -1;
    }
    return run_template(owner, &blob, procedure, param);
}

static INT_PTR run_id(HWND owner, uint16_t id, DLGPROC procedure, LPARAM param)
{
    NeBlob blob;

    if (!assets_resource_id(app_assets, NE_RT_DIALOG, id, &blob)) {
        return -1;
    }
    return run_template(owner, &blob, procedure, param);
}

/* For boxes that only need to close: OK, Cancel, Play, Continue or Escape. */
static INT_PTR CALLBACK close_procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    if (message == WM_INITDIALOG) {
        return TRUE;
    }
    if (message == WM_COMMAND && (LOWORD(wparam) == ID_OK || LOWORD(wparam) == ID_CANCEL)) {
        EndDialog(dialog, LOWORD(wparam));
        return TRUE;
    }
    return FALSE;
}

static INT_PTR CALLBACK wait_procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)wparam;
    (void)lparam;
    switch (message) {
    case WM_INITDIALOG:
        SetTimer(dialog, WAIT_TIMER, WAIT_MILLISECONDS, NULL);
        return TRUE;
    case WM_TIMER:
        KillTimer(dialog, WAIT_TIMER);
        EndDialog(dialog, ID_OK);
        return TRUE;
    }
    return FALSE;
}

static int speed_button(GameSpeed speed)
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

static GameSpeed checked_speed(HWND dialog)
{
    if (IsDlgButtonChecked(dialog, ID_BEGINNER) == BST_CHECKED) {
        return GAME_SPEED_BEGINNER;
    }
    if (IsDlgButtonChecked(dialog, ID_EXPERT) == BST_CHECKED) {
        return GAME_SPEED_EXPERT;
    }
    return GAME_SPEED_INTERMEDIATE;
}

static INT_PTR CALLBACK options_procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    GameSettings *settings = (GameSettings *)GetWindowLongPtrW(dialog, DWLP_USER);

    switch (message) {
    case WM_INITDIALOG:
        settings = (GameSettings *)lparam;
        SetWindowLongPtrW(dialog, DWLP_USER, lparam);
        CheckRadioButton(dialog, ID_BEGINNER, ID_EXPERT, speed_button(settings->speed));
        CheckDlgButton(dialog, ID_LEVEL_WAIT, settings->level_wait ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, ID_CHEAT, settings->cheat ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_OK:
            settings->speed = checked_speed(dialog);
            settings->level_wait = IsDlgButtonChecked(dialog, ID_LEVEL_WAIT) == BST_CHECKED;
            settings->cheat = IsDlgButtonChecked(dialog, ID_CHEAT) == BST_CHECKED;
            EndDialog(dialog, ID_OK);
            return TRUE;
        case ID_CANCEL:
            EndDialog(dialog, ID_CANCEL);
            return TRUE;
        case ID_HELP:
            /* The original opened its WinHelp file, which modern Windows cannot show. */
            dialogs_instructions(dialog);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR CALLBACK high_scores_procedure(HWND dialog, UINT message, WPARAM wparam,
                                              LPARAM lparam)
{
    const HighScores *table;
    char line[HIGH_SCORE_LINE];
    int i;

    if (message == WM_INITDIALOG) {
        table = (const HighScores *)lparam;
        for (i = 0; i < HIGH_SCORES; i++) {
            /* Line 10 has its own format, with less padding after "10.". */
            const char *format = assets_text(app_assets, i < HIGH_SCORES - 1
                                                             ? TEXT_HIGH_SCORE_FORMAT
                                                             : TEXT_HIGH_SCORE_10_FORMAT);
            snprintf(line, sizeof(line), format, i + 1, table->entries[i].score,
                     table->entries[i].name);
            SetDlgItemTextA(dialog, ID_FIRST_HIGH_SCORE + i, line);
        }
        return TRUE;
    }
    return close_procedure(dialog, message, wparam, lparam);
}

static INT_PTR CALLBACK new_high_score_procedure(HWND dialog, UINT message, WPARAM wparam,
                                                 LPARAM lparam)
{
    NewHighScore *entry = (NewHighScore *)GetWindowLongPtrW(dialog, DWLP_USER);
    char score[32];

    switch (message) {
    case WM_INITDIALOG:
        entry = (NewHighScore *)lparam;
        SetWindowLongPtrW(dialog, DWLP_USER, lparam);
        snprintf(score, sizeof(score), "%ld", entry->score);
        SetDlgItemTextA(dialog, ID_SCORE, score);
        SendDlgItemMessageA(dialog, ID_NAME, EM_LIMITTEXT, NAME_LENGTH, 0);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_OK) {
            GetDlgItemTextA(dialog, ID_NAME, entry->name, (int)entry->name_size);
            EndDialog(dialog, ID_OK);
            return TRUE;
        }
        if (LOWORD(wparam) == ID_CANCEL) {
            EndDialog(dialog, ID_CANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR CALLBACK about_procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_INITDIALOG) {
        /* Shareware registration does not apply to this port. */
        EnableWindow(GetDlgItem(dialog, ID_REGISTER), FALSE);
        return TRUE;
    }
    return close_procedure(dialog, message, wparam, lparam);
}

/*
 * The About dialog's SPLASH controls paint a .bmp file from the game
 * directory, chosen by the control's style: 0 is winlogo.bmp, 1 jwlthief.bmp.
 */
static void paint_splash(HWND window, HDC dc)
{
    static const char *const FILES[] = { "winlogo.bmp", "jwlthief.bmp" };
    enum { FILE_HEADER_SIZE = 14 };
    size_t index = (size_t)(GetWindowLongW(window, GWL_STYLE) & 0xFFFF);
    char path[FS_MAX_PATH];
    uint8_t *data;
    size_t size;
    Dib dib;

    if (index >= sizeof(FILES) / sizeof(FILES[0])
        || !fs_join_path(path, sizeof(path), app_assets->dir, FILES[index])
        || !fs_read_file(path, &data, &size)) {
        return;
    }
    if (size > FILE_HEADER_SIZE && dib_parse(data + FILE_HEADER_SIZE, size - FILE_HEADER_SIZE, &dib)) {
        SetDIBitsToDevice(dc, 0, 0, (DWORD)dib.width, (DWORD)dib.height, 0, 0, 0,
                          (UINT)dib.height, dib.pixels, (const BITMAPINFO *)dib.info,
                          DIB_RGB_COLORS);
    }
    free(data);
}

static LRESULT CALLBACK splash_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    PAINTSTRUCT paint;

    if (message == WM_PAINT) {
        BeginPaint(window, &paint);
        paint_splash(window, paint.hdc);
        EndPaint(window, &paint);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

bool dialogs_init(HINSTANCE instance, const Assets *assets)
{
    WNDCLASSA splash;

    app_instance = instance;
    app_assets = assets;
    memset(&splash, 0, sizeof(splash));
    splash.lpfnWndProc = splash_procedure;
    splash.hInstance = instance;
    splash.hCursor = LoadCursor(NULL, IDC_ARROW);
    splash.lpszClassName = "SPLASH";
    return RegisterClassA(&splash) != 0;
}

void dialogs_pause(HWND owner)
{
    run_named(owner, "PAUSEBOX", close_procedure, 0);
}

void dialogs_wait(HWND owner)
{
    run_named(owner, "WAITBOX", wait_procedure, 0);
}

bool dialogs_options(HWND owner, GameSettings *settings)
{
    return run_id(owner, DIALOG_OPTIONS, options_procedure, (LPARAM)settings) == ID_OK;
}

void dialogs_high_scores(HWND owner, const HighScores *table)
{
    run_id(owner, DIALOG_HIGH_SCORES, high_scores_procedure, (LPARAM)table);
}

void dialogs_new_high_score(HWND owner, long score, char *name, size_t name_size)
{
    NewHighScore entry = { score, name, name_size };

    name[0] = '\0';
    run_id(owner, DIALOG_NEW_HIGH_SCORE, new_high_score_procedure, (LPARAM)&entry);
}

void dialogs_about(HWND owner)
{
    run_id(owner, DIALOG_ABOUT, about_procedure, 0);
}

void dialogs_instructions(HWND owner)
{
    run_named(owner, "INSTRBOX", close_procedure, 0);
}

void dialogs_hints(HWND owner)
{
    run_named(owner, "HINTSBOX", close_procedure, 0);
}

void dialogs_story(HWND owner)
{
    run_named(owner, "STORYBOX", close_procedure, 0);
}
