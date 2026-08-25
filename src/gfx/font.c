/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "font.h"

#include <stddef.h>

const glyph *font_find(const font *f, uint32_t codepoint)
{
    int lo = 0, hi = f->count - 1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        uint32_t here = f->glyphs[mid].codepoint;

        if (here == codepoint) return &f->glyphs[mid];
        if (here <  codepoint) lo = mid + 1;
        else                   hi = mid - 1;
    }

    /* Fehlt das Zeichen, wird das Ersatzzeichen gezeichnet - sichtbar, statt
     * stillschweigend nichts. Nur wenn auch das fehlt, gibt es NULL. */
    if (codepoint != 0xFFFDu) return font_find(f, 0xFFFDu);
    return NULL;
}
