#include "layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sortkey.h"

#define L_CARD 0x2
#define L_TITLES 0x8
#define L_POS 0xD9A
#define L_FOLDER_IDX 0x106A
#define L_FOLDER_POS 0x11DC
#define L_FOLDER_NAMES 0x1560
#define S_TITLES 0x8
#define S_POS 0xCB0
#define S_FOLDER_IDX 0xF80

#define SYSTEM_APPS 0x00040010u /* system apps */
#define DS_LAUNCHER 0x00048004u

typedef struct {
    uint8_t *buf;
    size_t poff;
    uint64_t tid;
    int pos;
    int folder;
    const NameEntry *nm;
} Entry;

typedef struct {
    int pos;
    int idx;
    char name[NAME_MAX_BYTES];
    char key[NAME_MAX_BYTES];
} Folder;

typedef int (*Cmp)(const void *a, const void *b);

/* Stable insertion sort over an array of pointers (n is a few hundred at most). */
static void sort_ptrs(const void **v, size_t n, Cmp cmp)
{
    for (size_t i = 1; i < n; i++) {
        const void *x = v[i];
        size_t j = i;
        while (j > 0 && cmp(v[j - 1], x) > 0) {
            v[j] = v[j - 1];
            j--;
        }
        v[j] = x;
    }
}

static int cmp_pos(const void *a, const void *b) { return ((const Entry *)a)->pos - ((const Entry *)b)->pos; }

static int cmp_tid(const void *a, const void *b)
{
    uint64_t x = ((const Entry *)a)->tid, y = ((const Entry *)b)->tid;
    return (x > y) - (x < y);
}

static int cmp_name(const void *a, const void *b)
{
    const NameEntry *x = ((const Entry *)a)->nm, *y = ((const Entry *)b)->nm;
    return sk_order(x->name, x->key, y->name, y->key);
}

static int cmp_folder(const void *a, const void *b)
{
    const Folder *x = a, *y = b;
    return sk_order(x->name, x->key, y->name, y->key);
}

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

/* UTF-16LE folder name to UTF-8 (BMP only, as HOME limits names to 16 characters). */
static void folder_name(const uint8_t *launcher, int idx, char *out, size_t cap)
{
    const uint8_t *p = launcher + L_FOLDER_NAMES + FOLDER_NAME_BYTES * (size_t)idx;
    size_t n = 0;
    for (size_t i = 0; i < FOLDER_NAME_BYTES / 2; i++) {
        unsigned c = (unsigned)(p[2 * i] | p[2 * i + 1] << 8);
        if (!c) break;
        if (c < 0x80) { if (n + 1 < cap) out[n++] = (char)c; }
        else if (c < 0x800) { if (n + 2 < cap) { out[n++] = (char)(0xC0 | c >> 6); out[n++] = (char)(0x80 | (c & 0x3F)); } }
        else if (n + 3 < cap) { out[n++] = (char)(0xE0 | c >> 12); out[n++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[n++] = (char)(0x80 | (c & 0x3F)); }
    }
    out[n] = '\0';
}

static size_t collect(uint8_t *lau, uint8_t *sav, const NameDB *names, Entry *out)
{
    size_t n = 0;
    int card = lau[L_CARD] | lau[L_CARD + 1] << 8;
    struct { uint8_t *buf; size_t tid, pos, fol; } files[2] = {
        {lau, L_TITLES, L_POS, L_FOLDER_IDX}, {sav, S_TITLES, S_POS, S_FOLDER_IDX}};
    for (int f = 0; f < 2; f++) {
        for (int i = 0; i < LAYOUT_SLOTS; i++) {
            if (f == 0 && i == card) continue;
            uint64_t tid = rd64(files[f].buf + files[f].tid + 8 * (size_t)i);
            int pos = rd16s(files[f].buf + files[f].pos + 2 * (size_t)i);
            int folder = (int8_t)files[f].buf[files[f].fol + (size_t)i];
            if (tid == TID_EMPTY || pos < 0) continue;
            out[n++] = (Entry){files[f].buf, files[f].pos + 2 * (size_t)i, tid, pos, folder, namedb_find(names, tid)};
        }
    }
    return n;
}

/* Every top level position is used once and every folder is packed from 0. */
static bool consistent(uint8_t *lau, uint8_t *sav, const NameDB *names)
{
    Entry e[2 * LAYOUT_SLOTS];
    size_t n = collect(lau, sav, names, e);
    int top[2 * LAYOUT_SLOTS + LAYOUT_FOLDERS], nt = 0;
    for (size_t i = 0; i < n; i++)
        if (e[i].folder < 0) top[nt++] = e[i].pos;
    for (int i = 0; i < LAYOUT_FOLDERS; i++) {
        char name[NAME_MAX_BYTES];
        int pos = rd16s(lau + L_FOLDER_POS + 2 * i);
        folder_name(lau, i, name, sizeof name);
        if (pos >= 0 && name[0]) top[nt++] = pos;
    }
    qsort(top, (size_t)nt, sizeof(int), cmp_int);
    for (int i = 1; i < nt; i++)
        if (top[i] == top[i - 1]) return false;
    for (int f = 0; f < LAYOUT_FOLDERS; f++) {
        int in[2 * LAYOUT_SLOTS], ni = 0;
        for (size_t i = 0; i < n; i++)
            if (e[i].folder == f) in[ni++] = e[i].pos;
        qsort(in, (size_t)ni, sizeof(int), cmp_int);
        for (int i = 0; i < ni; i++)
            if (in[i] != i) return false;
    }
    return true;
}

LayoutStatus layout_sort(uint8_t *launcher, size_t launcher_len, uint8_t *save, size_t save_len, const NameDB *names)
{
    if (launcher_len != LAUNCHER_SIZE || save_len != SAVEDATA_SIZE) return LAYOUT_BAD_SIZE;

    uint8_t *lau = malloc(LAUNCHER_SIZE), *sav = malloc(SAVEDATA_SIZE);
    Entry *e = malloc(sizeof(Entry) * 2 * LAYOUT_SLOTS);
    const void **v = malloc(sizeof(void *) * 3 * 2 * LAYOUT_SLOTS);
    if (!lau || !sav || !e || !v) { free(lau); free(sav); free(e); free(v); return LAYOUT_INVALID; }
    memcpy(lau, launcher, LAUNCHER_SIZE); /* work on copies: the caller's buffers change only on success */
    memcpy(sav, save, SAVEDATA_SIZE);

    size_t n = collect(lau, sav, names, e);

    for (int f = 0; f < LAYOUT_FOLDERS; f++) { /* titles inside each folder, packed from slot 0 */
        size_t nu = 0, nn = 0;
        const void **un = v, **nm = v + n;
        for (size_t i = 0; i < n; i++) {
            if (e[i].folder != f) continue;
            if (e[i].nm) nm[nn++] = &e[i]; else un[nu++] = &e[i];
        }
        sort_ptrs(un, nu, cmp_pos);
        sort_ptrs(nm, nn, cmp_name);
        int slot = 0;
        for (size_t i = 0; i < nu; i++) wr16(((const Entry *)un[i])->buf + ((const Entry *)un[i])->poff, (int16_t)slot++);
        for (size_t i = 0; i < nn; i++) wr16(((const Entry *)nm[i])->buf + ((const Entry *)nm[i])->poff, (int16_t)slot++);
    }

    Folder folders[LAYOUT_FOLDERS];
    int nf = 0;
    for (int i = 0; i < LAYOUT_FOLDERS; i++) {
        Folder *fo = &folders[nf];
        fo->pos = rd16s(lau + L_FOLDER_POS + 2 * i);
        folder_name(lau, i, fo->name, sizeof fo->name);
        if (fo->pos < 0 || !fo->name[0]) continue;
        fo->idx = i;
        sk_normalize(fo->name, fo->key, sizeof fo->key);
        nf++;
    }

    /* Top level: system apps by title id, unnamed by title id, named alphabetically, then
     * folders alphabetically, reusing exactly the positions already in use. */
    const void **s_ = v, **u_ = v + n, **n_ = v + 2 * n;
    size_t ns = 0, nu = 0, nn = 0;
    int slots[2 * LAYOUT_SLOTS + LAYOUT_FOLDERS];
    int nslots = 0;
    for (size_t i = 0; i < n; i++) {
        if (e[i].folder >= 0) continue;
        slots[nslots++] = e[i].pos;
        uint32_t hi = (uint32_t)(e[i].tid >> 32);
        if (hi == SYSTEM_APPS || hi == DS_LAUNCHER) s_[ns++] = &e[i];
        else if (!e[i].nm) u_[nu++] = &e[i];
        else n_[nn++] = &e[i];
    }
    for (int i = 0; i < nf; i++) slots[nslots++] = folders[i].pos;
    qsort(slots, (size_t)nslots, sizeof(int), cmp_int);
    sort_ptrs(s_, ns, cmp_tid);
    sort_ptrs(u_, nu, cmp_tid);
    sort_ptrs(n_, nn, cmp_name);
    int k = 0;
    const void **groups[3] = {s_, u_, n_};
    size_t counts[3] = {ns, nu, nn};
    for (int g = 0; g < 3; g++)
        for (size_t i = 0; i < counts[g]; i++, k++) {
            const Entry *x = groups[g][i];
            wr16(x->buf + x->poff, (int16_t)slots[k]);
        }
    const void **fp = malloc(sizeof(void *) * (size_t)(nf ? nf : 1));
    if (!fp) { free(lau); free(sav); free(e); free(v); return LAYOUT_INVALID; }
    for (int i = 0; i < nf; i++) fp[i] = &folders[i];
    sort_ptrs(fp, (size_t)nf, cmp_folder);
    for (int i = 0; i < nf; i++, k++) wr16(lau + L_FOLDER_POS + 2 * ((const Folder *)fp[i])->idx, (int16_t)slots[k]);
    free(fp);

    LayoutStatus st = consistent(lau, sav, names) ? LAYOUT_OK : LAYOUT_INVALID;
    if (st == LAYOUT_OK) {
        memcpy(launcher, lau, LAUNCHER_SIZE);
        memcpy(save, sav, SAVEDATA_SIZE);
    }
    free(lau); free(sav); free(e); free(v);
    return st;
}

bool layout_diff_span(const uint8_t *a, const uint8_t *b, size_t len, size_t *first, size_t *last)
{
    size_t lo = len, hi = 0;
    for (size_t i = 0; i < len; i++) {
        if (a[i] == b[i]) continue;
        if (i < lo) lo = i;
        hi = i;
    }
    if (lo == len) return false;
    *first = lo;
    *last = hi;
    return true;
}

size_t layout_diff_bytes(const uint8_t *a, const uint8_t *b, size_t len)
{
    size_t n = 0;
    for (size_t i = 0; i < len; i++) n += a[i] != b[i];
    return n;
}

static void label_for(const NameDB *names, uint64_t tid, char *out, size_t cap)
{
    const NameEntry *nm = namedb_find(names, tid);
    if (nm) snprintf(out, cap, "%s", nm->name);
    else snprintf(out, cap, "%08X%08X", (unsigned)(tid >> 32), (unsigned)tid);
}

typedef struct { int pos; int folder; uint64_t tid; bool moved; } Item;

static int cmp_item(const void *a, const void *b) { return ((const Item *)a)->pos - ((const Item *)b)->pos; }

size_t layout_rows(const uint8_t *al, const uint8_t *as, const uint8_t *bl, const uint8_t *bs, const NameDB *names,
                   LayoutRow *rows, size_t cap)
{
    Item items[2 * LAYOUT_SLOTS];
    size_t n = 0;
    int card = al[L_CARD] | al[L_CARD + 1] << 8;
    struct { const uint8_t *a, *b; size_t tid, pos, fol; } f[2] = {
        {al, bl, L_TITLES, L_POS, L_FOLDER_IDX}, {as, bs, S_TITLES, S_POS, S_FOLDER_IDX}};
    for (int x = 0; x < 2; x++)
        for (int i = 0; i < LAYOUT_SLOTS; i++) {
            if (x == 0 && i == card) continue;
            uint64_t tid = rd64(f[x].a + f[x].tid + 8 * (size_t)i);
            int pos = rd16s(f[x].a + f[x].pos + 2 * (size_t)i);
            if (tid == TID_EMPTY || pos < 0) continue;
            int old = rd16s(f[x].b + f[x].pos + 2 * (size_t)i);
            items[n++] = (Item){pos, (int8_t)f[x].a[f[x].fol + (size_t)i], tid, old != pos};
        }
    Item *sorted = malloc(sizeof(Item) * (n ? n : 1));
    if (!sorted) return 0;
    memcpy(sorted, items, sizeof(Item) * n);
    for (size_t i = 1; i < n; i++) { /* stable by position */
        Item x = sorted[i];
        size_t j = i;
        while (j > 0 && cmp_item(&sorted[j - 1], &x) > 0) { sorted[j] = sorted[j - 1]; j--; }
        sorted[j] = x;
    }

    typedef struct { int pos, idx; bool moved; } Fo;
    Fo fo[LAYOUT_FOLDERS];
    int nf = 0;
    for (int i = 0; i < LAYOUT_FOLDERS; i++) {
        char name[NAME_MAX_BYTES];
        int pos = rd16s(al + L_FOLDER_POS + 2 * i);
        folder_name(al, i, name, sizeof name);
        if (pos < 0 || !name[0]) continue;
        fo[nf++] = (Fo){pos, i, rd16s(bl + L_FOLDER_POS + 2 * i) != pos};
    }
    for (int i = 1; i < nf; i++) {
        Fo x = fo[i];
        int j = i;
        while (j > 0 && fo[j - 1].pos > x.pos) { fo[j] = fo[j - 1]; j--; }
        fo[j] = x;
    }

    size_t w = 0;
    for (size_t i = 0; i < n && w < cap; i++) {
        if (sorted[i].folder >= 0) continue;
        rows[w] = (LayoutRow){.moved = sorted[i].moved};
        label_for(names, sorted[i].tid, rows[w].label, sizeof rows[w].label);
        w++;
    }
    for (int k = 0; k < nf && w < cap; k++) {
        char name[NAME_MAX_BYTES];
        folder_name(al, fo[k].idx, name, sizeof name);
        rows[w] = (LayoutRow){.is_folder = true, .moved = fo[k].moved};
        snprintf(rows[w].label, sizeof rows[w].label, "%s", name);
        w++;
        for (size_t i = 0; i < n && w < cap; i++) {
            if (sorted[i].folder != fo[k].idx) continue;
            rows[w] = (LayoutRow){.nested = true, .moved = sorted[i].moved};
            label_for(names, sorted[i].tid, rows[w].label, sizeof rows[w].label);
            w++;
        }
    }
    free(sorted);
    return w;
}

size_t layout_unnamed_count(const uint8_t *launcher, const uint8_t *save, const NameDB *names)
{
    size_t c = 0;
    int card = launcher[L_CARD] | launcher[L_CARD + 1] << 8;
    for (int x = 0; x < 2; x++) {
        const uint8_t *b = x ? save : launcher;
        for (int i = 0; i < LAYOUT_SLOTS; i++) {
            if (x == 0 && i == card) continue;
            uint64_t tid = rd64(b + (x ? S_TITLES : L_TITLES) + 8 * (size_t)i);
            int pos = rd16s(b + (x ? S_POS : L_POS) + 2 * (size_t)i);
            uint32_t hi = (uint32_t)(tid >> 32);
            if (tid != TID_EMPTY && pos >= 0 && hi != SYSTEM_APPS && hi != DS_LAUNCHER && !namedb_find(names, tid)) c++;
        }
    }
    return c;
}

bool layout_has_title(const uint8_t *launcher, const uint8_t *save, uint64_t tid)
{
    for (int x = 0; x < 2; x++) {
        const uint8_t *b = x ? save : launcher;
        for (int i = 0; i < LAYOUT_SLOTS; i++)
            if (rd64(b + (x ? S_TITLES : L_TITLES) + 8 * (size_t)i) == tid && rd16s(b + (x ? S_POS : L_POS) + 2 * (size_t)i) >= 0) return true;
    }
    return false;
}
