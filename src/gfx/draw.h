/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Ein kleines QuickDraw.
 *
 * Alle Zeichenoperationen bekommen einen Zeichenzustand (gc) übergeben, statt
 * ihn global zu halten. Das kostet ein Argument und spart jede Diskussion
 * darüber, wer wann was zurücksetzen muss.
 *
 * Koordinaten sind relativ zu gc.origin. Gezeichnet wird immer mit gc.pat im
 * Modus gc.mode, und nie außerhalb von gc.clip.
 */
#ifndef PDA_GFX_DRAW_H
#define PDA_GFX_DRAW_H

#include "bitmap.h"
#include "pattern.h"

/* Wie das Musterbit (quelle) auf das vorhandene Pixel (ziel) wirkt.
 * XOR ist der wichtigste Modus: derselbe Aufruf zeichnet und löscht wieder,
 * das trägt den Fensterumriss beim Ziehen und den Schreibcursor. */
typedef enum {
    GFX_COPY,      /* ziel = quelle */
    GFX_OR,        /* ziel = ziel | quelle */
    GFX_XOR,       /* ziel = ziel ^ quelle */
    GFX_CLEAR,     /* ziel = ziel & ~quelle */
    GFX_NOTCOPY    /* ziel = ~quelle */
} gfx_mode;

typedef struct {
    bitmap  *dst;
    rect     clip;     /* in Bildkoordinaten, immer innerhalb von dst */
    point    origin;   /* wird auf jede übergebene Koordinate addiert */
    pattern  pat;
    gfx_mode mode;
} gc;

/* clip = ganzes Bild, origin = (0,0), pat = PAT_BLACK, mode = GFX_COPY. */
void gc_init(gc *g, bitmap *dst);

/* Schneidet r mit dem Bild. Ein leeres Ergebnis ist erlaubt: dann zeichnet
 * nichts mehr, statt dass irgendetwas danebengeht. */
void gc_clip(gc *g, rect r);

/* Nur der Schnitt aus r und dem bisherigen Clip. Fenster in Fenster. */
void gc_clip_intersect(gc *g, rect r);

rect rect_make(int x, int y, int w, int h);
rect rect_intersect(rect a, rect b);
bool rect_empty(rect r);
bool rect_contains(rect r, int x, int y);

/* Füllt den gesamten Clip-Bereich. */
void gfx_clear(gc *g);

void gfx_pset(gc *g, int x, int y);
void gfx_hline(gc *g, int x, int y, int w);
void gfx_vline(gc *g, int x, int y, int h);
void gfx_line(gc *g, int x0, int y0, int x1, int y1);

void gfx_fill_rect(gc *g, rect r);
void gfx_frame_rect(gc *g, rect r);         /* ein Pixel breiter Rahmen, innen liegend */
/* Abgerundetes Rechteck. Der Radius wird auf die halbe kürzere Kante
 * begrenzt; ein Radius von 0 ergibt ein gewöhnliches Rechteck.
 *
 * Gebraucht für Knöpfe: die waren in System 1 abgerundet, mit einem Radius
 * von vier Pixeln. */
void gfx_fill_round_rect(gc *g, rect r, int radius);
void gfx_frame_round_rect(gc *g, rect r, int radius);

void gfx_fill_oval(gc *g, rect r);
void gfx_frame_oval(gc *g, rect r);
void gfx_invert_rect(gc *g, rect r);        /* unabhängig von pat und mode */

/* Kopiert src nach (x,y). Modus und Clip gelten, das Muster nicht:
 * hier ist src die Quelle. */
void gfx_blit(gc *g, const bitmap *src, int x, int y);

/* Dasselbe für einen rohen 1-Bit-Block, der keine bitmap ist - etwa eine
 * Glyphe im Zeichensatz. Bitreihenfolge wie in bitmap.h: MSB links. */
void gfx_blit_bits(gc *g, const uint8_t *bits, int stride, int w, int h,
                   int x, int y);

#endif /* PDA_GFX_DRAW_H */
