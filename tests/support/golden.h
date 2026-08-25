/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Sollbild-Vergleich.
 *
 * golden_check()      für Ausschnitte, speichert P1 (Text, im Diff lesbar).
 * golden_check_full() für Vollbilder, speichert P4 (binär, kompakt).
 *
 * golden_check() weist zu große Bitmaps ab, statt das Repository mit
 * Textdateien von hunderten Kilobyte zu fluten. Die Größenregel steht damit
 * im Code und nicht nur in der Dokumentation.
 *
 * Mit gesetztem PDA_GOLDEN_ACCEPT=1 wird das Sollbild geschrieben statt
 * verglichen ("make test ACCEPT=1").
 */
#ifndef PDA_TEST_GOLDEN_H
#define PDA_TEST_GOLDEN_H

#include <stdbool.h>

#include "gfx/bitmap.h"

#define GOLDEN_MAX_P1_PIXELS (256 * 256)

bool golden_check(const char *name, const bitmap *bm);
bool golden_check_full(const char *name, const bitmap *bm);

#endif /* PDA_TEST_GOLDEN_H */
