#include "store.h"

#include <stdlib.h>
#include <string.h>

#include "layout.h"
#include "smdh.h"

/* Write the bytes that differ between two same-size files; returns 0 or the store error. */
static int write_diff(const Store *s, StoreFile f, const uint8_t *from, const uint8_t *to, size_t len, int must_commit)
{
    size_t first, last;
    if (!layout_diff_span(from, to, len, &first, &last)) return 0;
    int rc = s->write(s->ctx, f, first, to + first, last - first + 1);
    if (rc) return rc;
    rc = s->commit(s->ctx, f);
    return must_commit ? rc : 0;
}

static int write_layout(const Store *s, const uint8_t *from_l, const uint8_t *from_s, const uint8_t *to_l, const uint8_t *to_s)
{
    int rc = write_diff(s, STORE_SAVEDATA, from_s, to_s, SAVEDATA_SIZE, 0);
    if (rc) return rc;
    return write_diff(s, STORE_LAUNCHER, from_l, to_l, LAUNCHER_SIZE, 1);
}

StoreStatus store_apply_layout(const Store *s, const uint8_t *cur_l, const uint8_t *cur_s, const uint8_t *to_l,
                               const uint8_t *to_s, int *err)
{
    int rc = write_layout(s, cur_l, cur_s, to_l, to_s);
    if (rc) {
        write_layout(s, to_l, to_s, cur_l, cur_s); /* best effort: put the originals back */
        if (err) *err = rc;
        return STORE_WRITE_FAILED;
    }
    uint8_t *rl = malloc(LAUNCHER_SIZE), *rs = malloc(SAVEDATA_SIZE);
    int bad = !rl || !rs || s->read(s->ctx, STORE_LAUNCHER, 0, rl, LAUNCHER_SIZE) || s->read(s->ctx, STORE_SAVEDATA, 0, rs, SAVEDATA_SIZE) ||
              memcmp(rl, to_l, LAUNCHER_SIZE) || memcmp(rs, to_s, SAVEDATA_SIZE);
    free(rl);
    free(rs);
    if (bad) {
        write_layout(s, to_l, to_s, cur_l, cur_s);
        return STORE_VERIFY_FAILED;
    }
    return STORE_OK;
}

StoreStatus store_set_icons(const Store *s, size_t entry, const uint8_t *old_icons, const uint8_t *new_icons, int *err)
{
    uint64_t off = (uint64_t)entry * SMDH_SIZE + SMDH_ICON_SMALL;
    uint8_t back[SMDH_ICONS_LEN];
    int rc = s->write(s->ctx, STORE_CACHED, off, new_icons, SMDH_ICONS_LEN);
    if (rc) {
        s->write(s->ctx, STORE_CACHED, off, old_icons, SMDH_ICONS_LEN);
        if (err) *err = rc;
        return STORE_WRITE_FAILED;
    }
    if (s->read(s->ctx, STORE_CACHED, off, back, SMDH_ICONS_LEN) || memcmp(back, new_icons, SMDH_ICONS_LEN)) {
        s->write(s->ctx, STORE_CACHED, off, old_icons, SMDH_ICONS_LEN);
        return STORE_VERIFY_FAILED;
    }
    s->commit(s->ctx, STORE_CACHED); /* extdata is written through; some firmwares refuse the commit */
    return STORE_OK;
}
