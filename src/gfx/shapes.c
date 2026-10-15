#include "shapes.h"

#include <stdbool.h>
#include <stdlib.h>

/*
 * Integer arithmetic only, so the DOS build runs on a 386 without a math
 * coprocessor. Centers and pen edges fall on half pixels, so geometry is
 * worked out in doubled coordinates, where they are whole numbers.
 */

#define MAX_POLYGON_POINTS 16

typedef struct {
    int x; /* first pixel at or right of the crossing */
    int direction;
} Crossing;

/* An ellipse's center and radii, doubled. */
typedef struct {
    int center_x, center_y;
    int radius_x, radius_y;
} Ellipse;

/* A direction on screen, doubled, with y pointing up. */
typedef struct {
    int x, y;
} Ray;

static void put_pixel(Framebuffer *fb, int x, int y, uint32_t color)
{
    if (x >= 0 && y >= 0 && x < fb->width && y < fb->height) {
        fb->pixels[(size_t)y * (size_t)fb->width + (size_t)x] = color;
    }
}

static int64_t floor_div(int64_t numerator, int64_t denominator)
{
    int64_t quotient;

    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    quotient = numerator / denominator;
    return quotient * denominator > numerator ? quotient - 1 : quotient;
}

static int64_t ceil_div(int64_t numerator, int64_t denominator)
{
    return -floor_div(-numerator, denominator);
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

/*
 * The first pixel whose center is at or right of where edge a-b crosses
 * the center line of row y: ceil(crossing - 1/2).
 */
static int crossing_pixel(ShapePoint a, ShapePoint b, int y)
{
    int64_t rise = 2 * (int64_t)(b.y - a.y);
    int64_t run = (int64_t)(2 * y + 1 - 2 * a.y) * (b.x - a.x);

    return (int)ceil_div(2 * (int64_t)a.x * rise + 2 * run - rise, 2 * rise);
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
        int found = 0, winding = 0, k;

        for (i = 0; i < count; i++) {
            ShapePoint a = points[i];
            ShapePoint b = points[(i + 1) % count];
            int low = a.y < b.y ? a.y : b.y;
            int high = a.y < b.y ? b.y : a.y;

            /* Only edges spanning the row's center line, at y + 1/2. */
            if (a.y == b.y || y < low || y >= high) {
                continue;
            }
            crossings[found].x = crossing_pixel(a, b, y);
            crossings[found].direction = b.y > a.y ? 1 : -1;
            found++;
        }
        sort_crossings(crossings, found);

        for (k = 0; k + 1 < found; k++) {
            winding += crossings[k].direction;
            if (winding != 0) {
                fb_fill_rect(fb, crossings[k].x, y, crossings[k + 1].x - crossings[k].x, 1, color);
            }
        }
    }
}

static Ellipse ellipse_in(ShapeBox box)
{
    Ellipse ellipse;

    /* A 1-pixel outline spans left..right-1, as GDI draws it. */
    ellipse.center_x = box.left + box.right - 1;
    ellipse.center_y = box.top + box.bottom - 1;
    ellipse.radius_x = box.right - 1 - box.left;
    ellipse.radius_y = box.bottom - 1 - box.top;
    return ellipse;
}

/* Whether offset (dx, dy) lies inside the ellipse of radii (rx, ry); all doubled. */
static bool inside(int dx, int dy, int rx, int ry)
{
    int64_t rx2 = (int64_t)rx * rx, ry2 = (int64_t)ry * ry;

    if (rx <= 0 || ry <= 0) {
        return false;
    }
    return (int64_t)dx * dx * ry2 + (int64_t)dy * dy * rx2 <= rx2 * ry2;
}

static int64_t cross(Ray a, Ray b)
{
    return (int64_t)a.x * b.y - (int64_t)a.y * b.x;
}

static int64_t dot(Ray a, Ray b)
{
    return (int64_t)a.x * b.x + (int64_t)a.y * b.y;
}

/* 0 when `ray` is less than half a turn counterclockwise from `from`, else 1. */
static int half_turn(Ray from, Ray ray)
{
    int64_t side = cross(from, ray);
    return side > 0 || (side == 0 && dot(from, ray) > 0) ? 0 : 1;
}

/* Whether `ray` is reached strictly before `other` going counterclockwise from `from`. */
static bool turns_before(Ray from, Ray ray, Ray other)
{
    int ray_half = half_turn(from, ray), other_half = half_turn(from, other);

    if (ray_half != other_half) {
        return ray_half < other_half;
    }
    return cross(ray, other) > 0;
}

static bool same_direction(Ray a, Ray b)
{
    return cross(a, b) == 0 && dot(a, b) > 0;
}

/* The counterclockwise sweep from `start` to `end`; equal rays sweep a full turn. */
typedef struct {
    Ray start, end;
} Sweep;

static bool in_sweep(const Sweep *sweep, Ray ray)
{
    /* The center has no direction; it counts as pointing right. */
    if (ray.x == 0 && ray.y == 0) {
        ray.x = 1;
    }
    return same_direction(sweep->start, sweep->end) || !turns_before(sweep->start, sweep->end, ray);
}

/*
 * Draws the pen ring of an ellipse, and its inside when `fill` is set.
 * With a sweep, only the pixels within it are drawn.
 */
static void draw_ellipse(Framebuffer *fb, ShapeBox box, bool has_fill, uint32_t fill, uint32_t pen,
                         int pen_width, const Sweep *sweep)
{
    Ellipse e = ellipse_in(box);
    int left = (int)floor_div(e.center_x - e.radius_x - pen_width, 2);
    int right = (int)ceil_div(e.center_x + e.radius_x + pen_width, 2);
    int top = (int)floor_div(e.center_y - e.radius_y - pen_width, 2);
    int bottom = (int)ceil_div(e.center_y + e.radius_y + pen_width, 2);
    int x, y;

    for (y = top; y <= bottom; y++) {
        for (x = left; x <= right; x++) {
            int dx = 2 * x - e.center_x, dy = 2 * y - e.center_y;
            Ray ray = { dx, -dy };

            if (!inside(dx, dy, e.radius_x + pen_width, e.radius_y + pen_width)) {
                continue;
            }
            if (sweep != NULL && !in_sweep(sweep, ray)) {
                continue;
            }
            if (!inside(dx, dy, e.radius_x - pen_width, e.radius_y - pen_width)) {
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
    draw_ellipse(fb, box, true, fill, pen, pen_width, NULL);
}

void shapes_arc(Framebuffer *fb, ShapeBox box, ShapePoint start, ShapePoint end, uint32_t pen,
                int pen_width)
{
    Ellipse e = ellipse_in(box);
    Sweep sweep;

    sweep.start.x = 2 * start.x - e.center_x;
    sweep.start.y = e.center_y - 2 * start.y;
    sweep.end.x = 2 * end.x - e.center_x;
    sweep.end.y = e.center_y - 2 * end.y;
    draw_ellipse(fb, box, false, 0, pen, pen_width, &sweep);
}

/* Stamps a round pen tip, `width` pixels across, centered on (x, y). */
static void stamp_pen(Framebuffer *fb, int x, int y, uint32_t pen, int width)
{
    int i, j;

    for (j = 0; j < width; j++) {
        for (i = 0; i < width; i++) {
            int ox = 2 * i - (width - 1), oy = 2 * j - (width - 1);
            if (ox * ox + oy * oy <= width * width) {
                put_pixel(fb, x + (int)floor_div(ox + 1, 2), y + (int)floor_div(oy + 1, 2), pen);
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
