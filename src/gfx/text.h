/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Textsatz.
 *
 * Text ist UTF-8. Die Glyphensuche geht über den Codepunkt, nie über ein Byte.
 *
 * y ist die GRUNDLINIE, nicht die Oberkante. Das ist die typografisch richtige
 * Bezugslinie und die einzige, die stimmt, wenn einmal zwei Schriften in einer
 * Zeile stehen. Wer von oben rechnet, schreibt y + f->ascent.
 */
#ifndef PDA_GFX_TEXT_H
#define PDA_GFX_TEXT_H

#include "draw.h"
#include "font.h"

/* Zeichnet s mit gc.mode und gc.clip. Das Muster gilt nicht: Quelle sind die
 * Glyphenbits. Liefert die Breite des gezeichneten Textes. */
int gfx_text(gc *g, const font *f, int x, int y, const char *s);

/* Breite ohne zu zeichnen, in Pixeln. */
int text_width(const font *f, const char *s);

#endif /* PDA_GFX_TEXT_H */
