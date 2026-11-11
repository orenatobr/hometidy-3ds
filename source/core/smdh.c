#include "smdh.h"

#include <string.h>

#include "sortkey.h"

uint64_t rd64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int16_t rd16s(const uint8_t *p) { return (int16_t)(uint16_t)(p[0] | p[1] << 8); }

void wr16(uint8_t *p, int16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((uint16_t)v >> 8);
}

static size_t put_utf8(uint32_t c, char *tmp)
{
    if (c < 0x80) { tmp[0] = (char)c; return 1; }
    if (c < 0x800) { tmp[0] = (char)(0xC0 | c >> 6); tmp[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) { tmp[0] = (char)(0xE0 | c >> 12); tmp[1] = (char)(0x80 | ((c >> 6) & 0x3F)); tmp[2] = (char)(0x80 | (c & 0x3F)); return 3; }
    tmp[0] = (char)(0xF0 | c >> 18); tmp[1] = (char)(0x80 | ((c >> 12) & 0x3F)); tmp[2] = (char)(0x80 | ((c >> 6) & 0x3F)); tmp[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

/* UTF-16LE (at most max_units code units, stops at NUL) to UTF-8 with runs of whitespace
 * collapsed to one space. Returns the length written. */
static size_t utf16_to_utf8(const uint8_t *src, size_t max_units, char *out, size_t cap)
{
    size_t n = 0;
    int space = 0;
    for (size_t i = 0; i < max_units; i++) {
        uint32_t c = (uint32_t)(src[2 * i] | src[2 * i + 1] << 8);
        if (!c) break;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < max_units) {
            uint32_t lo = (uint32_t)(src[2 * i + 2] | src[2 * i + 3] << 8);
            if (lo < 0xDC00 || lo >= 0xE000) continue;
            c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
            i++;
        } else if (c >= 0xD800 && c < 0xE000) {
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0xA0 || c == 0x3000) {
            space = n > 0;
            continue;
        }
        char tmp[4];
        size_t k = put_utf8(c, tmp);
        if (n + (space ? 1u : 0u) + k + 1 > cap) break;
        if (space) out[n++] = ' ';
        space = 0;
        memcpy(out + n, tmp, k);
        n += k;
    }
    out[n] = '\0';
    return n;
}

bool smdh_valid(const uint8_t *smdh) { return memcmp(smdh, "SMDH", 4) == 0; }

bool smdh_title(const uint8_t *smdh, char *out, size_t cap)
{
    for (int i = 0; i < 17; i++) {
        int lang = i == 0 ? 1 : i - 1; /* English first, then all languages in order */
        if (utf16_to_utf8(smdh + 8 + 0x200 * lang, 0x40, out, cap)) return true;
    }
    return false;
}

const NameEntry *namedb_find(const NameDB *db, uint64_t tid)
{
    for (size_t i = 0; i < db->n; i++)
        if (db->v[i].tid == tid) return &db->v[i];
    return NULL;
}

void namedb_set(NameDB *db, uint64_t tid, const char *name)
{
    NameEntry *e = (NameEntry *)namedb_find(db, tid);
    if (!e) {
        if (db->n >= NAMEDB_MAX) return;
        e = &db->v[db->n++];
        e->tid = tid;
    }
    strncpy(e->name, name, NAME_MAX_BYTES - 1);
    e->name[NAME_MAX_BYTES - 1] = '\0';
    sk_normalize(e->name, e->key, NAME_MAX_BYTES);
}

void namedb_load_cache(NameDB *db, const uint8_t *cache, size_t cache_len, const uint8_t *cached, size_t cached_len)
{
    size_t count = cache_len / CACHE_ENTRY;
    if (cached_len / SMDH_SIZE < count) count = cached_len / SMDH_SIZE;
    for (size_t i = 0; i < count; i++) {
        uint64_t tid = rd64(cache + i * CACHE_ENTRY + 8);
        const uint8_t *smdh = cached + i * SMDH_SIZE;
        char title[NAME_MAX_BYTES];
        if (tid == 0 || tid == TID_EMPTY || !smdh_valid(smdh) || !smdh_title(smdh, title, sizeof title)) continue;
        namedb_set(db, tid, title);
    }
}
