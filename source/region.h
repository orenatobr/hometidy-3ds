/** @file region.h
 *  HOME Menu save ids per console region.
 */
#ifndef HT_REGION_H
#define HT_REGION_H

#include <stdint.h>

typedef struct {
    const char *name;
    uint32_t ext_id; /* SD extdata: SaveData.dat, Cache.dat, CacheD.dat */
    uint32_t sys_id; /* system save: Launcher.dat */
} Region;

/** Region of this console (Australia uses the European HOME Menu). */
const Region *region_detect(void);

#endif
