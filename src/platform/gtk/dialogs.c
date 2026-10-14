#include "dialogs.h"

#include <stdio.h>
#include <string.h>

#define ORIGINAL_CODE_SET "WINDOWS-1252"
#define WAIT_MILLISECONDS 3000
#define NAME_LENGTH 25
#define HIGH_SCORE_LINE 96
#define HELP_TEXT_SIZE 1024
#define TEXT_WIDTH_CHARS 48
#define SPACING 12

typedef void (*Finish)(GtkWindow *window, bool accepted);

/* What a dialog window carries until it closes. */
typedef struct {
    Finish finish;
    bool closable;
    bool closed;
    DialogsDone done;
    DialogsAnswered answered;
    DialogsOptionsDone options_done;
    DialogsNameDone name_done;
} DialogState;

static GtkWindow *owner;
static const Assets *app_assets;
static int open_count;

/* The original's texts are Windows-1252 with CR LF line breaks. */
static char *to_utf8(const char *text)
{
    char *converted = g_convert(text, -1, "UTF-8", ORIGINAL_CODE_SET, NULL, NULL, NULL);
    char *read, *write;

    if (converted == NULL) {
        converted = g_utf8_make_valid(text, -1);
    }
    for (read = write = converted; *read != '\0'; read++) {
        if (*read != '\r') {
            *write++ = *read;
        }
    }
    *write = '\0';
    return converted;
}

static void to_original(const char *text, char *out, size_t size)
{
    char *converted = g_convert_with_fallback(text, -1, ORIGINAL_CODE_SET, "UTF-8", "?", NULL,
                                              NULL, NULL);

    g_strlcpy(out, converted != NULL ? converted : "", size);
    g_free(converted);
}

static DialogState *state_of(GtkWindow *window)
{
    return g_object_get_data(G_OBJECT(window), "dialog-state");
}

static void close_dialog(GtkWindow *window, bool accepted)
{
    DialogState *state = state_of(window);

    if (state->closed) {
        return;
    }
    state->closed = true;
    open_count--;
    if (state->finish != NULL) {
        state->finish(window, accepted);
    }
    gtk_window_destroy(window);
}

static gboolean on_close_request(GtkWindow *window, gpointer unused)
{
    (void)unused;
    if (state_of(window)->closable) {
        close_dialog(window, false);
    }
    return TRUE;
}

/* Destroyed along with the main window: no callback, the game is ending. */
static void on_destroy(GtkWidget *window, gpointer unused)
{
    DialogState *state = state_of(GTK_WINDOW(window));

    (void)unused;
    if (!state->closed) {
        state->closed = true;
        open_count--;
    }
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint key, guint code,
                               GdkModifierType modifiers, gpointer window)
{
    (void)controller;
    (void)code;
    (void)modifiers;
    if (key == GDK_KEY_Escape) {
        close_dialog(window, false);
        return TRUE;
    }
    return FALSE;
}

static GtkWindow *create_dialog(const char *title, Finish finish, bool closable)
{
    GtkWindow *window = GTK_WINDOW(gtk_window_new());
    DialogState *state = g_new0(DialogState, 1);

    state->finish = finish;
    state->closable = closable;
    g_object_set_data_full(G_OBJECT(window), "dialog-state", state, g_free);

    gtk_window_set_title(window, title);
    gtk_window_set_modal(window, TRUE);
    gtk_window_set_resizable(window, FALSE);
    gtk_window_set_deletable(window, closable);
    if (owner != NULL) {
        gtk_window_set_transient_for(window, owner);
        gtk_window_set_destroy_with_parent(window, TRUE);
    }
    g_signal_connect(window, "close-request", G_CALLBACK(on_close_request), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);
    if (closable) {
        GtkEventController *keys = gtk_event_controller_key_new();

        g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), window);
        gtk_widget_add_controller(GTK_WIDGET(window), keys);
    }
    open_count++;
    return window;
}

static GtkWidget *create_box(GtkOrientation orientation)
{
    return gtk_box_new(orientation, SPACING);
}

static GtkWidget *create_buttons(void)
{
    GtkWidget *buttons = create_box(GTK_ORIENTATION_HORIZONTAL);

    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    return buttons;
}

static void on_button_clicked(GtkButton *button, gpointer window)
{
    close_dialog(window, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "accepted")));
}

/* The first button added with `is_default` answers Enter. */
static GtkWidget *add_button(GtkWidget *buttons, GtkWindow *window, const char *label,
                             bool accepted, bool is_default)
{
    GtkWidget *button = gtk_button_new_with_mnemonic(label);

    g_object_set_data(G_OBJECT(button), "accepted", GINT_TO_POINTER(accepted));
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), window);
    gtk_box_append(GTK_BOX(buttons), button);
    if (is_default) {
        gtk_window_set_default_widget(window, button);
        gtk_widget_grab_focus(button);
    }
    return button;
}

static GtkWidget *create_label(const char *utf8_text)
{
    GtkWidget *label = gtk_label_new(utf8_text);

    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), TEXT_WIDTH_CHARS);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    return label;
}

static GtkWidget *create_original_label(const char *text)
{
    char *utf8 = to_utf8(text);
    GtkWidget *label = create_label(utf8);

    g_free(utf8);
    return label;
}

static void show_dialog(GtkWindow *window, GtkWidget *body, GtkWidget *buttons)
{
    GtkWidget *content = create_box(GTK_ORIENTATION_VERTICAL);

    gtk_widget_set_margin_top(content, SPACING);
    gtk_widget_set_margin_bottom(content, SPACING);
    gtk_widget_set_margin_start(content, SPACING);
    gtk_widget_set_margin_end(content, SPACING);
    gtk_box_append(GTK_BOX(content), body);
    if (buttons != NULL) {
        gtk_box_append(GTK_BOX(content), buttons);
    }
    gtk_window_set_child(window, content);
    gtk_window_present(window);
}

static void finish_done(GtkWindow *window, bool accepted)
{
    DialogState *state = state_of(window);

    (void)accepted;
    if (state->done != NULL) {
        state->done();
    }
}

static void finish_question(GtkWindow *window, bool accepted)
{
    state_of(window)->answered(accepted);
}

/* Errors may come before the original is loaded and can give its title. */
static char *title(void)
{
    const char *original = assets_text(app_assets, TEXT_TITLE);

    return to_utf8(original[0] != '\0' ? original : "Jewel Thief");
}

void dialogs_init(GtkWindow *main_window, const Assets *assets)
{
    owner = main_window;
    app_assets = assets;
}

bool dialogs_open(void)
{
    return open_count > 0;
}

void dialogs_message(const char *text, DialogsDone done)
{
    char *caption = title();
    GtkWindow *window = create_dialog(caption, finish_done, true);
    GtkWidget *buttons = create_buttons();

    g_free(caption);
    state_of(window)->done = done;
    add_button(buttons, window, "_OK", true, true);
    show_dialog(window, create_original_label(text), buttons);
}

void dialogs_question(const char *text, DialogsAnswered answered)
{
    char *caption = title();
    GtkWindow *window = create_dialog(caption, finish_question, true);
    GtkWidget *buttons = create_buttons();

    g_free(caption);
    state_of(window)->answered = answered;
    add_button(buttons, window, "_No", false, false);
    add_button(buttons, window, "_Yes", true, true);
    show_dialog(window, create_original_label(text), buttons);
}

void dialogs_pause(void)
{
    GtkWindow *window = create_dialog("Paused", NULL, true);
    GtkWidget *buttons = create_buttons();

    add_button(buttons, window, "_Play", true, true);
    show_dialog(window, create_label("Paused..."), buttons);
}

static gboolean on_wait_elapsed(gpointer window)
{
    close_dialog(window, true);
    return G_SOURCE_REMOVE;
}

void dialogs_wait(DialogsDone done)
{
    GtkWindow *window = create_dialog("Get ready", finish_done, false);

    state_of(window)->done = done;
    /* The reference keeps the state readable if the window goes first. */
    g_timeout_add_full(G_PRIORITY_DEFAULT, WAIT_MILLISECONDS, on_wait_elapsed,
                       g_object_ref(window), g_object_unref);
    show_dialog(window, create_label("Get ready..."), NULL);
}

static void finish_options(GtkWindow *window, bool accepted)
{
    GObject *object = G_OBJECT(window);
    GameSettings settings;

    if (!accepted) {
        state_of(window)->options_done(NULL);
        return;
    }
    settings.speed = GAME_SPEED_INTERMEDIATE;
    if (gtk_check_button_get_active(g_object_get_data(object, "beginner"))) {
        settings.speed = GAME_SPEED_BEGINNER;
    }
    else if (gtk_check_button_get_active(g_object_get_data(object, "expert"))) {
        settings.speed = GAME_SPEED_EXPERT;
    }
    settings.level_wait = gtk_check_button_get_active(g_object_get_data(object, "level-wait"));
    settings.cheat = gtk_check_button_get_active(g_object_get_data(object, "cheat"));
    state_of(window)->options_done(&settings);
}

static GtkWidget *add_check(GtkWidget *box, GtkWindow *window, const char *key, const char *label,
                            bool active, GtkWidget *group)
{
    GtkWidget *check = gtk_check_button_new_with_mnemonic(label);

    gtk_check_button_set_active(GTK_CHECK_BUTTON(check), active);
    if (group != NULL) {
        gtk_check_button_set_group(GTK_CHECK_BUTTON(check), GTK_CHECK_BUTTON(group));
    }
    g_object_set_data(G_OBJECT(window), key, check);
    gtk_box_append(GTK_BOX(box), check);
    return check;
}

/* The original's Help button opened WinHelp; the instructions box stands in. */
static void on_options_help(GtkButton *button, gpointer unused)
{
    (void)button;
    (void)unused;
    dialogs_instructions();
}

void dialogs_options(const GameSettings *settings, DialogsOptionsDone done)
{
    GtkWindow *window = create_dialog("Options", finish_options, true);
    GtkWidget *body = create_box(GTK_ORIENTATION_VERTICAL);
    GtkWidget *speeds = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *frame = gtk_frame_new("Speed");
    GtkWidget *buttons = create_buttons();
    GtkWidget *help, *first;

    state_of(window)->options_done = done;
    first = add_check(speeds, window, "beginner", "_Beginner",
                      settings->speed == GAME_SPEED_BEGINNER, NULL);
    add_check(speeds, window, "intermediate", "_Intermediate",
              settings->speed == GAME_SPEED_INTERMEDIATE, first);
    add_check(speeds, window, "expert", "_Expert", settings->speed == GAME_SPEED_EXPERT, first);
    gtk_frame_set_child(GTK_FRAME(frame), speeds);
    gtk_box_append(GTK_BOX(body), frame);
    add_check(body, window, "level-wait", "_Pause after each level", settings->level_wait, NULL);
    add_check(body, window, "cheat", "_Cheat mode: the thief can leave the playing field",
              settings->cheat, NULL);

    help = gtk_button_new_with_mnemonic("_Help");
    g_signal_connect(help, "clicked", G_CALLBACK(on_options_help), NULL);
    gtk_box_append(GTK_BOX(buttons), help);
    add_button(buttons, window, "_Cancel", false, false);
    add_button(buttons, window, "_OK", true, true);
    show_dialog(window, body, buttons);
}

void dialogs_high_scores(const HighScores *table, DialogsDone done)
{
    GtkWindow *window = create_dialog("High Scores", finish_done, true);
    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, SPACING / 2);
    GtkWidget *buttons = create_buttons();
    char line[HIGH_SCORE_LINE];
    int i;

    state_of(window)->done = done;
    for (i = 0; i < HIGH_SCORES; i++) {
        /* Line 10 has its own format, with less padding after "10.". */
        const char *format = assets_text(app_assets, i < HIGH_SCORES - 1
                                                         ? TEXT_HIGH_SCORE_FORMAT
                                                         : TEXT_HIGH_SCORE_10_FORMAT);

        snprintf(line, sizeof(line), format, i + 1, table->entries[i].score,
                 table->entries[i].name);
        gtk_box_append(GTK_BOX(body), create_original_label(line));
    }
    add_button(buttons, window, "_OK", true, true);
    show_dialog(window, body, buttons);
}

static void finish_new_high_score(GtkWindow *window, bool accepted)
{
    GtkEditable *entry = g_object_get_data(G_OBJECT(window), "name");
    char name[NAME_LENGTH * 4 + 1];

    name[0] = '\0';
    if (accepted) {
        to_original(gtk_editable_get_text(entry), name, sizeof(name));
    }
    state_of(window)->name_done(name);
}

void dialogs_new_high_score(long score, DialogsNameDone done)
{
    GtkWindow *window = create_dialog("New High Score", finish_new_high_score, true);
    GtkWidget *body = create_box(GTK_ORIENTATION_VERTICAL);
    GtkWidget *entry = gtk_entry_new();
    GtkWidget *buttons = create_buttons();
    char text[64];

    state_of(window)->name_done = done;
    snprintf(text, sizeof(text), "Your score of %ld is in the top ten.", score);
    gtk_box_append(GTK_BOX(body), create_label(text));
    gtk_box_append(GTK_BOX(body), create_label("Enter your name:"));
    gtk_entry_set_max_length(GTK_ENTRY(entry), NAME_LENGTH);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    g_object_set_data(G_OBJECT(window), "name", entry);
    gtk_box_append(GTK_BOX(body), entry);
    add_button(buttons, window, "_Cancel", false, false);
    add_button(buttons, window, "_OK", true, true);
    show_dialog(window, body, buttons);
    gtk_widget_grab_focus(entry);
}

void dialogs_about(void)
{
    GtkWindow *window = create_dialog("About Jewel Thief", NULL, true);
    GtkWidget *buttons = create_buttons();

    add_button(buttons, window, "_OK", true, true);
    show_dialog(window,
                create_label("Jewel Thief v1.4\n"
                             "Original game © 1991–1993 ServantWare.\n\n"
                             "Native port. The graphics and texts are read from your own "
                             "copy of JWLTHIEF.EXE."),
                buttons);
}

static void show_help(const char *caption, const char *template_name)
{
    char text[HELP_TEXT_SIZE];
    GtkWindow *window;
    GtkWidget *buttons;

    if (!assets_dialog_text(app_assets, template_name, text, sizeof(text))) {
        return;
    }
    window = create_dialog(caption, NULL, true);
    buttons = create_buttons();
    add_button(buttons, window, "_OK", true, true);
    show_dialog(window, create_original_label(text), buttons);
}

void dialogs_instructions(void)
{
    show_help("Instructions", "INSTRBOX");
}

void dialogs_hints(void)
{
    show_help("Hints", "HINTSBOX");
}

void dialogs_story(void)
{
    show_help("Our Story", "STORYBOX");
}
