/** @file fsio.h
 *  Access to the HOME Menu saves through the FS service, plus backups on the SD card.
 *
 *  Needs the access rights declared in meta/hometidy.rsf, which is why the app is a CIA.
 */
#ifndef HT_FSIO_H
#define HT_FSIO_H

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/smdh.h"
#include "region.h"

typedef struct {
    FS_Archive ext; /* region extdata: SaveData.dat, Cache.dat, CacheD.dat */
    FS_Archive sys; /* system save 0x00020098: Launcher.dat */
    bool ext_open;
    bool sys_open;
} HomeFs;

Result fsio_open(HomeFs *fs, const Region *region);
void fsio_close(HomeFs *fs);

/** Read a whole file into a new malloc buffer (free it).
 *
 *  Reads big chunks and, where a chunk fails (some HOME extdata files have unreadable
 *  blocks), falls back to 4 KB blocks. Blocks that still fail stay zero and are counted in
 *  @p bad; pass NULL to make any unreadable block an error.
 */
Result fsio_read_all(FS_Archive ar, const char *path, uint8_t **buf, size_t *len, size_t *bad);

/** Read @p len bytes at @p off into @p buf. */
Result fsio_read_at(FS_Archive ar, const char *path, uint64_t off, uint8_t *buf, size_t len);

/** Overwrite a range of an existing file (never changes its size). */
Result fsio_write_at(FS_Archive ar, const char *path, uint64_t off, const uint8_t *data, size_t len);

/** Read the icon blocks (SMDH_ICONS_LEN bytes) from the title's own ExeFS icon, which is the
 *  original HOME caches. Tries the SD card, then the NAND. */
Result fsio_title_icons(uint64_t tid, uint8_t *out);

/** Commit pending writes of a save data archive. */
Result fsio_commit(FS_Archive ar);

/** Write @p data to a file on the SD card, creating its folders. */
bool sd_write(const char *path, const uint8_t *data, size_t len);

/** Read a whole SD file (malloc buffer), or NULL. */
uint8_t *sd_read(const char *path, size_t *len);

bool sd_exists(const char *path);

/** Create "sdmc:/3ds/HomeTidy/backup/<stamp>" and return its path in @p out. */
bool backup_dir_new(char *out, size_t cap);

/** Newest backup folder, or false when there is none. */
bool backup_dir_latest(char *out, size_t cap);

#endif
