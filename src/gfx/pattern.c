#include "pattern.h"

const pattern PAT_BLACK = { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };
const pattern PAT_WHITE = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

/* Schachbrett: jede Zeile das Komplement der vorigen. */
const pattern PAT_GRAY50 = { { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 } };

/* Jedes vierte Pixel gesetzt, in jeder Zeile gleich. */
const pattern PAT_GRAY25 = { { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88 } };

/* Komplement von PAT_GRAY25. */
const pattern PAT_GRAY75 = { { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77 } };

/* Eine einzelne Diagonale je Kachel; die Kachelgröße von acht Pixeln passt
 * genau zur Steigung eins, deshalb setzt sich die Schraffur über Kachelgrenzen
 * hinweg nahtlos fort. */
const pattern PAT_HATCH = { { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } };

pattern pattern_make(uint8_t r0, uint8_t r1, uint8_t r2, uint8_t r3,
                     uint8_t r4, uint8_t r5, uint8_t r6, uint8_t r7)
{
    pattern p;

    p.rows[0] = r0;
    p.rows[1] = r1;
    p.rows[2] = r2;
    p.rows[3] = r3;
    p.rows[4] = r4;
    p.rows[5] = r5;
    p.rows[6] = r6;
    p.rows[7] = r7;

    return p;
}
