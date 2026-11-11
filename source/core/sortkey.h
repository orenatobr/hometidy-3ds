/** @file sortkey.h
 *  Alphabetical ordering keys for game titles (articles, tags and punctuation ignored,
 *  digit runs compared as numbers).
 */
#ifndef HT_SORTKEY_H
#define HT_SORTKEY_H

#include <stddef.h>

/** Normalize a UTF-8 title into its sort key.
 *
 *  Tags in () and [] are dropped, ", The" and a leading "The " are ignored, everything
 *  that is not a letter or digit becomes one space, and letters are lowercased.
 *
 *  @param title Title in UTF-8.
 *  @param out   Destination buffer.
 *  @param cap   Size of @p out in bytes (at least 1).
 */
void sk_normalize(const char *title, char *out, size_t cap);

/** Compare two normalized keys; digit runs compare by value ("2" before "10"). */
int sk_compare(const char *a, const char *b);

/** Full title order: key first, then the lowercased title as tie break. */
int sk_order(const char *name_a, const char *key_a, const char *name_b, const char *key_b);

#endif
