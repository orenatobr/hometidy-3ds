/** @file smdh.h
 *  Names stored in the HOME Menu icon cache (Cache.dat and CacheD.dat).
 *
 *  CacheD.dat is a list of SMDH blocks, and line i of Cache.dat (16 bytes, title id in the
 *  last 8) describes block i.
 */
#ifndef HT_SMDH_H
#define HT_SMDH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SMDH_SIZE 0x36C0
#define SMDH_ICON_SMALL 0x2040 /* 24x24 RGB565, 8x8 tiles in Morton order */
#define SMDH_ICON_LARGE 0x24C0 /* 48x48 */
#define SMDH_ICONS_LEN (SMDH_SIZE - SMDH_ICON_SMALL)
#define CACHE_ENTRY 16
#define TID_EMPTY 0xFFFFFFFFFFFFFFFFull

#define NAME_MAX_BYTES 96
#define NAMEDB_MAX 400

typedef struct {
    uint64_t tid;
    char name[NAME_MAX_BYTES];
    char key[NAME_MAX_BYTES];
} NameEntry;

typedef struct {
    NameEntry v[NAMEDB_MAX];
    size_t n;
} NameDB;

uint64_t rd64(const uint8_t *p);
uint32_t rd32(const uint8_t *p);
int16_t rd16s(const uint8_t *p);
void wr16(uint8_t *p, int16_t v);

/** Add or replace the name of a title; the sort key is derived from it. */
void namedb_set(NameDB *db, uint64_t tid, const char *name);
const NameEntry *namedb_find(const NameDB *db, uint64_t tid);

/** True when the block starts with the SMDH magic. */
bool smdh_valid(const uint8_t *smdh);

/** Read the title of an SMDH (English first, then the other languages).
 *  @return false when the SMDH has no title.
 */
bool smdh_title(const uint8_t *smdh, char *out, size_t cap);

/** Read every title of the HOME icon cache into @p db (later entries win). */
void namedb_load_cache(NameDB *db, const uint8_t *cache, size_t cache_len, const uint8_t *cached, size_t cached_len);

#endif
