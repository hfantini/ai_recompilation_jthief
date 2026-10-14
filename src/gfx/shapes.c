#include "shapes.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_POLYGON_POINTS 16
#define TWO_PI 6.28318530717958647692

typedef struct {
    double x;
    int direction;
} Crossing;

/* Where an ellipse's center and radii sit, in pixel coordinates. */
typedef struct {
    double center_x, center_y;
    double radius_x, radius_y;
} Ellipse;

static void put_pixel(Framebuffer *fb, int x, int y, uint32_t color)
{
    if (x >= 0 && y >= 0 && x < fb->width && y < fb->height) {
        fb->pixels[(size_t)y * (size_t)fb->width + (size_t)x] = color;
    }
}

static void sort_crossings(Crossing *crossings, int count)
{
    int i, j;

    for (i = 1; i < count; i++) {
        Crossing current = crossings[i];
        for (j = i; j > 0 && crossings[j - 1].x > current.x; j--) {
            crossings[j] = crossings[j - 1];
        }
        crossings[j] = current;
    }
}

void shapes_fill_polygon(Framebuffer *fb, const ShapePoint *points, int count, uint32_t color)
{
    Crossing crossings[MAX_POLYGON_POINTS];
    int top = points[0].y, bottom = points[0].y;
    int i, y;

    if (count < 3 || count > MAX_POLYGON_POINTS) {
        return;
    }
    for (i = 1; i < count; i++) {
        top = points[i].y < top ? points[i].y : top;
        bottom = points[i].y > bottom ? points[i].y : bottom;
    }

    for (y = top; y < bottom; y++) {
        double center_y = y + 0.5;
        int found = 0, winding = 0, k;

        for (i = 0; i < count; i++) {
            ShapePoint a = points[i];
            ShapePoint b = points[(i + 1) % count];
            int low = a.y < b.y ? a.y : b.y;
            int high = a.y < b.y ? b.y : a.y;

            if (a.y == b.y || center_y < low || center_y >= high) {
                continue;
            }
            crossings[found].x = a.x + (center_y - a.y) * (b.x - a.x) / (double)(b.y - a.y);
            crossings[found].direction = b.y > a.y ? 1 : -1;
            found++;
        }
        sort_crossings(crossings, found);

        for (k = 0; k + 1 < found; k++) {
            winding += crossings[k].direction;
            if (winding != 0) {
                int from = (int)ceil(crossings[k].x - 0.5);
                int to = (int)ceil(crossings[k + 1].x - 0.5);
                fb_fill_rect(fb, from, y, to - from, 1, color);
            }
        }
    }
}

static Ellipse ellipse_in(ShapeBox box)
{
    Ellipse ellipse;

    /* A 1-pixel outline spans left..right-1, as GDI draws it. */
    ellipse.center_x = (box.left + box.right - 1) / 2.0;
    ellipse.center_y = (box.top + box.bottom - 1) / 2.0;
    ellipse.radius_x = (box.right - 1 - box.left) / 2.0;
    ellipse.radius_y = (box.bottom - 1 - box.top) / 2.0;
    return ellipse;
}

static bool inside(double dx, double dy, double radius_x, double radius_y)
{
    if (radius_x <= 0 || radius_y <= 0) {
        return false;
    }
    return (dx * dx) / (radius_x * radius_x) + (dy * dy) / (radius_y * radius_y) <= 1.0;
}

/* Angle of (dx, dy) on screen, counterclockwise from the right, in [0, 2pi). */
static double screen_angle(double dx, double dy)
{
    double angle = atan2(-dy, dx);
    return angle < 0 ? angle + TWO_PI : angle;
}

/*
 * Draws the pen ring of an ellipse, and its inside when `fill` is set.
 * With an arc, only the pixels between its start and end angles are drawn.
 */
static void draw_ellipse(Framebuffer *fb, ShapeBox box, bool has_fill, uint32_t fill, uint32_t pen,
                         int pen_width, const double *arc_start, double arc_span)
{
    Ellipse e = ellipse_in(box);
    double half = pen_width / 2.0;
    int left = (int)floor(e.center_x - e.radius_x - half);
    int right = (int)ceil(e.center_x + e.radius_x + half);
    int top = (int)floor(e.center_y - e.radius_y - half);
    int bottom = (int)ceil(e.center_y + e.radius_y + half);
    int x, y;

    for (y = top; y <= bottom; y++) {
        for (x = left; x <= right; x++) {
            double dx = x - e.center_x, dy = y - e.center_y;

            if (!inside(dx, dy, e.radius_x + half, e.radius_y + half)) {
                continue;
            }
            if (arc_start != NULL) {
                double offset = screen_angle(dx, dy) - *arc_start;
                if (offset < 0) {
                    offset += TWO_PI;
                }
                if (offset > arc_span) {
                    continue;
                }
            }
            if (!inside(dx, dy, e.radius_x - half, e.radius_y - half)) {
                put_pixel(fb, x, y, pen);
            }
            else if (has_fill) {
                put_pixel(fb, x, y, fill);
            }
        }
    }
}

void shapes_ellipse(Framebuffer *fb, ShapeBox box, uint32_t fill, uint32_t pen, int pen_width)
{
    draw_ellipse(fb, box, true, fill, pen, pen_width, NULL, 0);
}

void shapes_arc(Framebuffer *fb, ShapeBox box, ShapePoint start, ShapePoint end, uint32_t pen,
                int pen_width)
{
    Ellipse e = ellipse_in(box);
    double from = screen_angle(start.x - e.center_x, start.y - e.center_y);
    double to = screen_angle(end.x - e.center_x, end.y - e.center_y);
    double span = to - from;

    /* Equal rays mean a full ellipse, as in GDI. */
    if (span <= 0) {
        span += TWO_PI;
    }
    draw_ellipse(fb, box, false, 0, pen, pen_width, &from, span);
}

/* Stamps a round pen tip, `width` pixels across, centered on (x, y). */
static void stamp_pen(Framebuffer *fb, int x, int y, uint32_t pen, int width)
{
    double reach = (width - 1) / 2.0;
    double radius = width / 2.0;
    int i, j;

    for (j = 0; j < width; j++) {
        for (i = 0; i < width; i++) {
            double ox = i - reach, oy = j - reach;
            if (ox * ox + oy * oy <= radius * radius) {
                put_pixel(fb, x + (int)floor(ox + 0.5), y + (int)floor(oy + 0.5), pen);
            }
        }
    }
}

void shapes_line(Framebuffer *fb, ShapePoint from, ShapePoint to, uint32_t pen, int pen_width)
{
    int dx = abs(to.x - from.x), dy = -abs(to.y - from.y);
    int step_x = from.x < to.x ? 1 : -1, step_y = from.y < to.y ? 1 : -1;
    int error = dx + dy;
    int x = from.x, y = from.y;

    /* Bresenham, stamping the pen at each step. */
    for (;;) {
        /* Both tests must see the error from before this step. */
        int doubled = 2 * error;

        stamp_pen(fb, x, y, pen, pen_width);
        if (x == to.x && y == to.y) {
            break;
        }
        if (doubled >= dy) {
            error += dy;
            x += step_x;
        }
        if (doubled <= dx) {
            error += dx;
            y += step_y;
        }
    }
}
