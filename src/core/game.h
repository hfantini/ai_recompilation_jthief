#ifndef JT_CORE_GAME_H
#define JT_CORE_GAME_H

/*
 * Platform-independent game state and rules, transcribed from Jewel Thief
 * v1.4 (see docs/research.md). Sprite sizes come in as plain numbers: the
 * core knows nothing about bitmaps, windows or time.
 *
 * The original's quirks are kept on purpose, for fidelity:
 * - every mouse move outside the playfield costs a life, not just leaving it;
 * - a guard catching the thief on the last life leaves 0 lives and the game
 *   goes on until the next catch;
 * - at 9 lives the extra life threshold does not advance, so the next jewel
 *   taken below 9 lives grants it at once.
 */

#include "rng.h"

#include <stdbool.h>

#define GAME_LEVELS 14
#define GAME_SCENES 7
#define GAME_STARS 10
#define GAME_MAX_ENEMIES 32
#define GAME_MAX_JEWELS 32
#define GAME_START_LIVES 9
#define GAME_MAX_LIVES 9
#define GAME_EXTRA_LIFE_STEP 300

typedef enum {
    GAME_SPEED_BEGINNER,
    GAME_SPEED_INTERMEDIATE,
    GAME_SPEED_EXPERT
} GameSpeed;

/* What happened during an update, as bit flags, so the platform can react. */
enum {
    GAME_EVENT_LIFE_LOST = 1 << 0,
    GAME_EVENT_EXTRA_LIFE = 1 << 1,
    GAME_EVENT_JEWEL_TAKEN = 1 << 2,
    GAME_EVENT_LEVEL_UP = 1 << 3,
    GAME_EVENT_GAME_OVER = 1 << 4,
    GAME_EVENT_GAME_COMPLETED = 1 << 5
};

typedef struct {
    int left, top, right, bottom;
} GameRect;

typedef struct {
    int width, height;
} GameSize;

/* Bitmap sizes per level, which the collision and bounce rules depend on. */
typedef struct {
    GameSize guard[GAME_LEVELS];
    GameSize jewel[GAME_LEVELS];
} GameSprites;

/* A guard. As in the original, a negative x marks a free slot. */
typedef struct {
    int x, y;
    int speed_x, speed_y;
    bool moving_right;
    bool moving_down;
} Enemy;

typedef struct {
    int x, y;
    bool present;
} Jewel;

typedef struct {
    int x, y;
} GamePoint;

typedef struct {
    Rng rng;
    GameRect playfield;
    GameSprites sprites;
    GameSpeed speed;
    bool cheat;

    int level;
    int lives;
    int score;              /* displayed times 100 */
    int next_extra_life;
    bool over;              /* game over or completed; updates are ignored */

    Enemy enemies[GAME_MAX_ENEMIES];
    int enemy_count;
    Jewel jewels[GAME_MAX_JEWELS];
    int jewel_count;
    int jewels_left;

    /* Changes whenever the scenery must be drawn again. */
    unsigned scenery_serial;
    /* Where the star scene puts its stars, drawn at random each time. */
    GamePoint stars[GAME_STARS];

    /* Last cursor position inside the playfield. */
    int cursor_x, cursor_y;
    bool facing_left;
    /* False after a catch, or without focus, until the mouse moves again. */
    bool player_active;

    /* Debug aid, not in the original: guards and leaving cost no lives. */
    bool god_mode;
} Game;

/* Name of the level, which is also the name of its jewel bitmap. */
const char *game_level_name(int level);

const char *game_guard_bitmap(int level);

/* Seeds the generator, like srand() on WM_CREATE, and starts a game. */
void game_init(Game *game, GameRect playfield, const GameSprites *sprites, uint16_t seed,
               GameSpeed speed, bool cheat);

/* Starts a new game at level 0 without reseeding. */
void game_new(Game *game, GameSpeed speed, bool cheat);

/* Level being played, clamped for display once all 14 are completed. */
int game_display_level(const Game *game);

/* Each scenery serves two levels. */
int game_scene(const Game *game);

bool game_in_playfield(const Game *game, int x, int y);

void game_set_focus(Game *game, bool focused);

/* The cursor moved to (x, y), its hotspot being the top-left corner. */
unsigned game_mouse_move(Game *game, int x, int y);

/* One timer tick: guards move and may catch the thief. */
unsigned game_tick(Game *game);

/* Test helper: jumps to `level` the way a level-up does. */
void game_debug_set_level(Game *game, int level);

#endif
