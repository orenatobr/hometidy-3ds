/* HomeTidy: sorts the HOME Menu and draws a green check on finished games.
 *
 * Runs as a CIA on Luma3DS. Every write is preceded by a backup on the SD card and
 * checked by reading it back; a failed write puts the original bytes back.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/icon.h"
#include "core/layout.h"
#include "core/smdh.h"
#include "core/sortkey.h"
#include "core/store.h"
#include "fsio.h"
#include "i18n.h"
#include "region.h"
#include "ui.h"

#define ROWS_MAX (2 * LAYOUT_SLOTS + LAYOUT_FOLDERS)
#define ROOT "sdmc:/3ds/HomeTidy"
#define SORT_PAGE 10
#define MARK_PAGE 6
#define VERSION "0.1.0"

typedef enum {
    ST_LOADING, ST_MENU, ST_SORT, ST_SORT_CONFIRM, ST_RESTORE_CONFIRM, ST_WORK, ST_DONE, ST_ERROR, ST_MARK, ST_REBOOT
} State;

typedef struct {
    size_t idx; /* line of Cache.dat / block of CacheD.dat */
    uint64_t tid;
    const NameEntry *nm;
    bool marked;
    bool has_backup;
} Game;

typedef struct {
    State state, after_error;
    bool quit, dirty;
    int tick;
    char msg[256], detail[160], backup[96];

    const Region *region;
    HomeFs fs;
    uint8_t *launcher, *save, *cache, *cached;
    size_t ll, sl, cl, dl;
    NameDB *names;

    uint8_t *new_l, *new_s;
    LayoutRow *rows;
    size_t nrows, changes, n_titles, n_folders, unnamed, hidden, bad_blocks;
    int scroll;
    bool can_restore;

    Game *games;
    size_t ngames;
    int sel, mscroll;
    bool show_all, have_original;
    UiIcon *ic_now, *ic_after;
    char after_label[32];

    int menu_focus;
    u32 reboot_rc;
    u64 reboot_at;
} App;

/* ---- helpers ------------------------------------------------------------------------- */

static void fail_msg(App *a, State next, const char *msg)
{
    snprintf(a->msg, sizeof a->msg, "%s", msg);
    a->after_error = next;
    a->state = ST_ERROR;
}

static void fail_rc(App *a, State next, StrId id, Result rc)
{
    snprintf(a->msg, sizeof a->msg, tr(id), (unsigned long)rc);
    a->after_error = next;
    a->state = ST_ERROR;
}

static bool hit(const touchPosition *t, float x, float y, float w, float h)
{
    return t->px >= x && t->px < x + w && t->py >= y && t->py < y + h;
}

static void lock_home(App *a)
{
    a->dirty = true;
    aptSetHomeAllowed(false); /* HOME would write its own copy of the layout over ours */
}

static void draw(App *a);

static void progress(App *a, StrId what)
{
    snprintf(a->msg, sizeof a->msg, "%s", tr(what));
    State prev = a->state;
    a->state = ST_WORK;
    draw(a);
    a->state = prev;
}

/* ---- loading ------------------------------------------------------------------------- */

static bool is_listed_kind(uint64_t tid)
{
    uint32_t hi = (uint32_t)(tid >> 32);
    return hi != 0x00040010 && hi != 0x00048004 && hi != 0x00040030 && hi != 0x00040130 && hi != 0x00040138;
}

static bool backup_path_for(uint64_t tid, char *out, size_t cap)
{
    return snprintf(out, cap, ROOT "/icons/%016llX.bin", (unsigned long long)tid) < (int)cap;
}

static void refresh_game(Game *g, const App *a)
{
    const uint8_t *smdh = a->cached + g->idx * SMDH_SIZE;
    char p[96];
    g->marked = icon_is_marked(smdh);
    g->has_backup = backup_path_for(g->tid, p, sizeof p) && sd_exists(p);
}

static void build_games(App *a)
{
    size_t lines = a->cl / CACHE_ENTRY;
    if (a->dl / SMDH_SIZE < lines) lines = a->dl / SMDH_SIZE;
    free(a->games);
    a->games = calloc(lines ? lines : 1, sizeof(Game));
    a->ngames = 0;
    if (!a->games) return;
    for (size_t i = 0; i < lines; i++) {
        uint64_t tid = rd64(a->cache + i * CACHE_ENTRY + 8);
        const NameEntry *nm = namedb_find(a->names, tid);
        if (!nm || !smdh_valid(a->cached + i * SMDH_SIZE) || !is_listed_kind(tid) || !layout_has_title(a->launcher, a->save, tid)) continue;
        if (!a->show_all && layout_folder_of(a->launcher, a->save, tid) < 0) continue; /* games sit in folders, apps at the top */
        bool dup = false;
        for (size_t k = 0; k < a->ngames; k++) dup |= a->games[k].tid == tid;
        if (dup) continue;
        Game g = {.idx = i, .tid = tid, .nm = nm};
        refresh_game(&g, a);
        size_t j = a->ngames++; /* insertion sort by title */
        while (j > 0 && sk_order(a->games[j - 1].nm->name, a->games[j - 1].nm->key, nm->name, nm->key) > 0) {
            a->games[j] = a->games[j - 1];
            j--;
        }
        a->games[j] = g;
    }
}

/* Names that could not be read from the icon cache are kept in names.txt on the SD card:
 * every name seen is saved there, and the saved ones fill gaps on later runs. */
#define NAMES_FILE ROOT "/names.txt"

static void merge_saved_names(App *a)
{
    FILE *f = fopen(NAMES_FILE, "r");
    if (!f) return;
    char line[200];
    while (fgets(line, sizeof line, f)) {
        unsigned long long tid = 0;
        int off = 0;
        line[strcspn(line, "\r\n")] = '\0';
        if (sscanf(line, "%llx %n", &tid, &off) == 1 && off > 0 && line[off] && !namedb_find(a->names, tid)) namedb_set(a->names, tid, line + off);
    }
    fclose(f);
}

static void save_names(const App *a)
{
    size_t cap = a->names->n * (NAME_MAX_BYTES + 24) + 1, n = 0;
    char *txt = malloc(cap);
    if (!txt) return;
    for (size_t i = 0; i < a->names->n; i++)
        n += (size_t)snprintf(txt + n, cap - n, "%016llX\t%s\n", (unsigned long long)a->names->v[i].tid, a->names->v[i].name);
    size_t old_n = 0;
    uint8_t *old = sd_read(NAMES_FILE, &old_n);
    if (!old || old_n != n || memcmp(old, txt, n) != 0) sd_write(NAMES_FILE, (const uint8_t *)txt, n);
    free(old);
    free(txt);
}

/* Titles in HOME whose icon block could not be read and that have no saved name either. */
static size_t count_hidden(const App *a)
{
    size_t lines = a->cl / CACHE_ENTRY, hidden = 0;
    if (a->dl / SMDH_SIZE < lines) lines = a->dl / SMDH_SIZE;
    for (size_t i = 0; i < lines; i++) {
        uint64_t tid = rd64(a->cache + i * CACHE_ENTRY + 8);
        if (tid == 0 || tid == TID_EMPTY || smdh_valid(a->cached + i * SMDH_SIZE)) continue;
        hidden += !namedb_find(a->names, tid) && is_listed_kind(tid) && layout_has_title(a->launcher, a->save, tid);
    }
    return hidden;
}

static bool prepare_sort(App *a)
{
    memcpy(a->new_l, a->launcher, LAUNCHER_SIZE);
    memcpy(a->new_s, a->save, SAVEDATA_SIZE);
    if (layout_sort(a->new_l, a->ll, a->new_s, a->sl, a->names) != LAYOUT_OK) return false;
    a->nrows = layout_rows(a->new_l, a->new_s, a->launcher, a->save, a->names, a->rows, ROWS_MAX);
    a->changes = a->n_titles = a->n_folders = 0;
    for (size_t i = 0; i < a->nrows; i++) {
        a->changes += a->rows[i].moved;
        if (a->rows[i].is_folder) a->n_folders++; else a->n_titles++;
    }
    a->unnamed = layout_unnamed_count(a->launcher, a->save, a->names);
    char dir[96];
    a->can_restore = backup_dir_latest(dir, sizeof dir);
    return true;
}

static void load_all(App *a)
{
    /* The saves are open only while reading: if HOME resumed while this app held them, it
     * could not open its own and would abort. */
    Result rc = fsio_open(&a->fs, a->region);
    if (R_FAILED(rc)) {
        snprintf(a->msg, sizeof a->msg, tr(S_ERR_OPEN), (unsigned long)rc);
        size_t n = strlen(a->msg);
        n += (size_t)snprintf(a->msg + n, sizeof a->msg - n, " ");
        snprintf(a->msg + n, sizeof a->msg - n, tr(S_ERR_OPEN_HINT), a->region->name);
        a->after_error = ST_ERROR; /* fatal: leaving the screen exits the app */
        a->state = ST_ERROR;
        return;
    }
    struct { FS_Archive ar; const char *path; uint8_t **buf; size_t *len; } files[] = {
        {a->fs.sys, "/Launcher.dat", &a->launcher, &a->ll},
        {a->fs.ext, "/SaveData.dat", &a->save, &a->sl},
        {a->fs.ext, "/Cache.dat", &a->cache, &a->cl},
        {a->fs.ext, "/CacheD.dat", &a->cached, &a->dl},
    };
    const char *failed = NULL;
    for (size_t i = 0; i < sizeof files / sizeof *files && !failed; i++) {
        rc = fsio_read_all(files[i].ar, files[i].path, files[i].buf, files[i].len, i == 3 ? &a->bad_blocks : NULL);
        if (R_FAILED(rc)) failed = files[i].path + 1;
    }
    fsio_close(&a->fs);
    if (failed) {
        snprintf(a->msg, sizeof a->msg, tr(S_ERR_READ), failed, (unsigned long)rc);
        a->after_error = ST_ERROR;
        a->state = ST_ERROR;
        return;
    }
    if (a->ll != LAUNCHER_SIZE || a->sl != SAVEDATA_SIZE) { fail_msg(a, ST_ERROR, tr(S_ERR_FORMAT)); return; }
    a->names = calloc(1, sizeof *a->names);
    a->new_l = malloc(LAUNCHER_SIZE);
    a->new_s = malloc(SAVEDATA_SIZE);
    a->rows = malloc(sizeof(LayoutRow) * ROWS_MAX);
    if (!a->names || !a->new_l || !a->new_s || !a->rows) { fail_msg(a, ST_ERROR, tr(S_ERR_MEMORY)); return; }
    namedb_load_cache(a->names, a->cache, a->cl, a->cached, a->dl);
    merge_saved_names(a);
    save_names(a);
    a->hidden = count_hidden(a);
    if (!prepare_sort(a)) { fail_msg(a, ST_ERROR, tr(S_ERR_FORMAT)); return; }
    build_games(a);
    a->ic_now = ui_icon_new();
    a->ic_after = ui_icon_new();
    a->state = ST_MENU;
}

/* ---- writing ------------------------------------------------------------------------- */

/* The HOME files through the FS service, as a Store. */
static FS_Archive archive_of(const App *a, StoreFile f) { return f == STORE_LAUNCHER ? a->fs.sys : a->fs.ext; }

static const char *path_of(StoreFile f)
{
    return f == STORE_LAUNCHER ? "/Launcher.dat" : f == STORE_SAVEDATA ? "/SaveData.dat" : "/CacheD.dat";
}

static int store_read(void *ctx, StoreFile f, uint64_t off, uint8_t *buf, size_t len)
{
    return (int)fsio_read_at(archive_of(ctx, f), path_of(f), off, buf, len);
}

static int store_write(void *ctx, StoreFile f, uint64_t off, const uint8_t *buf, size_t len)
{
    return (int)fsio_write_at(archive_of(ctx, f), path_of(f), off, buf, len);
}

static int store_commit(void *ctx, StoreFile f) { return (int)fsio_commit(archive_of(ctx, f)); }

static Store store_of(App *a) { return (Store){a, store_read, store_write, store_commit}; }

static bool save_backup(App *a)
{
    char p[160];
    if (!backup_dir_new(a->backup, sizeof a->backup)) return false;
    snprintf(p, sizeof p, "%s/Launcher.dat", a->backup);
    if (!sd_write(p, a->launcher, a->ll)) return false;
    snprintf(p, sizeof p, "%s/SaveData.dat", a->backup);
    return sd_write(p, a->save, a->sl);
}

/* Replace the live layout with (to_l, to_s): backup, write, read back, undo on any failure. */
static bool apply_layout(App *a, const uint8_t *to_l, const uint8_t *to_s)
{
    progress(a, S_WORK_BACKUP);
    if (!save_backup(a)) { fail_msg(a, ST_SORT, tr(S_ERR_BACKUP)); return false; }

    progress(a, S_WORK_WRITE);
    Result orc = fsio_open(&a->fs, a->region);
    if (R_FAILED(orc)) { fail_rc(a, ST_SORT, S_ERR_WRITE, orc); return false; }
    Store st = store_of(a);
    int err = 0;
    StoreStatus res = store_apply_layout(&st, a->launcher, a->save, to_l, to_s, &err);
    fsio_close(&a->fs);
    if (res == STORE_WRITE_FAILED) { fail_rc(a, ST_SORT, S_ERR_WRITE, (Result)err); return false; }
    if (res == STORE_VERIFY_FAILED) { fail_msg(a, ST_SORT, tr(S_ERR_VERIFY)); return false; }

    memcpy(a->launcher, to_l, LAUNCHER_SIZE);
    memcpy(a->save, to_s, SAVEDATA_SIZE);
    lock_home(a);
    return true;
}

static void do_sort(App *a)
{
    if (!apply_layout(a, a->new_l, a->new_s)) return;
    prepare_sort(a);
    snprintf(a->msg, sizeof a->msg, "%s", tr(S_DONE_SORT));
    a->state = ST_DONE;
}

static void do_restore(App *a)
{
    char dir[96], p[160];
    if (!backup_dir_latest(dir, sizeof dir)) { fail_msg(a, ST_SORT, tr(S_ERR_NOBACKUP)); return; }
    size_t nl = 0, ns = 0;
    snprintf(p, sizeof p, "%s/Launcher.dat", dir);
    uint8_t *bl = sd_read(p, &nl);
    snprintf(p, sizeof p, "%s/SaveData.dat", dir);
    uint8_t *bs = sd_read(p, &ns);
    if (!bl || !bs || nl != LAUNCHER_SIZE || ns != SAVEDATA_SIZE) {
        free(bl);
        free(bs);
        fail_msg(a, ST_SORT, tr(S_ERR_BAD_BACKUP));
        return;
    }
    bool ok = apply_layout(a, bl, bs);
    free(bl);
    free(bs);
    if (!ok) return;
    prepare_sort(a);
    snprintf(a->msg, sizeof a->msg, "%s", tr(S_DONE_RESTORE));
    a->state = ST_DONE;
}

/* ---- green check --------------------------------------------------------------------- */

/* The icon blocks a marked title had before: the saved copy, else the title's own icon. */
static bool original_icons(const Game *g, uint8_t *out)
{
    char p[96];
    size_t n = 0;
    if (g->has_backup && backup_path_for(g->tid, p, sizeof p)) {
        uint8_t *orig = sd_read(p, &n);
        bool ok = orig && n == SMDH_ICONS_LEN;
        if (ok) memcpy(out, orig, n);
        free(orig);
        if (ok) return true;
    }
    return R_SUCCEEDED(fsio_title_icons(g->tid, out));
}

static void refresh_previews(App *a)
{
    if (!a->ngames || !a->ic_now || !a->ic_after) return;
    Game *g = &a->games[a->sel];
    static uint8_t smdh[SMDH_SIZE]; /* big buffers are static: the 3DS main thread stack is small */
    memcpy(smdh, a->cached + g->idx * SMDH_SIZE, SMDH_SIZE);
    ui_icon_set(a->ic_now, smdh + SMDH_ICON_LARGE, 48);
    snprintf(a->after_label, sizeof a->after_label, "%s", tr(S_MARK_AFTER));
    a->have_original = true;
    if (!g->marked) {
        icon_mark(smdh);
    } else {
        static uint8_t orig[SMDH_ICONS_LEN];
        a->have_original = original_icons(g, orig);
        if (a->have_original) memcpy(smdh + SMDH_ICON_SMALL, orig, SMDH_ICONS_LEN);
        snprintf(a->after_label, sizeof a->after_label, "%s", tr(S_MARK_ORIGINAL));
    }
    ui_icon_set(a->ic_after, smdh + SMDH_ICON_LARGE, 48);
}

static void toggle_mark(App *a)
{
    if (!a->ngames) return;
    Game *g = &a->games[a->sel];
    uint8_t *live = a->cached + g->idx * SMDH_SIZE;
    static uint8_t fresh[SMDH_ICONS_LEN];
    char p[96];
    backup_path_for(g->tid, p, sizeof p);

    if (!g->marked) {
        if (!sd_exists(p) && !sd_write(p, live + SMDH_ICON_SMALL, SMDH_ICONS_LEN)) { fail_msg(a, ST_MARK, tr(S_ERR_BACKUP)); return; }
        static uint8_t smdh[SMDH_SIZE];
        memcpy(smdh, live, SMDH_SIZE);
        icon_mark(smdh);
        memcpy(fresh, smdh + SMDH_ICON_SMALL, SMDH_ICONS_LEN);
    } else if (!original_icons(g, fresh)) {
        fail_msg(a, ST_MARK, tr(S_MARK_NO_ORIGINAL));
        return;
    }

    Result orc = fsio_open(&a->fs, a->region);
    if (R_FAILED(orc)) { fail_rc(a, ST_MARK, S_ERR_WRITE, orc); return; }
    Store st = store_of(a);
    int err = 0;
    StoreStatus res = store_set_icons(&st, g->idx, live + SMDH_ICON_SMALL, fresh, &err);
    fsio_close(&a->fs);
    if (res == STORE_WRITE_FAILED) { fail_rc(a, ST_MARK, S_ERR_WRITE, (Result)err); return; }
    if (res == STORE_VERIFY_FAILED) { fail_msg(a, ST_MARK, tr(S_ERR_VERIFY)); return; }
    memcpy(live + SMDH_ICON_SMALL, fresh, SMDH_ICONS_LEN);
    refresh_game(g, a);
    refresh_previews(a);
    lock_home(a);
}

static void select_game(App *a, int sel)
{
    if (!a->ngames) return;
    if (sel < 0) sel = 0;
    if (sel >= (int)a->ngames) sel = (int)a->ngames - 1;
    a->sel = sel;
    if (sel < a->mscroll) a->mscroll = sel;
    if (sel >= a->mscroll + MARK_PAGE) a->mscroll = sel - MARK_PAGE + 1;
    refresh_previews(a);
}

/* ---- update -------------------------------------------------------------------------- */

static void clamp_scroll(App *a)
{
    int max = (int)a->nrows - SORT_PAGE;
    if (a->scroll > max) a->scroll = max;
    if (a->scroll < 0) a->scroll = 0;
}

static void start_reboot(App *a)
{
    Result rc = ptmSysmInit();
    if (R_SUCCEEDED(rc)) rc = PTMSYSM_RebootAsync(0);
    a->reboot_rc = (u32)rc;
    a->reboot_at = osGetTime();
    a->state = ST_REBOOT;
}

static void update(App *a, u32 down, const touchPosition *t)
{
    bool tap = down & KEY_TOUCH;
    switch (a->state) {
    case ST_LOADING:
        if (++a->tick == 3) load_all(a);
        break;
    case ST_MENU:
        if (down & KEY_START) a->quit = true;
        if (down & KEY_DUP) a->menu_focus = 0;
        if (down & KEY_DDOWN) a->menu_focus = 1;
        if (tap && hit(t, 20, 40, 280, 72)) { a->menu_focus = 0; down |= KEY_A; }
        else if (tap && hit(t, 20, 128, 280, 72)) { a->menu_focus = 1; down |= KEY_A; }
        if (down & KEY_A) {
            if (a->menu_focus == 0) { a->scroll = 0; a->state = ST_SORT; }
            else { select_game(a, a->sel); a->state = ST_MARK; }
        }
        break;
    case ST_SORT:
        if (down & KEY_DUP) a->scroll--;
        if (down & KEY_DDOWN) a->scroll++;
        if (down & KEY_L) a->scroll -= SORT_PAGE;
        if (down & KEY_R) a->scroll += SORT_PAGE;
        clamp_scroll(a);
        if ((down & KEY_B) || (tap && hit(t, 20, 160, 280, 40))) a->state = ST_MENU;
        if (a->changes && !a->hidden && ((down & KEY_A) || (tap && hit(t, 20, 104, 280, 48)))) a->state = ST_SORT_CONFIRM;
        if (a->can_restore && ((down & KEY_Y) || (tap && hit(t, 20, 206, 280, 28)))) a->state = ST_RESTORE_CONFIRM;
        break;
    case ST_SORT_CONFIRM:
        if ((down & KEY_A) || (tap && hit(t, 20, 120, 130, 56))) do_sort(a);
        else if ((down & KEY_B) || (tap && hit(t, 170, 120, 130, 56))) a->state = ST_SORT;
        break;
    case ST_RESTORE_CONFIRM:
        if ((down & KEY_A) || (tap && hit(t, 20, 120, 130, 56))) do_restore(a);
        else if ((down & KEY_B) || (tap && hit(t, 170, 120, 130, 56))) a->state = ST_SORT;
        break;
    case ST_MARK:
        if (down & KEY_B) {
            a->state = a->dirty ? ST_DONE : ST_MENU;
            snprintf(a->msg, sizeof a->msg, "%s", tr(S_DONE_MARK));
            a->backup[0] = '\0';
            break;
        }
        if (down & KEY_DUP) select_game(a, a->sel - 1);
        if (down & KEY_DDOWN) select_game(a, a->sel + 1);
        if (down & KEY_L) select_game(a, a->sel - MARK_PAGE);
        if (down & KEY_R) select_game(a, a->sel + MARK_PAGE);
        if (tap) {
            for (int i = 0; i < MARK_PAGE; i++) {
                int idx = a->mscroll + i;
                if (idx < (int)a->ngames && hit(t, 8, 8 + i * 34, 288, 32)) {
                    if (idx == a->sel) down |= KEY_A; else select_game(a, idx);
                }
            }
        }
        if (down & KEY_X) {
            uint64_t keep = a->ngames ? a->games[a->sel].tid : 0;
            a->show_all = !a->show_all;
            build_games(a);
            a->sel = a->mscroll = 0;
            for (size_t i = 0; i < a->ngames; i++)
                if (a->games[i].tid == keep) a->sel = (int)i;
            select_game(a, a->sel);
            a->mscroll = a->sel > MARK_PAGE / 2 ? a->sel - MARK_PAGE / 2 : 0;
        }
        if (down & KEY_A) toggle_mark(a);
        break;
    case ST_DONE:
        if (down & KEY_A) start_reboot(a);
        if (down & KEY_B) { aptSetHomeAllowed(true); a->quit = true; }
        break;
    case ST_ERROR:
        if (down & (KEY_A | KEY_B)) {
            if (a->after_error == ST_ERROR) a->quit = true; else a->state = a->after_error;
        }
        break;
    case ST_REBOOT:
        if (osGetTime() - a->reboot_at > 6000) { /* still here: the reboot was refused */
            snprintf(a->msg, sizeof a->msg, tr(S_REBOOT_FAIL), (unsigned long)a->reboot_rc);
            a->after_error = ST_DONE;
            a->state = ST_ERROR;
        }
        break;
    case ST_WORK:
        break;
    }
}

/* ---- drawing ------------------------------------------------------------------------- */

static void draw_menu(App *a)
{
    char s[128];
    ui_top();
    ui_header("HomeTidy", "v" VERSION);
    ui_card(20, 48, 360, 128);
    ui_text(36, 58, 0.55f, COL_FG, tr(S_TAGLINE), 330);
    snprintf(s, sizeof s, tr(S_INFO_HOME), (int)a->n_titles, (int)a->n_folders);
    ui_text(36, 92, 0.5f, COL_FG_MUTED, s, 330);
    snprintf(s, sizeof s, tr(S_INFO_NAMES), (int)a->names->n);
    ui_text(36, 114, 0.5f, COL_FG_MUTED, s, 330);
    if (a->unnamed) {
        snprintf(s, sizeof s, tr(S_INFO_UNNAMED), (int)a->unnamed);
        ui_text(36, 136, 0.5f, COL_WARN, s, 330);
    }
    snprintf(s, sizeof s, "Luma3DS  |  HOME %s", a->region->name);
    ui_text_center(TOP_W / 2, 196, 0.45f, COL_FG_MUTED, s);

    ui_bottom();
    const char *label[2] = {tr(S_MENU_SORT), tr(S_MENU_MARK)};
    const char *sub[2] = {tr(S_MENU_SORT_SUB), tr(S_MENU_MARK_SUB)};
    for (int i = 0; i < 2; i++) {
        float y = 40 + i * 88;
        bool f = a->menu_focus == i;
        if (f) ui_rrect(17, y - 3, 286, 78, 11, COL_ACCENT);
        ui_card(20, y, 280, 72);
        ui_text(36, y + 12, 0.65f, COL_FG, label[i], 250);
        ui_text(36, y + 40, 0.45f, COL_FG_MUTED, sub[i], 250);
    }
    ui_text_center(BOT_W / 2, 214, 0.42f, COL_FG_MUTED, tr(S_HINT_MENU));
}

static void draw_sort(App *a)
{
    char s[96];
    ui_top();
    snprintf(s, sizeof s, tr(S_SORT_CHANGES), (int)a->changes);
    ui_header(tr(S_SORT_TITLE), s);
    for (int i = 0; i < SORT_PAGE; i++) {
        size_t k = (size_t)a->scroll + (size_t)i;
        if (k >= a->nrows) break;
        const LayoutRow *r = &a->rows[k];
        float y = 36 + i * 20;
        if (r->is_folder) C2D_DrawRectSolid(8, y, 0.3f, TOP_W - 24, 19, COL_MUTED);
        if (r->moved) C2D_DrawCircleSolid(16, y + 10, 0.3f, 3, COL_WARN);
        float x = r->nested ? 40 : 28;
        ui_text(x, y + 1, 0.5f, r->is_folder ? COL_ACCENT : COL_FG, r->label, TOP_W - x - 20);
    }
    if (a->nrows > SORT_PAGE) { /* scroll bar */
        float track = 200, h = track * SORT_PAGE / a->nrows, y = 36 + (track - h) * a->scroll / (float)(a->nrows - SORT_PAGE);
        C2D_DrawRectSolid(TOP_W - 8, 36, 0.3f, 3, track, COL_MUTED);
        C2D_DrawRectSolid(TOP_W - 8, y, 0.35f, 3, h, COL_BORDER);
    }

    ui_bottom();
    ui_card(20, 12, 280, 80);
    if (a->hidden) {
        snprintf(s, sizeof s, tr(S_SORT_BLOCKED), (int)a->hidden);
        ui_text_wrap(32, 24, 256, 0.5f, COL_WARN, s);
    } else if (a->changes) {
        snprintf(s, sizeof s, tr(S_SORT_CHANGES), (int)a->changes);
        ui_text_center(BOT_W / 2, 24, 0.7f, COL_FG, s);
        ui_text_center(BOT_W / 2, 58, 0.42f, COL_FG_MUTED, tr(S_SORT_NOTE));
    } else {
        ui_text_center(BOT_W / 2, 36, 0.7f, COL_ACCENT, tr(S_SORT_SORTED));
    }
    char b[64];
    snprintf(b, sizeof b, "A  %s", tr(S_BTN_APPLY));
    if (a->changes && !a->hidden) ui_button(20, 104, 280, 48, b, true, true);
    snprintf(b, sizeof b, "B  %s", tr(S_BTN_BACK));
    ui_button(20, 160, 280, 40, b, !a->changes || a->hidden, false);
    if (a->can_restore) {
        snprintf(b, sizeof b, "Y  %s", tr(S_BTN_RESTORE));
        ui_text_center(BOT_W / 2, 212, 0.5f, COL_FG_MUTED, b);
    }
}

static void draw_confirm(App *a)
{
    ui_top();
    ui_header(tr(S_SORT_TITLE), NULL);
    ui_text_center(TOP_W / 2, 100, 0.7f, COL_FG, a->state == ST_RESTORE_CONFIRM ? tr(S_CONFIRM_RESTORE) : tr(S_CONFIRM_SORT));
    ui_text_center(TOP_W / 2, 140, 0.45f, COL_FG_MUTED, tr(S_SORT_NOTE));
    ui_bottom();
    char b[48];
    snprintf(b, sizeof b, "A  %s", tr(S_BTN_YES));
    ui_button(20, 120, 130, 56, b, true, true);
    snprintf(b, sizeof b, "B  %s", tr(S_BTN_NO));
    ui_button(170, 120, 130, 56, b, false, false);
}

static void draw_mark(App *a)
{
    char s[96];
    size_t marked = 0;
    for (size_t i = 0; i < a->ngames; i++) marked += a->games[i].marked;
    snprintf(s, sizeof s, "%d / %d %s", (int)marked, (int)a->ngames, a->show_all ? tr(S_MARK_ALL) : tr(S_MARK_GAMES));
    ui_top();
    ui_header(tr(S_MARK_TITLE), s);
    if (!a->ngames) {
        ui_text_center(TOP_W / 2, 110, 0.55f, COL_FG_MUTED, tr(S_MARK_EMPTY));
    } else {
        const Game *g = &a->games[a->sel];
        ui_card(36, 44, 150, 138);
        ui_card(214, 44, 150, 138);
        ui_text_center(111, 52, 0.45f, COL_FG_MUTED, tr(S_MARK_BEFORE));
        ui_text_center(289, 52, 0.45f, COL_FG_MUTED, a->after_label);
        ui_icon_draw(a->ic_now, 63, 78, 2.0f);
        if (a->have_original) ui_icon_draw(a->ic_after, 241, 78, 2.0f);
        ui_text_center(TOP_W / 2, 192, 0.55f, COL_FG, g->nm->name);
        ui_text_center(TOP_W / 2, 214, 0.45f, g->marked ? COL_ACCENT : COL_FG_MUTED, g->marked ? tr(S_MARK_ON) : tr(S_MARK_OFF));
    }

    ui_bottom();
    for (int i = 0; i < MARK_PAGE; i++) {
        int idx = a->mscroll + i;
        if (idx >= (int)a->ngames) break;
        const Game *g = &a->games[idx];
        float y = 8 + i * 34;
        if (idx == a->sel) ui_rrect(5, y - 3, 294, 38, 9, COL_ACCENT);
        ui_rrect(8, y, 288, 32, 7, idx == a->sel ? COL_MUTED : COL_CARD);
        ui_text(18, y + 5, 0.5f, COL_FG, g->nm->name, g->marked ? 236 : 266);
        if (g->marked) ui_check_badge(276, y + 16, 11, COL_ACCENT);
    }
    if (a->ngames > MARK_PAGE) {
        float track = 204, h = track * MARK_PAGE / a->ngames, y = 8 + (track - h) * a->mscroll / (float)(a->ngames - MARK_PAGE);
        C2D_DrawRectSolid(308, 8, 0.3f, 3, track, COL_MUTED);
        C2D_DrawRectSolid(308, y, 0.35f, 3, h, COL_BORDER);
    }
    ui_text_center(BOT_W / 2, 218, 0.38f, COL_FG_MUTED, tr(S_MARK_HINT));
}

static void draw_done(App *a)
{
    char s[160];
    ui_top();
    ui_header("HomeTidy", NULL);
    ui_check_badge(TOP_W / 2, 84, 34, COL_ACCENT);
    ui_text_center(TOP_W / 2, 132, 0.75f, COL_FG, a->msg);
    if (a->backup[0]) {
        snprintf(s, sizeof s, tr(S_DONE_BACKUP_AT), a->backup + strlen("sdmc:"));
        ui_text(20, 176, 0.42f, COL_FG_MUTED, s, TOP_W - 40);
    }
    ui_bottom();
    float y = ui_text_wrap(20, 14, 280, 0.5f, COL_WARN, tr(S_DONE_NOHOME));
    ui_text_wrap(20, y + 6, 280, 0.5f, COL_FG_MUTED, tr(S_DONE_REBOOT));
    char b[64];
    snprintf(b, sizeof b, "A  %s", tr(S_BTN_REBOOT));
    ui_button(20, 146, 280, 50, b, true, true);
    ui_text_center(BOT_W / 2, 208, 0.45f, COL_FG_MUTED, tr(S_DONE_LEAVE));
}

static void draw_error(App *a)
{
    ui_top();
    ui_header("HomeTidy", NULL);
    C2D_DrawCircleSolid(TOP_W / 2, 70, 0.3f, 28, COL_DANGER);
    ui_text_center(TOP_W / 2, 48, 1.1f, COL_FG, "!");
    ui_text_center(TOP_W / 2, 112, 0.65f, COL_FG, tr(S_ERR_TITLE));
    ui_text_wrap(40, 146, TOP_W - 80, 0.5f, COL_FG_MUTED, a->msg);
    ui_bottom();
    char b[48];
    snprintf(b, sizeof b, "A  %s", tr(S_BTN_BACK));
    ui_button(20, 150, 280, 52, b, true, true);
}

static void draw(App *a)
{
    ui_begin();
    switch (a->state) {
    case ST_LOADING:
    case ST_WORK:
        ui_top();
        ui_header("HomeTidy", NULL);
        ui_text_center(TOP_W / 2, 104, 0.7f, COL_FG, a->state == ST_LOADING ? tr(S_LOADING) : a->msg);
        ui_bottom();
        break;
    case ST_MENU: draw_menu(a); break;
    case ST_SORT: draw_sort(a); break;
    case ST_SORT_CONFIRM:
    case ST_RESTORE_CONFIRM: draw_confirm(a); break;
    case ST_MARK: draw_mark(a); break;
    case ST_DONE: draw_done(a); break;
    case ST_ERROR: draw_error(a); break;
    case ST_REBOOT:
        ui_top();
        ui_header("HomeTidy", NULL);
        ui_text_center(TOP_W / 2, 104, 0.7f, COL_FG, tr(S_REBOOTING));
        ui_bottom();
        break;
    }
    ui_end();
}

int main(void)
{
    if (!ui_init()) return 1;
    App *a = calloc(1, sizeof *a);
    if (!a) return 1;
    a->state = ST_LOADING;
    a->region = region_detect();

    while (aptMainLoop() && !a->quit) {
        hidScanInput();
        u32 down = hidKeysDown();
        touchPosition t = {0};
        if (down & KEY_TOUCH) hidTouchRead(&t);
        update(a, down, &t);
        draw(a);
    }

    fsio_close(&a->fs);
    ui_icon_free(a->ic_now);
    ui_icon_free(a->ic_after);
    ui_exit();
    return 0;
}
