/* Klappt ein 1-Bit-Bild nach RGB565 aus.
 *
 * Beide Plattformen brauchen das: der ESP32 schreibt damit in den PSRAM, und
 * SDL3 füllt damit seine Textur. Dass es dieselbe Umsetzung ist, hat einen
 * Grund über die Ersparnis hinaus - der für M16 heikelste Code wird schon ab
 * M4 auf dem Arbeitsplatz mitgetestet.
 *
 * Die Tabelle bildet ein Quellbyte auf acht Pixel ab. Ein Byte wird damit zu
 * einem memcpy über sechzehn Bytes, ohne Verzweigung je Pixel. Sie ist
 * 256 * 8 * 2 = 4096 Bytes groß und wird einmal beim Start aufgebaut.
 */
#ifndef PDA_PLAT_EXPAND_H
#define PDA_PLAT_EXPAND_H

#include <stdint.h>

#include "gfx/bitmap.h"

typedef struct {
    uint16_t px[256][8];
} expand_table;

/* on ist die Farbe für ein gesetztes Bit (schwarz), off die für ein leeres.
 * Zwei einstellbare Werte statt hart Schwarz auf Weiß kosten nichts, weil sie
 * ohnehin in der Tabelle stehen, und erlauben einen ruhigen Papierton. */
void expand_table_init(expand_table *t, uint16_t on, uint16_t off);

/* Klappt die Zeilen y0 bis einschließlich y1 aus src nach dst aus. dst zeigt
 * auf den Anfang des Zielbildes, dst_stride_px ist dessen Breite in Pixeln.
 * Bereiche außerhalb von src werden nicht angefasst. */
void expand_rows(const expand_table *t, const bitmap *src,
                 int y0, int y1, uint16_t *dst, int dst_stride_px);

#endif /* PDA_PLAT_EXPAND_H */
