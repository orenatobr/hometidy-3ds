#include "i18n.h"

static const char *const TEXT[] = {
#define X(id, text) text,
    HT_STRINGS(X)
#undef X
};

const char *tr(StrId id) { return TEXT[id]; }
