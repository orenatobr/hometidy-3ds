/* Host tests for the HomeTidy core. Build and run with `make test`. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "icon.h"
#include "layout.h"
#include "smdh.h"
#include "sortkey.h"
#include "store.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

enum { L_TITLES = 0x8, L_POS = 0xD9A, L_FOLDER_IDX = 0x106A, L_FOLDER_POS = 0x11DC, L_FOLDER_NAMES = 0x1560 };
enum { S_TITLES = 0x8, S_POS = 0xCB0, S_FOLDER_IDX = 0xF80 };

static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }

static void blank(uint8_t *lau, uint8_t *sav)
{
    memset(lau, 0, LAUNCHER_SIZE);
    memset(sav, 0, SAVEDATA_SIZE);
    memset(lau + L_TITLES, 0xFF, 8 * LAYOUT_SLOTS);
    memset(lau + L_POS, 0xFF, 2 * LAYOUT_SLOTS);
    memset(lau + L_FOLDER_IDX, 0xFF, LAYOUT_SLOTS);
    memset(lau + L_FOLDER_POS, 0xFF, 2 * LAYOUT_FOLDERS);
    memset(sav + S_TITLES, 0xFF, 8 * LAYOUT_SLOTS);
    memset(sav + S_POS, 0xFF, 2 * LAYOUT_SLOTS);
    memset(sav + S_FOLDER_IDX, 0xFF, LAYOUT_SLOTS);
    lau[2] = 0; /* card in slot 0 */
}

static void set_title(uint8_t *sav, int slot, uint64_t tid, int pos, int folder)
{
    put64(sav + S_TITLES + 8 * slot, tid);
    wr16(sav + S_POS + 2 * slot, (int16_t)pos);
    sav[S_FOLDER_IDX + slot] = (uint8_t)folder;
}

static void set_folder(uint8_t *lau, int idx, const char *name, int pos)
{
    wr16(lau + L_FOLDER_POS + 2 * idx, (int16_t)pos);
    for (int i = 0; name[i]; i++) lau[L_FOLDER_NAMES + FOLDER_NAME_BYTES * idx + 2 * i] = (uint8_t)name[i];
}

static int pos_of(const uint8_t *sav, int slot) { return rd16s(sav + S_POS + 2 * slot); }

static void test_sortkey(void)
{
    char a[96], b[96];
    sk_normalize("Legend of Zelda, The - Minish Cap", a, sizeof a);
    sk_normalize("The Legend of Zelda - Minish Cap", b, sizeof b);
    CHECK(strcmp(a, b) == 0);
    sk_normalize("Final Fantasy IV (Europe) [En,Fr]", a, sizeof a);
    CHECK(strcmp(a, "final fantasy iv") == 0);
    sk_normalize("Pok\xC3\xA9mon: Black 2!", a, sizeof a);
    CHECK(strcmp(a, "pok mon black 2") == 0);
    CHECK(sk_compare("mega man zero", "mega man zero 2") < 0);
    CHECK(sk_compare("mega man zero 2", "mega man zero 10") < 0);
    CHECK(sk_compare("mega man zero 02", "mega man zero 2") == 0);
    CHECK(sk_compare("2 fast", "abc") < 0); /* numbers before text */
    CHECK(sk_order("Game (EU)", "game", "Game (US)", "game") < 0); /* tie: title decides */
}

static void test_smdh_title(void)
{
    static uint8_t smdh[SMDH_SIZE];
    memcpy(smdh, "SMDH", 4);
    const char *jp = "J\0a\0p\0a\0n\0\0";
    memcpy(smdh + 8, jp, 10); /* only the Japanese slot is filled */
    char out[96];
    CHECK(smdh_title(smdh, out, sizeof out) && strcmp(out, "Japan") == 0);
    const char *en = "H\0i\0 \0 \0t\0h\0e\0r\0e\0\0";
    memcpy(smdh + 8 + 0x200, en, 18); /* English wins and spaces collapse */
    CHECK(smdh_title(smdh, out, sizeof out) && strcmp(out, "Hi there") == 0);
}

static void test_layout(void)
{
    static uint8_t lau[LAUNCHER_SIZE], sav[SAVEDATA_SIZE], lau0[LAUNCHER_SIZE], sav0[SAVEDATA_SIZE];
    static NameDB db;
    blank(lau, sav);
    namedb_set(&db, 0x1001, "Zebra Game");
    namedb_set(&db, 0x1002, "Alpha Game");
    namedb_set(&db, 0x1003, "The Beta Game");
    namedb_set(&db, 0x2001, "Zed");
    namedb_set(&db, 0x2002, "Ann");
    set_title(sav, 1, 0x1001, 3, -1); /* loose, out of order */
    set_title(sav, 2, 0x1002, 5, -1);
    set_title(sav, 3, 0x1003, 4, -1);
    set_title(sav, 4, 0x00040010ull << 32 | 0x5, 6, -1); /* system app: stays in front */
    set_title(sav, 5, 0x9999, 7, -1); /* unknown name */
    set_title(sav, 6, 0x2001, 4, 0); /* inside folder 0, with a gap */
    set_title(sav, 7, 0x2002, 9, 0);
    set_folder(lau, 0, "Zeta", 8);
    set_folder(lau, 1, "Alpha", 9);
    set_title(sav, 8, 0x1001 + 100, 0, 1); /* unnamed title in folder "Alpha" */
    namedb_set(&db, 0x1001 + 100, "Inside");
    memcpy(lau0, lau, sizeof lau);
    memcpy(sav0, sav, sizeof sav);

    CHECK(layout_sort(lau, LAUNCHER_SIZE, sav, SAVEDATA_SIZE, &db) == LAYOUT_OK);
    /* used top level positions were 3,4,5,6,7,8,9 and stay the same set */
    CHECK(pos_of(sav, 4) == 3);  /* system app */
    CHECK(pos_of(sav, 5) == 4);  /* unknown */
    CHECK(pos_of(sav, 2) == 5);  /* Alpha Game */
    CHECK(pos_of(sav, 3) == 6);  /* The Beta Game */
    CHECK(pos_of(sav, 1) == 7);  /* Zebra Game */
    CHECK(rd16s(lau + L_FOLDER_POS + 2) == 8); /* Alpha folder */
    CHECK(rd16s(lau + L_FOLDER_POS) == 9);     /* Zeta folder */
    CHECK(pos_of(sav, 7) == 0 && pos_of(sav, 6) == 1); /* Ann, Zed packed from 0 */

    /* only position bytes may change */
    for (size_t i = 0; i < SAVEDATA_SIZE; i++)
        if (sav[i] != sav0[i]) CHECK(i >= S_POS && i < S_POS + 2 * LAYOUT_SLOTS);
    for (size_t i = 0; i < LAUNCHER_SIZE; i++)
        if (lau[i] != lau0[i]) CHECK((i >= L_POS && i < L_POS + 2 * LAYOUT_SLOTS) || (i >= L_FOLDER_POS && i < L_FOLDER_POS + 2 * LAYOUT_FOLDERS));

    /* running twice changes nothing */
    static uint8_t lau1[LAUNCHER_SIZE], sav1[SAVEDATA_SIZE];
    memcpy(lau1, lau, sizeof lau);
    memcpy(sav1, sav, sizeof sav);
    CHECK(layout_sort(lau, LAUNCHER_SIZE, sav, SAVEDATA_SIZE, &db) == LAYOUT_OK);
    CHECK(memcmp(lau, lau1, sizeof lau) == 0 && memcmp(sav, sav1, sizeof sav) == 0);

    static LayoutRow rows[64];
    size_t n = layout_rows(lau, sav, lau0, sav0, &db, rows, 64);
    CHECK(n >= 9 && strcmp(rows[2].label, "Alpha Game") == 0 && !rows[2].moved);
    CHECK(strcmp(rows[3].label, "The Beta Game") == 0 && rows[3].moved); /* 4 -> 6 */
    CHECK(layout_unnamed_count(lau, sav, &db) == 1);
    CHECK(layout_has_title(lau, sav, 0x1002) && !layout_has_title(lau, sav, 0x4242));
    CHECK(layout_folder_of(lau, sav, 0x1002) == -1 && layout_folder_of(lau, sav, 0x2001) == 0 && layout_folder_of(lau, sav, 0x4242) == -1);

    size_t first, last;
    CHECK(layout_diff_span(lau0, lau, LAUNCHER_SIZE, &first, &last) && first <= last);
    CHECK(!layout_diff_span(lau, lau, LAUNCHER_SIZE, &first, &last));
}

static void test_layout_rejects(void)
{
    static uint8_t lau[LAUNCHER_SIZE], sav[SAVEDATA_SIZE];
    static NameDB db;
    blank(lau, sav);
    CHECK(layout_sort(lau, LAUNCHER_SIZE - 1, sav, SAVEDATA_SIZE, &db) == LAYOUT_BAD_SIZE);
    CHECK(layout_sort(lau, LAUNCHER_SIZE, sav, SAVEDATA_SIZE + 1, &db) == LAYOUT_BAD_SIZE);
}

static void test_icon(void)
{
    static uint8_t smdh[SMDH_SIZE], orig[SMDH_SIZE];
    memcpy(smdh, "SMDH", 4);
    for (size_t i = SMDH_ICON_SMALL; i < SMDH_SIZE; i += 2) { smdh[i] = 0x34; smdh[i + 1] = 0x12; } /* flat gray-blue */
    memcpy(orig, smdh, sizeof smdh);
    CHECK(!icon_is_marked(smdh));
    icon_mark(smdh);
    CHECK(icon_is_marked(smdh));
    static uint8_t again[SMDH_SIZE];
    memcpy(again, smdh, sizeof smdh);
    icon_mark(again);
    CHECK(memcmp(again, smdh, sizeof smdh) == 0); /* idempotent */
    CHECK(memcmp(smdh, orig, SMDH_ICON_SMALL) == 0); /* titles and header untouched */
    int changed = 0;
    for (size_t i = SMDH_ICON_LARGE; i < SMDH_SIZE; i++) changed += smdh[i] != orig[i];
    CHECK(changed > 100 && changed < 2 * 18 * 18 + 1); /* only the 18x18 badge box changed */
    uint8_t rgb[3];
    icon_pixel(smdh + SMDH_ICON_LARGE, 48, 0, 0, rgb); /* far corner is not touched */
    CHECK(rgb[0] == ((0x1234 >> 11) << 3 | (0x1234 >> 11) >> 2));
}


/* ---- Store with failure injection -------------------------------------------------- */

typedef struct {
    uint8_t launcher[LAUNCHER_SIZE], save[SAVEDATA_SIZE], cached[SMDH_SIZE * 3];
    int writes;          /* write calls so far */
    int fail_write_at;   /* fail this write call (1 based), 0 = never */
    int fail_commit_launcher;
    int drop_writes_to;  /* silently ignore writes to this file (+1), 0 = none */
} Fake;

static uint8_t *fake_buf(Fake *f, StoreFile k) { return k == STORE_LAUNCHER ? f->launcher : k == STORE_SAVEDATA ? f->save : f->cached; }

static int fake_read(void *c, StoreFile k, uint64_t off, uint8_t *buf, size_t len) { memcpy(buf, fake_buf(c, k) + off, len); return 0; }

static int fake_write(void *c, StoreFile k, uint64_t off, const uint8_t *buf, size_t len)
{
    Fake *f = c;
    if (++f->writes == f->fail_write_at) return 0x77;
    if (f->drop_writes_to == (int)k + 1) return 0;
    memcpy(fake_buf(f, k) + off, buf, len);
    return 0;
}

static int fake_commit(void *c, StoreFile k) { return k == STORE_LAUNCHER && ((Fake *)c)->fail_commit_launcher ? 0x66 : 0; }

static void test_store_layout(void)
{
    static Fake f;
    static uint8_t to_l[LAUNCHER_SIZE], to_s[SAVEDATA_SIZE];
    Store st = {&f, fake_read, fake_write, fake_commit};
    int err = 0;

    for (size_t i = 0; i < LAUNCHER_SIZE; i++) f.launcher[i] = (uint8_t)(i * 7);
    for (size_t i = 0; i < SAVEDATA_SIZE; i++) f.save[i] = (uint8_t)(i * 5);
    memcpy(to_l, f.launcher, sizeof to_l);
    memcpy(to_s, f.save, sizeof to_s);
    to_l[100] ^= 0xFF; to_l[300] ^= 0x0F; /* Launcher changes */
    to_s[50] ^= 0xFF;                      /* SaveData changes */
    static uint8_t orig_l[LAUNCHER_SIZE], orig_s[SAVEDATA_SIZE];
    memcpy(orig_l, f.launcher, sizeof orig_l);
    memcpy(orig_s, f.save, sizeof orig_s);

    /* a clean write lands and is read back */
    CHECK(store_apply_layout(&st, orig_l, orig_s, to_l, to_s, &err) == STORE_OK);
    CHECK(memcmp(f.launcher, to_l, sizeof to_l) == 0 && memcmp(f.save, to_s, sizeof to_s) == 0);
    CHECK(f.writes == 2); /* one span per file, nothing else */

    /* an unchanged layout writes nothing */
    f.writes = 0;
    CHECK(store_apply_layout(&st, to_l, to_s, to_l, to_s, &err) == STORE_OK && f.writes == 0);

    /* Launcher write fails after SaveData was written: SaveData must be put back */
    memcpy(f.launcher, orig_l, sizeof orig_l); memcpy(f.save, orig_s, sizeof orig_s);
    f.writes = 0; f.fail_write_at = 2;
    CHECK(store_apply_layout(&st, orig_l, orig_s, to_l, to_s, &err) == STORE_WRITE_FAILED && err == 0x77);
    CHECK(memcmp(f.launcher, orig_l, sizeof orig_l) == 0 && memcmp(f.save, orig_s, sizeof orig_s) == 0);

    /* Launcher commit fails: both files go back */
    f.writes = 0; f.fail_write_at = 0; f.fail_commit_launcher = 1;
    CHECK(store_apply_layout(&st, orig_l, orig_s, to_l, to_s, &err) == STORE_WRITE_FAILED && err == 0x66);
    CHECK(memcmp(f.launcher, orig_l, sizeof orig_l) == 0 && memcmp(f.save, orig_s, sizeof orig_s) == 0);

    /* the console accepts the write but does not keep it: verify catches it */
    f.fail_commit_launcher = 0; f.drop_writes_to = STORE_SAVEDATA + 1;
    CHECK(store_apply_layout(&st, orig_l, orig_s, to_l, to_s, &err) == STORE_VERIFY_FAILED);
    CHECK(memcmp(f.launcher, orig_l, sizeof orig_l) == 0 && memcmp(f.save, orig_s, sizeof orig_s) == 0);
}

static void test_store_icons(void)
{
    static Fake f;
    Store st = {&f, fake_read, fake_write, fake_commit};
    uint8_t old[SMDH_ICONS_LEN], fresh[SMDH_ICONS_LEN];
    memset(old, 0x11, sizeof old);
    memset(fresh, 0x22, sizeof fresh);
    memset(f.cached, 0xAA, sizeof f.cached);
    memcpy(f.cached + SMDH_SIZE + SMDH_ICON_SMALL, old, sizeof old);
    int err = 0;

    CHECK(store_set_icons(&st, 1, old, fresh, &err) == STORE_OK);
    CHECK(memcmp(f.cached + SMDH_SIZE + SMDH_ICON_SMALL, fresh, sizeof fresh) == 0);
    CHECK(f.cached[SMDH_SIZE] == 0xAA && f.cached[SMDH_SIZE + SMDH_ICON_SMALL - 1] == 0xAA); /* header untouched */
    CHECK(f.cached[2 * SMDH_SIZE] == 0xAA); /* next entry untouched */

    f.writes = 0; f.fail_write_at = 1;
    CHECK(store_set_icons(&st, 1, fresh, old, &err) == STORE_WRITE_FAILED && err == 0x77);

    f.fail_write_at = 0; f.drop_writes_to = STORE_CACHED + 1;
    CHECK(store_set_icons(&st, 1, fresh, old, &err) == STORE_VERIFY_FAILED);
}

int main(void)
{
    test_sortkey();
    test_smdh_title();
    test_layout();
    test_layout_rejects();
    test_icon();
    test_store_layout();
    test_store_icons();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    puts("all tests passed");
    return 0;
}
