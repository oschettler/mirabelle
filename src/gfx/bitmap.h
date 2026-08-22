/* Eine Bitmap mit einem Bit je Pixel.
 *
 * Bitreihenfolge: das linkeste Pixel einer Zeile sitzt im höchstwertigen Bit
 * des ersten Bytes (MSB links). Ein gesetztes Bit bedeutet schwarz.
 *
 * Wichtige Zusicherung: die Füllbits am Zeilenende sind IMMER 0. Nur deshalb
 * dürfen bitmap_equal() und das Schreiben von P4-Dateien direkt über die
 * Bytes gehen, ohne Pixel für Pixel zu vergleichen.
 */
#ifndef PDA_GFX_BITMAP_H
#define PDA_GFX_BITMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int x, y;
} point;

typedef struct {
    int x, y, w, h;
} rect;

typedef struct {
    int      w, h;
    int      stride;   /* Bytes je Zeile: (w + 7) / 8 */
    uint8_t *bits;
    bool     owned;    /* true, wenn bitmap_free() den Speicher freigeben soll */
} bitmap;

/* Legt eine gelöschte (weiße) Bitmap an. false bei w/h <= 0 oder zu wenig Speicher. */
bool   bitmap_init(bitmap *bm, int w, int h);
void   bitmap_free(bitmap *bm);

/* Größe des Bitfeldes in Bytes. */
size_t bitmap_bytes(const bitmap *bm);

/* value: 0 löscht auf weiß, alles andere füllt mit schwarz. */
void   bitmap_clear(bitmap *bm, int value);

/* Außerhalb der Bitmap: get liefert 0, set tut nichts. */
int    bitmap_get(const bitmap *bm, int x, int y);
void   bitmap_set(bitmap *bm, int x, int y, int value);

/* Gleiche Größe und gleiche Bits. */
bool   bitmap_equal(const bitmap *a, const bitmap *b);

/* Legt dst als Kopie des Ausschnitts r aus src an. Teile außerhalb von src
 * werden weiß. dst muss uninitialisiert sein und wird bei true zum Eigentümer. */
bool   bitmap_copy_rect(bitmap *dst, const bitmap *src, rect r);

#endif /* PDA_GFX_BITMAP_H */
