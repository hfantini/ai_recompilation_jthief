#include "game.h"

#include <stddef.h>
#include <string.h>

#define SPAWN_MARGIN 10
#define SCENERY_CLEARANCE 82
#define STAR_AREA_HEIGHT 82
#define RAIN_LEVEL 12

/* The thief's hit box spans these offsets from the cursor hotspot. */
#define THIEF_BOX_NEAR 8
#define THIEF_BOX_FAR 24
#define PLAYFIELD_MARGIN 2

static const char *const LEVEL_NAMES[GAME_LEVELS] = {
    "Emerald", "Opal", "Sapphire", "Garnet", "Pearl", "Onyx", "Ruby",
    "Aquamarine", "Gold", "Amethyst", "Peridot", "Topaz", "Citrine", "Diamond",
};

static const char *const GUARD_BITMAPS[GAME_LEVELS] = {
    "OGRE", "NAZGUL", "CAMEL", "MASK", "CLIPPER", "BATTLESHIP", "BAT",
    "SPIDER", "SOLDIER", "WIZARD", "HELICOPTER", "APACHE", "RAINDROP", "CLOUD",
};

const char *game_level_name(int level)
{
    return level >= 0 && level < GAME_LEVELS ? LEVEL_NAMES[level] : NULL;
}

const char *game_guard_bitmap(int level)
{
    return level >= 0 && level < GAME_LEVELS ? GUARD_BITMAPS[level] : NULL;
}

int game_display_level(const Game *game)
{
    return game->level < GAME_LEVELS ? game->level : GAME_LEVELS - 1;
}

int game_scene(const Game *game)
{
    return game_display_level(game) / 2;
}

/*
 * The original draws the scenery on WM_PAINT and when a level-up reaches a
 * new scene; the star scene picks its stars with rand() each time. Here the
 * scenery is drawn when a game starts or the scene changes, with the
 * rand() calls at those points.
 */
static void redraw_scenery(Game *game)
{
    int i;

    game->scenery_serial++;
    if (game_scene(game) != 0) {
        return;
    }
    for (i = 0; i < GAME_STARS; i++) {
        game->stars[i].x = rng_next(&game->rng) % (game->playfield.right - SPAWN_MARGIN);
        game->stars[i].y = rng_next(&game->rng) % STAR_AREA_HEIGHT;
    }
}

static GameSize guard_size(const Game *game)
{
    return game->sprites.guard[game_display_level(game)];
}

static GameSize jewel_size(const Game *game)
{
    return game->sprites.jewel[game_display_level(game)];
}

static int enemy_count_for(int level)
{
    /* Levels 7 and 12 are swarms. */
    if (level == 7 || level == RAIN_LEVEL) {
        return level + 10;
    }
    return (level + 1) / 2 + 3;
}

/* Fills free guard slots; the order of the rand() calls matches the original. */
static void spawn_enemies(Game *game)
{
    int base_speed = ((int)game->speed + 1) * 2 + game->level / 2;
    int i;

    game->enemy_count = enemy_count_for(game->level);
    for (i = 0; i < game->enemy_count; i++) {
        Enemy *enemy = &game->enemies[i];

        if (enemy->x >= 0) {
            continue;
        }
        enemy->x = rng_next(&game->rng) % (game->playfield.right - 2 * SPAWN_MARGIN) + SPAWN_MARGIN;
        enemy->y = rng_next(&game->rng) % (game->playfield.bottom - 2 * SPAWN_MARGIN) + SPAWN_MARGIN;
        enemy->moving_right = rng_next(&game->rng) % 2 != 0;
        enemy->moving_down = rng_next(&game->rng) % 2 != 0;
        enemy->speed_x = rng_next(&game->rng) % 4 + base_speed;
        enemy->speed_y = rng_next(&game->rng) % 4 + base_speed;
    }
}

/*
 * Jewels in the upper band (y below 151) are pushed right, more so the
 * higher they are, keeping them off the scenery in the top-left corner.
 */
static void spawn_jewels(Game *game)
{
    int i;

    game->jewel_count = (game->level + 2) * 2;
    game->jewels_left = game->jewel_count;
    for (i = 0; i < game->jewel_count; i++) {
        Jewel *jewel = &game->jewels[i];
        int band = rng_next(&game->rng) % (game->playfield.bottom - 120);

        jewel->y = band + 100;
        if (jewel->y < 151 || jewel->y > game->playfield.bottom) {
            jewel->x = rng_next(&game->rng) % ((band + 70) * 4) + (150 - jewel->y) * 2;
        }
        else {
            jewel->x = rng_next(&game->rng) % (game->playfield.right - 2 * SPAWN_MARGIN);
        }
        jewel->present = true;
    }
}

void game_init(Game *game, GameRect playfield, const GameSprites *sprites, uint16_t seed,
               GameSpeed speed, bool cheat)
{
    memset(game, 0, sizeof(*game));
    rng_seed(&game->rng, seed);
    game->playfield = playfield;
    game->sprites = *sprites;
    game_new(game, speed, cheat);
}

void game_new(Game *game, GameSpeed speed, bool cheat)
{
    int i;

    game->speed = speed;
    game->cheat = cheat;
    game->lives = GAME_START_LIVES;
    game->next_extra_life = GAME_EXTRA_LIFE_STEP;
    game->level = 0;
    game->score = 0;
    game->over = false;
    /* A new game spawns jewels before guards; a level-up does the opposite. */
    spawn_jewels(game);
    for (i = 0; i < GAME_MAX_ENEMIES; i++) {
        game->enemies[i].x = -1;
    }
    spawn_enemies(game);
    /* The original paints the new game, scenery included, after spawning. */
    redraw_scenery(game);
}

bool game_in_playfield(const Game *game, int x, int y)
{
    const GameRect *field = &game->playfield;

    return x - PLAYFIELD_MARGIN >= field->left && x + THIEF_BOX_FAR <= field->right
        && y - PLAYFIELD_MARGIN >= field->top && y + THIEF_BOX_FAR <= field->bottom;
}

static bool thief_overlaps(const Game *game, int x, int y, GameSize size)
{
    return x <= game->cursor_x + THIEF_BOX_FAR && game->cursor_x + THIEF_BOX_NEAR <= x + size.width
        && y <= game->cursor_y + THIEF_BOX_FAR && game->cursor_y + THIEF_BOX_NEAR <= y + size.height;
}

static bool thief_caught(const Game *game)
{
    int i;

    if (!game_in_playfield(game, game->cursor_x, game->cursor_y)) {
        return false;
    }
    for (i = 0; i < game->enemy_count; i++) {
        if (thief_overlaps(game, game->enemies[i].x, game->enemies[i].y, guard_size(game))) {
            return true;
        }
    }
    return false;
}

static int jewel_under_thief(const Game *game)
{
    int i;

    if (!game_in_playfield(game, game->cursor_x, game->cursor_y)) {
        return -1;
    }
    for (i = 0; i < game->jewel_count; i++) {
        const Jewel *jewel = &game->jewels[i];

        if (jewel->present && thief_overlaps(game, jewel->x, jewel->y, jewel_size(game))) {
            return i;
        }
    }
    return -1;
}

static unsigned end_game(Game *game, unsigned event)
{
    game->over = true;
    return event;
}

void game_set_focus(Game *game, bool focused)
{
    game->player_active = focused;
}

static unsigned take_jewel(Game *game, int index)
{
    unsigned events = GAME_EVENT_JEWEL_TAKEN;

    game->jewels[index].present = false;
    game->jewels_left--;
    game->score += game->level + 1;
    if (game->next_extra_life <= game->score && game->lives < GAME_MAX_LIVES) {
        game->lives++;
        game->next_extra_life += GAME_EXTRA_LIFE_STEP;
        events |= GAME_EVENT_EXTRA_LIFE;
    }
    if (game->jewels_left > 0) {
        return events;
    }

    game->level++;
    if (game->level >= GAME_LEVELS) {
        return end_game(game, events | GAME_EVENT_GAME_COMPLETED);
    }
    if (game->level % 2 == 0) {
        redraw_scenery(game);
    }
    spawn_enemies(game);
    spawn_jewels(game);
    return events | GAME_EVENT_LEVEL_UP;
}

unsigned game_mouse_move(Game *game, int x, int y)
{
    int jewel;

    if (game->over) {
        return 0;
    }
    if (!game_in_playfield(game, x, y)) {
        unsigned events = 0;

        game->player_active = false;
        if (game->god_mode) {
            return 0;
        }
        if (!game->cheat) {
            game->lives--;
            events |= GAME_EVENT_LIFE_LOST;
        }
        return game->lives != 0 ? events : end_game(game, events | GAME_EVENT_GAME_OVER);
    }

    game->player_active = true;
    game->facing_left = x < game->cursor_x;
    game->cursor_x = x;
    game->cursor_y = y;
    jewel = jewel_under_thief(game);
    return jewel >= 0 ? take_jewel(game, jewel) : 0;
}

static void move_enemies(Game *game)
{
    const GameRect *field = &game->playfield;
    GameSize size = guard_size(game);
    bool clearance = game->level == 0 || game->level == 2 || game->level == 8 || game->level == 9;
    int upper = clearance ? field->top + SCENERY_CLEARANCE : field->top - 10;
    int i;

    for (i = 0; i < game->enemy_count; i++) {
        Enemy *enemy = &game->enemies[i];

        if (enemy->x + size.width >= field->right) {
            enemy->moving_right = false;
        }
        else if (enemy->x - size.width <= field->left) {
            enemy->moving_right = true;
        }

        if (enemy->y + size.height >= field->bottom) {
            if (game->level == RAIN_LEVEL) {
                /* Rain falls back in from the top instead of bouncing. */
                enemy->y = 0;
                enemy->moving_down = true;
            }
            else {
                enemy->moving_down = false;
            }
        }
        else if (enemy->y - size.height <= upper) {
            enemy->moving_down = true;
        }

        enemy->x += enemy->moving_right ? enemy->speed_x : -enemy->speed_x;
        enemy->y += enemy->moving_down ? enemy->speed_y : -enemy->speed_y;
    }
}

unsigned game_tick(Game *game)
{
    if (game->over) {
        return 0;
    }
    move_enemies(game);
    if (game->god_mode || !thief_caught(game) || !game->player_active) {
        return 0;
    }
    if (game->lives < 1) {
        return end_game(game, GAME_EVENT_GAME_OVER);
    }
    game->lives--;
    game->player_active = false;
    return GAME_EVENT_LIFE_LOST;
}

void game_debug_set_level(Game *game, int level)
{
    if (level < 0 || level >= GAME_LEVELS) {
        return;
    }
    game->level = level;
    redraw_scenery(game);
    spawn_enemies(game);
    spawn_jewels(game);
}
