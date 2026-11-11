#include "icon.h"

#include <math.h>
#include <stdlib.h>

#define SS 8 /* subsamples per pixel side */

static const uint8_t GREEN[3] = {62, 172, 70};

static int morton(int x, int y)
{
    int m = 0;
    for (int b = 0; b < 3; b++) m |= ((x >> b) & 1) << (2 * b) | ((y >> b) & 1) << (2 * b + 1);
    return m;
}

static size_t pixel_offset(int size, int x, int y)
{
    int tile = (y / 8) * (size / 8) + x / 8;
    return 2 * (size_t)(tile * 64 + morton(x % 8, y % 8));
}

void icon_pixel(const uint8_t *icon, int size, int x, int y, uint8_t rgb[3])
{
    size_t o = pixel_offset(size, x, y);
    unsigned v = (unsigned)(icon[o] | icon[o + 1] << 8);
    unsigned r = v >> 11, g = (v >> 5) & 0x3F, b = v & 0x1F;
    rgb[0] = (uint8_t)(r << 3 | r >> 2);
    rgb[1] = (uint8_t)(g << 2 | g >> 4);
    rgb[2] = (uint8_t)(b << 3 | b >> 2);
}

/* Top right corner, a bit under 40% of the icon. */
static void badge_box(int size, int *x0, int *y0, int *side)
{
    int s = (int)lround(size * 0.375);
    int margin = (int)lround(size / 24.0);
    if (margin < 1) margin = 1;
    *side = s;
    *x0 = size - margin - s;
    *y0 = margin;
}

static double seg_dist(double px, double py, double ax, double ay, double bx, double by)
{
    double dx = bx - ax, dy = by - ay;
    double t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    double cx = ax + t * dx - px, cy = ay + t * dy - py;
    return sqrt(cx * cx + cy * cy);
}

static bool in_round_rect(double x, double y, double big, double r)
{
    if (x < 0 || y < 0 || x > big || y > big) return false;
    double cx = x < r ? r : x > big - r ? big - r : x;
    double cy = y < r ? r : y > big - r ? big - r : y;
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
}

/* Badge pixel (px, py) inside the box: returns false outside the rounded square. */
static bool badge_pixel(int side, int px, int py, uint8_t rgb[3])
{
    double big = side * SS, r = big * 0.22, half = big * 0.17 / 2;
    const double p[3][2] = {{big * 0.22, big * 0.52}, {big * 0.43, big * 0.72}, {big * 0.78, big * 0.30}};
    int inside = 0, white = 0;
    for (int sy = 0; sy < SS; sy++)
        for (int sx = 0; sx < SS; sx++) {
            double x = px * SS + sx + 0.5, y = py * SS + sy + 0.5;
            inside += in_round_rect(x, y, big, r);
            white += seg_dist(x, y, p[0][0], p[0][1], p[1][0], p[1][1]) <= half ||
                     seg_dist(x, y, p[1][0], p[1][1], p[2][0], p[2][1]) <= half;
        }
    if (inside * 2 < SS * SS) return false; /* hard edge: colors never depend on what is underneath */
    double w = (double)white / (SS * SS);
    for (int c = 0; c < 3; c++) rgb[c] = (uint8_t)lround(GREEN[c] + (255 - GREEN[c]) * w);
    return true;
}

static void put_rgb(uint8_t *icon, int size, int x, int y, const uint8_t rgb[3])
{
    size_t o = pixel_offset(size, x, y);
    unsigned v = (unsigned)(rgb[0] >> 3) << 11 | (unsigned)(rgb[1] >> 2) << 5 | (unsigned)(rgb[2] >> 3);
    icon[o] = (uint8_t)(v & 0xFF);
    icon[o + 1] = (uint8_t)(v >> 8);
}

static void mark_one(uint8_t *icon, int size)
{
    int x0, y0, side;
    badge_box(size, &x0, &y0, &side);
    for (int py = 0; py < side; py++)
        for (int px = 0; px < side; px++) {
            uint8_t rgb[3];
            if (badge_pixel(side, px, py, rgb)) put_rgb(icon, size, x0 + px, y0 + py, rgb);
        }
}

/* Share of badge pixels that look like the badge (old tools antialiased differently). */
static bool looks_marked(const uint8_t *icon, int size)
{
    int x0, y0, side, total = 0, hit = 0;
    badge_box(size, &x0, &y0, &side);
    for (int py = 0; py < side; py++)
        for (int px = 0; px < side; px++) {
            uint8_t want[3], got[3];
            if (!badge_pixel(side, px, py, want)) continue;
            icon_pixel(icon, size, x0 + px, y0 + py, got);
            total++;
            hit += abs(got[0] - want[0]) <= 60 && abs(got[1] - want[1]) <= 60 && abs(got[2] - want[2]) <= 60;
        }
    return total && hit * 100 >= total * 92;
}

void icon_mark(uint8_t *smdh)
{
    mark_one(smdh + SMDH_ICON_SMALL, 24);
    mark_one(smdh + SMDH_ICON_LARGE, 48);
}

bool icon_is_marked(const uint8_t *smdh)
{
    return looks_marked(smdh + SMDH_ICON_SMALL, 24) && looks_marked(smdh + SMDH_ICON_LARGE, 48);
}
