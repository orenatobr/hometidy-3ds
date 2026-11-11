/** @file store.h
 *  Safe writes to the HOME Menu files: write, read back, and put the original bytes back on
 *  any failure. The storage itself (FS service on the console, memory in tests) is a Store.
 */
#ifndef HT_STORE_H
#define HT_STORE_H

#include <stddef.h>
#include <stdint.h>

typedef enum { STORE_LAUNCHER, STORE_SAVEDATA, STORE_CACHED } StoreFile;

typedef struct {
    void *ctx;
    /** Return 0 on success, any other value is passed back to the caller as the error. */
    int (*read)(void *ctx, StoreFile f, uint64_t off, uint8_t *buf, size_t len);
    int (*write)(void *ctx, StoreFile f, uint64_t off, const uint8_t *buf, size_t len);
    int (*commit)(void *ctx, StoreFile f);
} Store;

typedef enum {
    STORE_OK = 0,
    STORE_WRITE_FAILED,  /* a write or commit failed; originals were put back */
    STORE_VERIFY_FAILED, /* the read back differs; originals were put back */
} StoreStatus;

/** Replace the layout files @p cur_l / @p cur_s with @p to_l / @p to_s.
 *
 *  Writes only the span of bytes that differs, SaveData.dat first. Launcher.dat must commit;
 *  the SaveData.dat commit is best effort (extdata is written through).
 *
 *  @param err Receives the store's error code on STORE_WRITE_FAILED.
 */
StoreStatus store_apply_layout(const Store *s, const uint8_t *cur_l, const uint8_t *cur_s, const uint8_t *to_l,
                               const uint8_t *to_s, int *err);

/** Replace the two icon blocks of one CacheD.dat entry (SMDH_ICONS_LEN bytes). */
StoreStatus store_set_icons(const Store *s, size_t entry, const uint8_t *old_icons, const uint8_t *new_icons, int *err);

#endif
