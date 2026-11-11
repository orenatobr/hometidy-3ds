#include "sortkey.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

#define TMP_MAX 512

static int is_word(unsigned char c) { return isalnum(c) || c == '_' || c >= 0x80; }

/* Replace each "(...)" and "[...]" with one space. An unclosed bracket stays as is. */
static size_t drop_tags(const char *s, char *out)
{
    size_t n = 0;
    while (*s && n < TMP_MAX - 1) {
        char close = *s == '(' ? ')' : *s == '[' ? ']' : 0;
        const char *end = close ? strchr(s + 1, close) : NULL;
        if (end) {
            out[n++] = ' ';
            s = end + 1;
        } else {
            out[n++] = *s++;
        }
    }
    out[n] = '\0';
    return n;
}

/* Remove every ",\s*the\b" (case insensitive). */
static size_t drop_tail_article(char *s, size_t len)
{
    size_t w = 0;
    for (size_t r = 0; r < len;) {
        if (s[r] == ',') {
            size_t k = r + 1;
            while (k < len && isspace((unsigned char)s[k])) k++;
            if (k + 3 <= len && strncasecmp(s + k, "the", 3) == 0 && (k + 3 == len || !is_word((unsigned char)s[k + 3]))) {
                r = k + 3;
                continue;
            }
        }
        s[w++] = s[r++];
    }
    s[w] = '\0';
    return w;
}

void sk_normalize(const char *title, char *out, size_t cap)
{
    char tmp[TMP_MAX];
    size_t len = drop_tail_article(tmp, drop_tags(title, tmp));

    char *p = tmp;
    while (len && isspace((unsigned char)*p)) { p++; len--; }
    if (len >= 4 && strncasecmp(p, "the", 3) == 0 && isspace((unsigned char)p[3])) p += 3;

    size_t n = 0;
    int pending_space = 0;
    for (; *p && n + 2 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x80 && isalnum(c)) {
            if (pending_space && n) out[n++] = ' ';
            pending_space = 0;
            out[n++] = (char)tolower(c);
        } else {
            pending_space = 1;
        }
    }
    out[n] = '\0';
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }

/* Compare two digit runs by value without overflow. */
static int cmp_digits(const char *a, size_t la, const char *b, size_t lb)
{
    while (la > 1 && *a == '0') { a++; la--; }
    while (lb > 1 && *b == '0') { b++; lb--; }
    if (la != lb) return la < lb ? -1 : 1;
    int c = memcmp(a, b, la);
    return (c > 0) - (c < 0);
}

int sk_compare(const char *a, const char *b)
{
    while (*a || *b) {
        if (!*a) return -1;
        if (!*b) return 1;
        int da = is_digit(*a), db = is_digit(*b);
        if (da != db) return da ? -1 : 1; /* a number sorts before text */
        const char *ea = a, *eb = b;
        while (*ea && is_digit(*ea) == da) ea++;
        while (*eb && is_digit(*eb) == db) eb++;
        int c;
        if (da) {
            c = cmp_digits(a, (size_t)(ea - a), b, (size_t)(eb - b));
        } else {
            size_t la = (size_t)(ea - a), lb = (size_t)(eb - b);
            c = memcmp(a, b, la < lb ? la : lb);
            if (!c && la != lb) c = la < lb ? -1 : 1;
            c = (c > 0) - (c < 0);
        }
        if (c) return c;
        a = ea;
        b = eb;
    }
    return 0;
}

int sk_order(const char *name_a, const char *key_a, const char *name_b, const char *key_b)
{
    int c = sk_compare(key_a, key_b);
    if (c) return c;
    for (; *name_a && *name_b; name_a++, name_b++) {
        int x = tolower((unsigned char)*name_a), y = tolower((unsigned char)*name_b);
        if (x != y) return x < y ? -1 : 1;
    }
    return (*name_a != 0) - (*name_b != 0);
}
