#ifndef JT_RENDER_H
#define JT_RENDER_H

/*
 * Draws game frames with the original's layout. The scenery is drawn into a
 * background layer only when the game asks for it, and each frame copies
 * that layer before drawing the sprites. Text is left to the platform,
 * which owns the fonts: render_status_text() says what to print and where.
 */

#include "assets.h"
#include "framebuffer.h"
#include "game.h"

typedef struct {
    Dib guards[GAME_LEVELS];
    Dib jewels[GAME_LEVELS];
    Dib scenery[GAME_SCENES];
    Dib status_icons;
    Framebuffer background;
    unsigned background_serial;
    bool background_ready;
} Renderer;

typedef struct {
    int x, y;
    char text[32];
} RenderText;

typedef struct {
    RenderText level_name;
    RenderText lives;
    RenderText score;
} RenderStatusText;

typedef enum {
    RENDER_OK = 0,
    RENDER_ERR_MISSING_BITMAP,
    RENDER_ERR_OUT_OF_MEMORY
} RenderStatus;

/* On RENDER_ERR_MISSING_BITMAP, `missing` names the bitmap. */
RenderStatus render_init(Renderer *renderer, const Assets *assets, int width, int height,
                         const char **missing);

void render_destroy(Renderer *renderer);

/* The bitmap sizes the core's rules depend on. */
void render_sprite_sizes(const Renderer *renderer, GameSprites *out);

/* `fb` must have the size given to render_init. */
void render_frame(Renderer *renderer, Framebuffer *fb, const Game *game);

void render_status_text(const Game *game, RenderStatusText *out);

#endif
