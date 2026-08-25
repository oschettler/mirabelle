/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bitmap-Zeichensätze.
 *
 * Eine Schrift ist ein Feld von Glyphen, nach Codepunkt sortiert, plus ein
 * gemeinsamer Bitblock. Erzeugt wird beides von tools/fontc aus einer
 * .fnt-Textdatei; siehe docs/fnt-format.md.
 *
 * Alle Glyphen einer Schrift sind gleich hoch (size), unterschiedlich breit.
 * width ist zugleich der Vorschub zum nächsten Zeichen.
 */
#ifndef PDA_GFX_FONT_H
#define PDA_GFX_FONT_H

#include <stdint.h>

typedef struct {
    uint32_t codepoint;
    uint8_t  width;     /* Spalten und Vorschub */
    uint8_t  stride;    /* Bytes je Zeile: (width + 7) / 8 */
    uint32_t offset;    /* Startindex in font.bits */
} glyph;

typedef struct {
    const char    *name;
    int            size;      /* Höhe jeder Glyphe */
    int            ascent;    /* Zeilen oberhalb der Grundlinie */
    int            count;
    const glyph   *glyphs;    /* aufsteigend nach codepoint */
    const uint8_t *bits;
} font;

/* Binäre Suche. Fehlt der Codepunkt, wird die Glyphe für U+FFFD geliefert;
 * fehlt auch die, NULL. */
const glyph *font_find(const font *f, uint32_t codepoint);

#endif /* PDA_GFX_FONT_H */
