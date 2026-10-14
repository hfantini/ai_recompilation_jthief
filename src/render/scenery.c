#include "scenery.h"

#include "shapes.h"

#include <stddef.h>

/* Colors are kept as the original's COLORREF values (0x00BBGGRR). */
#define COLORREF_RGB(c) FB_RGB((c) & 0xFF, ((c) >> 8) & 0xFF, ((c) >> 16) & 0xFF)

#define BLACK COLORREF_RGB(0x000000)
#define SKY COLORREF_RGB(0xFFFF00)
#define SUN COLORREF_RGB(0x00FFFF)
#define SUN_PEN_WIDTH 7

#define SCENERY_HEIGHT 92
#define CLOUD_SKY_HEIGHT 100

#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

enum { SCENE_STAR, SCENE_SPHINX, SCENE_WATER, SCENE_CAVE, SCENE_CASTLE, SCENE_CITY, SCENE_CLOUD };

static const char *const BITMAPS[GAME_SCENES] = {
    "STAR", "SPHINX", "WATER", "CAVE", "CASTLE", "CITY", "CLOUD",
};

static const uint32_t FLOOR_COLORS[GAME_SCENES] = {
    COLORREF_RGB(0x000080), COLORREF_RGB(0x00FFFF), COLORREF_RGB(0x800000),
    COLORREF_RGB(0x008080), COLORREF_RGB(0x008000), COLORREF_RGB(0x808080),
    COLORREF_RGB(0xFFFF00),
};

/* The two regions the original builds on WM_CREATE, in window coordinates. */
static const ShapePoint HORIZON_BAND[] = {
    { 0, 92 }, { 500, 92 }, { 500, 150 }, { 400, 100 }, { 100, 100 }, { 0, 150 },
};

static const ShapePoint FLOOR[] = {
    { 0, 228 }, { 0, 150 }, { 100, 100 }, { 400, 100 }, { 500, 150 }, { 500, 228 },
};

const char *scenery_bitmap(int scene)
{
    return scene >= 0 && scene < GAME_SCENES ? BITMAPS[scene] : NULL;
}

static void fill_sky(Framebuffer *fb, const Game *game, int height, uint32_t color)
{
    const GameRect *field = &game->playfield;
    fb_fill_rect(fb, field->left, field->top, field->right - field->left, height, color);
}

static void fill_band(Framebuffer *fb, uint32_t color)
{
    shapes_fill_polygon(fb, HORIZON_BAND, COUNT_OF(HORIZON_BAND), color);
}

static void draw_sun(Framebuffer *fb, ShapeBox box)
{
    shapes_ellipse(fb, box, SUN, SUN, SUN_PEN_WIDTH);
}

static void draw_polyline(Framebuffer *fb, const ShapePoint *points, int count, uint32_t pen,
                          int width)
{
    int i;

    for (i = 0; i + 1 < count; i++) {
        shapes_line(fb, points[i], points[i + 1], pen, width);
    }
}

static void draw_star(Framebuffer *fb, const Game *game, const Dib *star)
{
    static const ShapeBox PLANET = { -100, 90, 600, 200 };
    int i;

    fill_sky(fb, game, SCENERY_HEIGHT, BLACK);
    fill_band(fb, BLACK);
    shapes_ellipse(fb, PLANET, COLORREF_RGB(0x000080), BLACK, 1);
    for (i = 0; i < GAME_STARS; i++) {
        fb_draw_bitmap(fb, star, game->stars[i].x, game->stars[i].y);
    }
}

/* Each pyramid has two faces, seen from a corner: left face, right face. */
static void draw_sphinx(Framebuffer *fb, const Game *game, const Dib *sphinx)
{
    static const ShapePoint BIG_LEFT[] = { { 44, 50 }, { 5, 94 }, { 44, 125 } };
    static const ShapePoint BIG_RIGHT[] = { { 44, 50 }, { 80, 94 }, { 44, 125 } };
    static const ShapePoint SMALL_LEFT[] = { { 454, 80 }, { 438, 99 }, { 454, 115 } };
    static const ShapePoint SMALL_RIGHT[] = { { 454, 80 }, { 470, 99 }, { 454, 115 } };
    static const ShapePoint BIG_OUTLINE[] = {
        { 44, 50 }, { 80, 94 }, { 44, 125 }, { 44, 50 }, { 5, 94 }, { 44, 125 },
    };
    static const ShapePoint SMALL_OUTLINE[] = {
        { 454, 80 }, { 470, 99 }, { 454, 115 }, { 454, 80 }, { 438, 99 }, { 454, 115 },
    };
    static const ShapeBox SUN_BOX = { 95, 25, 120, 50 };
    uint32_t lit = COLORREF_RGB(0x7DD5E1);
    uint32_t shaded = COLORREF_RGB(0x6FB9E6);

    fill_sky(fb, game, SCENERY_HEIGHT, SKY);
    fill_band(fb, COLORREF_RGB(0x4080FF));
    draw_sun(fb, SUN_BOX);
    shapes_fill_polygon(fb, BIG_LEFT, COUNT_OF(BIG_LEFT), lit);
    shapes_fill_polygon(fb, SMALL_LEFT, COUNT_OF(SMALL_LEFT), lit);
    shapes_fill_polygon(fb, BIG_RIGHT, COUNT_OF(BIG_RIGHT), shaded);
    shapes_fill_polygon(fb, SMALL_RIGHT, COUNT_OF(SMALL_RIGHT), shaded);
    draw_polyline(fb, BIG_OUTLINE, COUNT_OF(BIG_OUTLINE), BLACK, 2);
    draw_polyline(fb, SMALL_OUTLINE, COUNT_OF(SMALL_OUTLINE), BLACK, 2);
    fb_draw_bitmap(fb, sphinx, 300, 45);
}

/* Two rows of tiles, plus one more at each end where the band dips. */
static void draw_water(Framebuffer *fb, const Dib *water)
{
    int column, row;

    for (column = 0; column < 7; column++) {
        for (row = 0; row < 2; row++) {
            fb_draw_bitmap(fb, water, water->width * column, water->height * row);
        }
        if (column == 0 || column == 6) {
            fb_draw_bitmap(fb, water, water->width * column, water->height * 2);
        }
    }
}

static void draw_castle(Framebuffer *fb, const Game *game, const Dib *castle)
{
    static const ShapeBox SUN_BOX = { 75, 15, 100, 40 };
    static const ShapeBox LEFT_HILL = { -35, 80, 220, 150 };
    static const ShapeBox RIGHT_HILL = { 130, 75, 510, 125 };
    uint32_t grass = COLORREF_RGB(0x008000);
    uint32_t hill_outline = COLORREF_RGB(0x004000);

    fill_sky(fb, game, SCENERY_HEIGHT, SKY);
    draw_sun(fb, SUN_BOX);
    shapes_ellipse(fb, LEFT_HILL, grass, hill_outline, 1);
    shapes_ellipse(fb, RIGHT_HILL, grass, hill_outline, 1);
    fill_band(fb, grass);
    fb_draw_bitmap(fb, castle, 300, 0);
}

static void draw_city(Framebuffer *fb, const Dib *city)
{
    fill_band(fb, BLACK);
    fb_draw_bitmap(fb, city, 0, 0);
}

static void draw_cloud(Framebuffer *fb, const Game *game)
{
    static const ShapeBox SUN_BOX = { 25, 25, 50, 50 };
    static const uint32_t RAINBOW[] = { 0x0000FF, 0x00FFFF, 0x008000, 0xFF0000, 0xFF0080 };
    static const ShapePoint START = { 400, 100 };
    static const ShapePoint END = { 100, 100 };
    int i;

    fill_sky(fb, game, CLOUD_SKY_HEIGHT, SKY);
    fill_band(fb, SKY);
    draw_sun(fb, SUN_BOX);
    /* Five nested arcs, each 5 pixels inside the previous one. */
    for (i = 0; i < COUNT_OF(RAINBOW); i++) {
        ShapeBox box = { 100 + 5 * i, 5 + 5 * i, 400 - 5 * i, 260 + 5 * i };
        shapes_arc(fb, box, START, END, COLORREF_RGB(RAINBOW[i]), 7);
    }
}

void scenery_draw(Framebuffer *fb, const Game *game, const Dib *bitmap)
{
    int scene = game_scene(game);

    switch (scene) {
    case SCENE_STAR:
        draw_star(fb, game, bitmap);
        break;
    case SCENE_SPHINX:
        draw_sphinx(fb, game, bitmap);
        break;
    case SCENE_WATER:
        draw_water(fb, bitmap);
        break;
    case SCENE_CAVE:
        fb_draw_bitmap(fb, bitmap, 0, 0);
        break;
    case SCENE_CASTLE:
        draw_castle(fb, game, bitmap);
        break;
    case SCENE_CITY:
        draw_city(fb, bitmap);
        break;
    case SCENE_CLOUD:
        /* Its CLOUD bitmap is loaded but never drawn by the original. */
        draw_cloud(fb, game);
        break;
    }
    shapes_fill_polygon(fb, FLOOR, COUNT_OF(FLOOR), FLOOR_COLORS[scene]);
}
