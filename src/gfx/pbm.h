/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Lesen und Schreiben von PBM-Dateien (Portable Bitmap).
 *
 * Zwei Varianten, mit einer Größenregel dahinter:
 *
 *   P1 (Text)   ein Zeichen je Pixel. Ein Vollbild mit 800 x 480 wäre so rund
 *               750 kB groß. Deshalb nur für Ausschnitte - dafür zeigt
 *               "git diff" die Änderung lesbar an.
 *   P4 (binär) ein Bit je Pixel, also genau unser Speicherformat. Ein Vollbild
 *               mit 800 x 480 sind 48 000 Bytes. Für Vollbilder, und davon nur
 *               eine Handvoll.
 *
 * Beim Schreiben von P1 steht eine Bildzeile auf einer Textzeile. Die Spezifikation
 * empfiehlt höchstens 70 Zeichen je Zeile; wir weichen bewusst ab, weil sonst der
 * einzige Grund für das Textformat wegfällt. Übliche Betrachter stört das nicht.
 */
#ifndef PDA_GFX_PBM_H
#define PDA_GFX_PBM_H

#include <stdbool.h>

#include "bitmap.h"

bool pbm_write_p1(const char *path, const bitmap *bm);
bool pbm_write_p4(const char *path, const bitmap *bm);

/* Erkennt P1 und P4 selbst. Legt out an; out gehört danach dem Aufrufer. */
bool pbm_read(const char *path, bitmap *out);

#endif /* PDA_GFX_PBM_H */
