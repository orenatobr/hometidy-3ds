#include "region.h"

#include <3ds.h>

static const Region JPN = {"JPN", 0x82, 0x00020082}, USA = {"USA", 0x8F, 0x0002008F}, EUR = {"EUR", 0x98, 0x00020098},
                    CHN = {"CHN", 0xA1, 0x000200A1}, KOR = {"KOR", 0xA9, 0x000200A9}, TWN = {"TWN", 0xB1, 0x000200B1};

const Region *region_detect(void)
{
    u8 r = CFG_REGION_EUR;
    if (R_SUCCEEDED(cfguInit())) {
        CFGU_SecureInfoGetRegion(&r);
        cfguExit();
    }
    switch (r) {
    case CFG_REGION_JPN: return &JPN;
    case CFG_REGION_USA: return &USA;
    case CFG_REGION_CHN: return &CHN;
    case CFG_REGION_KOR: return &KOR;
    case CFG_REGION_TWN: return &TWN;
    default: return &EUR; /* EUR and AUS */
    }
}
