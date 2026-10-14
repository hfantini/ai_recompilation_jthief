#ifndef JT_GFX_SHAPES_H
#define JT_GFX_SHAPES_H

/*
 * GDI-like shapes drawn into a framebuffer, enough to redraw the original's
 * scenery. Boxes follow GDI: right and bottom are exclusive. Pens are
 * centered on the outline and have round ends, like GDI geometric pens.
 * The result matches the drawing, not GDI pixel for pixel.
 */

#include "framebuffer.h"

typedef struct {
    int x, y;
} ShapePoint;

typedef struct {
    int left, top, right, bottom;
} ShapeBox;

/* Fills the pixels whose center is inside the polygon (nonzero winding). */
void shapes_fill_polygon(Framebuffer *fb, const ShapePoint *points, int count, uint32_t color);

/* The ellipse inscribed in `box`, filled, then outlined with a pen. */
void shapes_ellipse(Framebuffer *fb, ShapeBox box, uint32_t fill, uint32_t pen, int pen_width);

/*
 * The part of the outline of the ellipse inscribed in `box` that runs
 * counterclockwise from the ray toward `start` to the ray toward `end`.
 */
void shapes_arc(Framebuffer *fb, ShapeBox box, ShapePoint start, ShapePoint end, uint32_t pen,
                int pen_width);

/* A line including both ends. */
void shapes_line(Framebuffer *fb, ShapePoint from, ShapePoint to, uint32_t pen, int pen_width);

#endif
