#include "i18n.h"

#include <3ds.h>

static const char *const EN[] = {
#define X(id, en, pt) en,
    HT_STRINGS(X)
#undef X
};

static const char *const PT[] = {
#define X(id, en, pt) pt,
    HT_STRINGS(X)
#undef X
};

static const char *const *table = EN;

void i18n_init(void)
{
    u8 lang = CFG_LANGUAGE_EN;
    if (R_SUCCEEDED(cfguInit())) {
        CFGU_GetSystemLanguage(&lang);
        cfguExit();
    }
    table = lang == CFG_LANGUAGE_PT ? PT : EN;
}

const char *tr(StrId id) { return table[id]; }
