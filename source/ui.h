/** @file ui.h
 *  Drawing primitives for the two screens (citro2d): dark theme, green accent.
 */
#ifndef HT_UI_H
#define HT_UI_H

#include <3ds.h>
#include <citro2d.h>
#include <stdbool.h>

#define TOP_W 400
#define BOT_W 320
#define SCREEN_H 240

/* Design tokens (dark, OLED friendly) */
#define COL_BG C2D_Color32(0x0F, 0x17, 0x2A, 0xFF)
#define COL_CARD C2D_Color32(0x1B, 0x23, 0x36, 0xFF)
#define COL_MUTED C2D_Color32(0x27, 0x2F, 0x42, 0xFF)
#define COL_BORDER C2D_Color32(0x47, 0x55, 0x69, 0xFF)
#define COL_FG C2D_Color32(0xF8, 0xFA, 0xFC, 0xFF)
#define COL_FG_MUTED C2D_Color32(0x94, 0xA3, 0xB8, 0xFF)
#define COL_ACCENT C2D_Color32(0x22, 0xC5, 0x5E, 0xFF)
#define COL_ON_ACCENT C2D_Color32(0x0F, 0x17, 0x2A, 0xFF)
#define COL_DANGER C2D_Color32(0xEF, 0x44, 0x44, 0xFF)
#define COL_WARN C2D_Color32(0xF5, 0x9E, 0x0B, 0xFF)

bool ui_init(void);
void ui_exit(void);

/** Start a frame; draw with ui_top() / ui_bottom() before ui_end(). */
void ui_begin(void);
void ui_top(void);
void ui_bottom(void);
void ui_end(void);

/** Draw text; returns its width. maxw > 0 cuts it with "..." to fit. */
float ui_text(float x, float y, float scale, u32 color, const char *s, float maxw);
float ui_text_width(const char *s, float scale);
void ui_text_center(float cx, float y, float scale, u32 color, const char *s);

/** Draw text wrapped at word boundaries inside maxw; returns the y below the last line. */
float ui_text_wrap(float x, float y, float maxw, float scale, u32 color, const char *s);

/** Rounded rectangle. */
void ui_rrect(float x, float y, float w, float h, float r, u32 color);
void ui_card(float x, float y, float w, float h);

/** Button with a label; focus draws an accent ring, primary fills it with the accent. */
void ui_button(float x, float y, float w, float h, const char *label, bool focused, bool primary);

/** Green circle with a white check. */
void ui_check_badge(float cx, float cy, float r, u32 fill);

/** Thin header with title (and optional right text). */
void ui_header(const char *title, const char *right);

/** Upload an SMDH icon (24 or 48 px) and draw it scaled. Handle with ui_icon_set. */
typedef struct UiIcon UiIcon;
UiIcon *ui_icon_new(void);
void ui_icon_set(UiIcon *ic, const uint8_t *icon_block, int size);
void ui_icon_draw(const UiIcon *ic, float x, float y, float scale);
void ui_icon_free(UiIcon *ic);

#endif
