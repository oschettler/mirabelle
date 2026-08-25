/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 8 x 8 Füllmuster.
 *
 * Muster ersetzen in einem System mit einem Bit je Pixel die Farbe: der
 * Schreibtisch ist ein Schachbrett, deaktivierte Menüeinträge sind gerastert.
 * Ein Muster ist acht Bytes, je Byte eine Zeile, MSB links — dieselbe
 * Bitreihenfolge wie in bitmap.h.
 *
 * Muster liegen am Zielbild aus, nicht am Objekt, das gezeichnet wird. Ein
 * gerastertes Rechteck sieht deshalb an jeder Stelle des Bildschirms gleich aus
 * und flimmert nicht, wenn ein Fenster darüber verschoben wird.
 */
#ifndef PDA_GFX_PATTERN_H
#define PDA_GFX_PATTERN_H

#include <stdint.h>

typedef struct {
    uint8_t rows[8];
} pattern;

/* Liefert das Musterbit für eine Position im Zielbild (0 oder 1). */
static inline int pattern_bit(const pattern *p, int x, int y)
{
    return (p->rows[(unsigned)y & 7u] >> (7 - ((unsigned)x & 7u))) & 1;
}

extern const pattern PAT_BLACK;    /* voll */
extern const pattern PAT_WHITE;    /* leer */
extern const pattern PAT_GRAY50;   /* Schachbrett, der Schreibtisch von System 1 */
extern const pattern PAT_GRAY25;   /* jedes vierte Pixel */
extern const pattern PAT_GRAY75;
extern const pattern PAT_HATCH;    /* Diagonalschraffur */

pattern pattern_make(uint8_t r0, uint8_t r1, uint8_t r2, uint8_t r3,
                     uint8_t r4, uint8_t r5, uint8_t r6, uint8_t r7);

#endif /* PDA_GFX_PATTERN_H */
