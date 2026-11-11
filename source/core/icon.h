/** @file icon.h
 *  Green check badge on the HOME icons (SMDH icon blocks: RGB565, 8x8 tiles, Morton order).
 */
#ifndef HT_ICON_H
#define HT_ICON_H

#include <stdbool.h>
#include <stdint.h>

#include "smdh.h"

/** Draw the badge on both icons of an SMDH. Drawing twice gives the same bytes. */
void icon_mark(uint8_t *smdh);

/** True when both icons carry the badge (also recognizes badges drawn by older tools). */
bool icon_is_marked(const uint8_t *smdh);

/** Decode the pixel (x, y) of an icon block to 8-bit RGB. */
void icon_pixel(const uint8_t *icon, int size, int x, int y, uint8_t rgb[3]);

#endif
