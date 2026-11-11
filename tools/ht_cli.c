/* Offline runner for the HomeTidy core, to try it on dumps taken from a console.
 *
 *   ht_cli sort Launcher.dat SaveData.dat Cache.dat CacheD.dat OUT_DIR
 *   ht_cli marked Cache.dat CacheD.dat
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "icon.h"
#include "layout.h"
#include "smdh.h"

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n ? (size_t)n : 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { perror(path); exit(2); }
    fclose(f);
    *len = (size_t)n;
    return b;
}

static void spit(const char *dir, const char *name, const uint8_t *b, size_t n)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(b, 1, n, f) != n) { perror(path); exit(2); }
    fclose(f);
}

int main(int argc, char **argv)
{
    NameDB *db = calloc(1, sizeof *db);
    if (argc == 7 && !strcmp(argv[1], "sort")) {
        size_t ll, sl, cl, dl;
        uint8_t *lau = slurp(argv[2], &ll), *sav = slurp(argv[3], &sl), *cache = slurp(argv[4], &cl), *cached = slurp(argv[5], &dl);
        namedb_load_cache(db, cache, cl, cached, dl);
        fprintf(stderr, "%zu titles named\n", db->n);
        LayoutStatus st = layout_sort(lau, ll, sav, sl, db);
        if (st != LAYOUT_OK) { fprintf(stderr, "sort failed: %d\n", st); return 1; }
        spit(argv[6], "Launcher.dat", lau, ll);
        spit(argv[6], "SaveData.dat", sav, sl);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "marked")) {
        size_t cl, dl;
        uint8_t *cache = slurp(argv[2], &cl), *cached = slurp(argv[3], &dl);
        for (size_t i = 0; i < cl / CACHE_ENTRY && (i + 1) * SMDH_SIZE <= dl; i++) {
            const uint8_t *s = cached + i * SMDH_SIZE;
            if (smdh_valid(s) && icon_is_marked(s)) printf("%zu %016llx\n", i, (unsigned long long)rd64(cache + i * CACHE_ENTRY + 8));
        }
        return 0;
    }
    fprintf(stderr, "usage: ht_cli sort L S CACHE CACHED OUT | ht_cli marked CACHE CACHED\n");
    return 2;
}
