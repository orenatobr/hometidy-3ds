/** @file layout.h
 *  HOME Menu layout: sorting of Launcher.dat and SaveData.dat.
 *
 *  Offsets follow github.com/mrissaoussama/HomeMenuEditor3DS. Only the position arrays are
 *  rewritten; every other byte is kept as is.
 */
#ifndef HT_LAYOUT_H
#define HT_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smdh.h"

#define LAUNCHER_SIZE 9560 /* system save of the HOME Menu */
#define SAVEDATA_SIZE 11680 /* extdata of the HOME Menu */
#define LAYOUT_SLOTS 360
#define LAYOUT_FOLDERS 60
#define FOLDER_NAME_BYTES 0x22

typedef enum {
    LAYOUT_OK = 0,
    LAYOUT_BAD_SIZE = -1,
    LAYOUT_INVALID = -2, /* the sorted result failed the consistency check */
} LayoutStatus;

/** Sort the layout in place.
 *
 *  System apps come first by title id, then apps with no known name by title id, then named
 *  apps alphabetically, then folders alphabetically. Inside each folder titles are packed from
 *  slot 0 (unnamed by their old position, then named alphabetically). The game card slot never
 *  moves and the set of positions in use at the top level never changes.
 *
 *  @return LAYOUT_OK, or an error with both buffers left untouched.
 */
LayoutStatus layout_sort(uint8_t *launcher, size_t launcher_len, uint8_t *save, size_t save_len, const NameDB *names);

/** Number of position bytes that differ between two layouts of the same file size. */
size_t layout_diff_bytes(const uint8_t *a, const uint8_t *b, size_t len);

/** First and last differing byte; returns false when the buffers are equal. */
bool layout_diff_span(const uint8_t *a, const uint8_t *b, size_t len, size_t *first, size_t *last);

typedef struct {
    char label[NAME_MAX_BYTES + 8];
    bool is_folder; /* folder heading */
    bool nested;    /* title inside a folder */
    bool moved;     /* position differs from @c before */
} LayoutRow;

/** List the layout for display: top level in order, then each folder with its titles.
 *
 *  @param after  Layout to show.
 *  @param before Layout to compare with (marks rows whose position changed), may equal @p after.
 *  @return Number of rows written.
 */
size_t layout_rows(const uint8_t *after_l, const uint8_t *after_s, const uint8_t *before_l, const uint8_t *before_s,
                   const NameDB *names, LayoutRow *rows, size_t cap);

/** Titles in the layout that have no known name. */
size_t layout_unnamed_count(const uint8_t *launcher, const uint8_t *save, const NameDB *names);

/** True when @p tid is placed somewhere in the HOME layout. */
bool layout_has_title(const uint8_t *launcher, const uint8_t *save, uint64_t tid);

#endif
