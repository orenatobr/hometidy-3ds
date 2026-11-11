#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Z_SHAPE 0.3f
#define Z_IMAGE 0.4f
#define Z_TEXT 0.5f
#define LINE_PX 30.0f /* height of one line of the system font at scale 1 */

static C3D_RenderTarget *g_top, *g_bot;
static C2D_TextBuf g_buf;

bool ui_init(void)
{
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) return false;
    C2D_Prepare();
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_buf = C2D_TextBufNew(16384);
    return g_top && g_bot && g_buf;
}

void ui_exit(void)
{
    C2D_TextBufDelete(g_buf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

void ui_begin(void)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(g_buf);
}

void ui_top(void)
{
    C2D_TargetClear(g_top, COL_BG);
    C2D_SceneBegin(g_top);
}

void ui_bottom(void)
{
    C2D_TargetClear(g_bot, COL_BG);
    C2D_SceneBegin(g_bot);
}

void ui_end(void) { C3D_FrameEnd(0); }

static float measure(const char *s, float scale)
{
    C2D_Text t;
    C2D_TextParse(&t, g_buf, s);
    float w, h;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    return w;
}

float ui_text_width(const char *s, float scale) { return measure(s, scale); }

float ui_text(float x, float y, float scale, u32 color, const char *s, float maxw)
{
    char tmp[200];
    snprintf(tmp, sizeof tmp, "%s", s);
    if (maxw > 0 && measure(tmp, scale) > maxw) {
        size_t n = strlen(tmp);
        while (n > 0) { /* cut whole UTF-8 characters until "..." fits */
            n--;
            while (n > 0 && (tmp[n] & 0xC0) == 0x80) n--;
            tmp[n] = '\0';
            char probe[210];
            snprintf(probe, sizeof probe, "%s...", tmp);
            if (measure(probe, scale) <= maxw) { strcat(tmp, "..."); break; }
        }
    }
    C2D_Text t;
    C2D_TextParse(&t, g_buf, tmp);
    C2D_TextOptimize(&t);
    float w, h;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    C2D_DrawText(&t, C2D_WithColor, x, y, Z_TEXT, scale, scale, color);
    return w;
}

void ui_text_center(float cx, float y, float scale, u32 color, const char *s)
{
    ui_text(cx - measure(s, scale) / 2, y, scale, color, s, 0);
}

float ui_text_wrap(float x, float y, float maxw, float scale, u32 color, const char *s)
{
    char line[200];
    size_t len = 0;
    line[0] = '\0';
    const char *p = s;
    while (*p) {
        const char *end = strchr(p, ' ');
        size_t wl = end ? (size_t)(end - p) : strlen(p);
        char word[96];
        snprintf(word, sizeof word, "%.*s", (int)wl, p);
        char trial[300];
        snprintf(trial, sizeof trial, "%s%s%s", line, len ? " " : "", word);
        if (len && measure(trial, scale) > maxw) {
            ui_text(x, y, scale, color, line, 0);
            y += LINE_PX * scale + 2;
            snprintf(line, sizeof line, "%s", word);
        } else {
            snprintf(line, sizeof line, "%.199s", trial);
        }
        len = strlen(line);
        p += wl;
        if (*p == ' ') p++;
    }
    if (len) {
        ui_text(x, y, scale, color, line, 0);
        y += LINE_PX * scale + 2;
    }
    return y;
}

void ui_rrect(float x, float y, float w, float h, float r, u32 color)
{
    C2D_DrawRectSolid(x + r, y, Z_SHAPE, w - 2 * r, h, color);
    C2D_DrawRectSolid(x, y + r, Z_SHAPE, r, h - 2 * r, color);
    C2D_DrawRectSolid(x + w - r, y + r, Z_SHAPE, r, h - 2 * r, color);
    C2D_DrawCircleSolid(x + r, y + r, Z_SHAPE, r, color);
    C2D_DrawCircleSolid(x + w - r, y + r, Z_SHAPE, r, color);
    C2D_DrawCircleSolid(x + r, y + h - r, Z_SHAPE, r, color);
    C2D_DrawCircleSolid(x + w - r, y + h - r, Z_SHAPE, r, color);
}

void ui_card(float x, float y, float w, float h)
{
    ui_rrect(x, y, w, h, 8, COL_BORDER);
    ui_rrect(x + 1, y + 1, w - 2, h - 2, 7, COL_CARD);
}

void ui_button(float x, float y, float w, float h, const char *label, bool focused, bool primary)
{
    if (focused) ui_rrect(x - 3, y - 3, w + 6, h + 6, 10, COL_ACCENT); /* visible focus ring */
    ui_rrect(x, y, w, h, 8, primary ? COL_ACCENT : COL_MUTED);
    float scale = 0.6f;
    ui_text_center(x + w / 2, y + (h - LINE_PX * scale) / 2, scale, primary ? COL_ON_ACCENT : COL_FG, label);
}

void ui_check_badge(float cx, float cy, float r, u32 fill)
{
    C2D_DrawCircleSolid(cx, cy, Z_SHAPE, r, fill);
    float t = r * 0.26f;
    float ax = cx - r * 0.48f, ay = cy + r * 0.02f, bx = cx - r * 0.12f, by = cy + r * 0.42f, dx = cx + r * 0.52f, dy = cy - r * 0.36f;
    u32 w = COL_ON_ACCENT;
    C2D_DrawLine(ax, ay, w, bx, by, w, t, Z_IMAGE);
    C2D_DrawLine(bx, by, w, dx, dy, w, t, Z_IMAGE);
    C2D_DrawCircleSolid(ax, ay, Z_IMAGE, t / 2, w);
    C2D_DrawCircleSolid(bx, by, Z_IMAGE, t / 2, w);
    C2D_DrawCircleSolid(dx, dy, Z_IMAGE, t / 2, w);
}

void ui_header(const char *title, const char *right)
{
    C2D_DrawRectSolid(0, 0, Z_SHAPE, TOP_W, 30, COL_CARD);
    C2D_DrawRectSolid(0, 30, Z_SHAPE, TOP_W, 1, COL_BORDER);
    C2D_DrawRectSolid(12, 9, Z_SHAPE, 4, 14, COL_ACCENT);
    ui_text(22, 3, 0.6f, COL_FG, title, 250);
    if (right) {
        float w = ui_text_width(right, 0.45f);
        ui_text(TOP_W - 12 - w, 7, 0.45f, COL_FG_MUTED, right, 0);
    }
}

struct UiIcon {
    C3D_Tex tex;
    Tex3DS_SubTexture sub;
    bool ready;
};

UiIcon *ui_icon_new(void)
{
    UiIcon *ic = calloc(1, sizeof *ic);
    if (!ic) return NULL;
    if (!C3D_TexInit(&ic->tex, 64, 64, GPU_RGB565)) { free(ic); return NULL; }
    C3D_TexSetFilter(&ic->tex, GPU_NEAREST, GPU_NEAREST);
    return ic;
}

void ui_icon_set(UiIcon *ic, const uint8_t *block, int size)
{
    /* SMDH icons are already in the GPU tile order: copy one row of tiles at a time into the
     * 64 wide texture. */
    memset(ic->tex.data, 0, 64 * 64 * 2);
    for (int ty = 0; ty < size / 8; ty++)
        memcpy((uint8_t *)ic->tex.data + (size_t)ty * 64 * 8 * 2, block + (size_t)ty * size * 8 * 2, (size_t)size * 8 * 2);
    C3D_TexFlush(&ic->tex);
    ic->sub = (Tex3DS_SubTexture){(u16)size, (u16)size, 0.0f, 1.0f, size / 64.0f, 1.0f - size / 64.0f};
    ic->ready = true;
}

void ui_icon_draw(const UiIcon *ic, float x, float y, float scale)
{
    if (!ic || !ic->ready) return;
    C2D_Image img = {(C3D_Tex *)&ic->tex, &ic->sub};
    C2D_DrawImageAt(img, x, y, Z_IMAGE, NULL, scale, scale);
}

void ui_icon_free(UiIcon *ic)
{
    if (!ic) return;
    C3D_TexDelete(&ic->tex);
    free(ic);
}
