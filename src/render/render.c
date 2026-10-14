#include "render.h"

#include "scenery.h"

#include <stdio.h>
#include <string.h>

/* The window class background, left visible only outside the scenery. */
#define PLAYFIELD_COLOR FB_RGB(0xFF, 0xFF, 0xFF)
#define STATUS_BAR_COLOR FB_RGB(0xC0, 0xC0, 0xC0)
#define SHADOW_COLOR FB_RGB(0x00, 0x00, 0x00)
#define HIGHLIGHT_COLOR FB_RGB(0xFF, 0xFF, 0xFF)

/* Status bar layout, in pixels from its top-left corner, as in the original. */
#define BOX_INSET 5
#define TEXT_TOP 8
#define LEVEL_NAME_LEFT 10
#define LEVEL_BOX_RIGHT 370
#define LIVES_BOX_LEFT 375
#define LIVES_BOX_RIGHT 425
#define LIVES_TEXT_LEFT 380
#define SCORE_BOX_LEFT 430
#define SCORE_TEXT_LEFT 435
#define ICONS_LEFT 90
#define ICONS_TOP 7
#define ICON_WIDTH 20

static const char *load_bitmaps(Renderer *renderer, const Assets *assets)
{
    int i;

    for (i = 0; i < GAME_LEVELS; i++) {
        if (!assets_bitmap(assets, game_guard_bitmap(i), &renderer->guards[i])) {
            return game_guard_bitmap(i);
        }
        if (!assets_bitmap(assets, game_level_name(i), &renderer->jewels[i])) {
            return game_level_name(i);
        }
    }
    for (i = 0; i < GAME_SCENES; i++) {
        if (!assets_bitmap(assets, scenery_bitmap(i), &renderer->scenery[i])) {
            return scenery_bitmap(i);
        }
    }
    if (!assets_bitmap(assets, "STATUSBR", &renderer->status_icons)) {
        return "STATUSBR";
    }
    return NULL;
}

RenderStatus render_init(Renderer *renderer, const Assets *assets, int width, int height,
                         const char **missing)
{
    memset(renderer, 0, sizeof(*renderer));
    *missing = load_bitmaps(renderer, assets);
    if (*missing != NULL) {
        return RENDER_ERR_MISSING_BITMAP;
    }
    if (!fb_create(&renderer->background, width, height)) {
        return RENDER_ERR_OUT_OF_MEMORY;
    }
    return RENDER_OK;
}

void render_destroy(Renderer *renderer)
{
    fb_destroy(&renderer->background);
}

void render_sprite_sizes(const Renderer *renderer, GameSprites *out)
{
    int level;

    for (level = 0; level < GAME_LEVELS; level++) {
        out->guard[level].width = renderer->guards[level].width;
        out->guard[level].height = renderer->guards[level].height;
        out->jewel[level].width = renderer->jewels[level].width;
        out->jewel[level].height = renderer->jewels[level].height;
    }
}

static void update_background(Renderer *renderer, const Game *game)
{
    Framebuffer *background = &renderer->background;

    if (renderer->background_ready && renderer->background_serial == game->scenery_serial) {
        return;
    }
    fb_fill_rect(background, 0, 0, background->width, game->playfield.bottom, PLAYFIELD_COLOR);
    scenery_draw(background, game, &renderer->scenery[game_scene(game)]);
    renderer->background_serial = game->scenery_serial;
    renderer->background_ready = true;
}

/* A sunken box: shadow on the left and top edges, highlight on the others. */
static void draw_sunken_box(Framebuffer *fb, int left, int top, int right, int bottom)
{
    fb_draw_line(fb, left, bottom - 1, left, top, SHADOW_COLOR);
    fb_draw_line(fb, left, top, right, top, SHADOW_COLOR);
    fb_draw_line(fb, right, top, right, bottom, HIGHLIGHT_COLOR);
    fb_draw_line(fb, right, bottom, left, bottom, HIGHLIGHT_COLOR);
}

static void draw_status_bar(Framebuffer *fb, const Game *game, const Renderer *renderer)
{
    int left = game->playfield.left;
    int top = game->playfield.bottom;
    int right = game->playfield.right;
    int bottom = fb->height;
    int box_top = top + BOX_INSET;
    int box_bottom = bottom - BOX_INSET + 1;

    fb_fill_rect(fb, left, top, right - left, bottom - top, STATUS_BAR_COLOR);
    fb_draw_line(fb, left, top, right, top, SHADOW_COLOR);
    fb_draw_line(fb, left, top + 1, right, top + 1, HIGHLIGHT_COLOR);
    draw_sunken_box(fb, left + BOX_INSET, box_top, left + LEVEL_BOX_RIGHT, box_bottom);
    draw_sunken_box(fb, left + LIVES_BOX_LEFT, box_top, left + LIVES_BOX_RIGHT, box_bottom);
    draw_sunken_box(fb, left + SCORE_BOX_LEFT, box_top, right - BOX_INSET, box_bottom);

    /* One icon per level reached: a growing prefix of the STATUSBR strip. */
    fb_draw_bitmap_region(fb, &renderer->status_icons, 0, 0,
                          ICON_WIDTH * (game_display_level(game) + 1),
                          renderer->status_icons.height, left + ICONS_LEFT, top + ICONS_TOP);
}

void render_frame(Renderer *renderer, Framebuffer *fb, const Game *game)
{
    int level = game_display_level(game);
    int i;

    update_background(renderer, game);
    memcpy(fb->pixels, renderer->background.pixels,
           (size_t)fb->width * (size_t)game->playfield.bottom * sizeof(*fb->pixels));
    for (i = 0; i < game->jewel_count; i++) {
        if (game->jewels[i].present) {
            fb_draw_sprite(fb, &renderer->jewels[level], game->jewels[i].x, game->jewels[i].y);
        }
    }
    for (i = 0; i < game->enemy_count; i++) {
        fb_draw_sprite(fb, &renderer->guards[level], game->enemies[i].x, game->enemies[i].y);
    }
    draw_status_bar(fb, game, renderer);
}

static void place_text(RenderText *text, int x, int y)
{
    text->x = x;
    text->y = y;
}

void render_status_text(const Game *game, RenderStatusText *out)
{
    int left = game->playfield.left;
    int top = game->playfield.bottom + TEXT_TOP;

    place_text(&out->level_name, left + LEVEL_NAME_LEFT, top);
    snprintf(out->level_name.text, sizeof(out->level_name.text), "%s",
             game_level_name(game_display_level(game)));

    place_text(&out->lives, left + LIVES_TEXT_LEFT, top);
    snprintf(out->lives.text, sizeof(out->lives.text), game->lives == 1 ? "%i man" : "%i men",
             game->lives);

    /* The score is kept divided by 100; zero is shown without the "00". */
    place_text(&out->score, left + SCORE_TEXT_LEFT, top);
    snprintf(out->score.text, sizeof(out->score.text), game->score < 1 ? "%i" : "%i00",
             game->score);
}
