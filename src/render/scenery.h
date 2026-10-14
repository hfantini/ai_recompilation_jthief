#ifndef JT_RENDER_SCENERY_H
#define JT_RENDER_SCENERY_H

/*
 * The original's seven backgrounds, one per pair of levels: the scenery at
 * the top, the band around the horizon, and the floor whose color the
 * sprites were designed against.
 */

#include "dib.h"
#include "framebuffer.h"
#include "game.h"

/* Name of the bitmap the scene uses. */
const char *scenery_bitmap(int scene);

/* Draws the scene of the current level over the whole playfield. */
void scenery_draw(Framebuffer *fb, const Game *game, const Dib *bitmap);

#endif
