/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "support/golden.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/pbm.h"

#ifndef PDA_GOLDEN_DIR
#define PDA_GOLDEN_DIR "tests/golden"
#endif

static void path_for(char *buf, size_t n, const char *name, const char *suffix)
{
    snprintf(buf, n, "%s/%s%s.pbm", PDA_GOLDEN_DIR, name, suffix);
}

static bool accepting(void)
{
    const char *v = getenv("PDA_GOLDEN_ACCEPT");
    return v != NULL && v[0] != '\0' && strcmp(v, "0") != 0;
}

/* Druckt den Bereich, in dem sich erwartet und bekommen unterscheiden.
 * Bei binären Vollbildern ist das der einzige Weg, überhaupt etwas zu sehen. */
static void print_diff(const bitmap *want, const bitmap *got)
{
    if (want->w != got->w || want->h != got->h) {
        printf("  Größe: erwartet %dx%d, bekommen %dx%d\n",
               want->w, want->h, got->w, got->h);
        return;
    }

    int x0 = want->w, y0 = want->h, x1 = -1, y1 = -1, n = 0;
    for (int y = 0; y < want->h; y++)
        for (int x = 0; x < want->w; x++)
            if (bitmap_get(want, x, y) != bitmap_get(got, x, y)) {
                if (x < x0) x0 = x;
                if (y < y0) y0 = y;
                if (x > x1) x1 = x;
                if (y > y1) y1 = y;
                n++;
            }

    if (x1 < 0) return;

    printf("  %d Pixel unterschiedlich, Bereich x %d..%d, y %d..%d\n",
           n, x0, x1, y0, y1);

    if (x1 - x0 >= 64 || y1 - y0 >= 32) {
        printf("  (Bereich zu groß für die Konsole)\n");
        return;
    }

    const char *titel[3] = { "erwartet", "bekommen", "Unterschied" };
    for (int which = 0; which < 3; which++) {
        printf("  %s:\n", titel[which]);
        for (int y = y0; y <= y1; y++) {
            printf("    ");
            for (int x = x0; x <= x1; x++) {
                int w = bitmap_get(want, x, y);
                int g = bitmap_get(got, x, y);
                int v = which == 0 ? w : which == 1 ? g : (w != g);
                putchar(which == 2 ? (v ? 'X' : '.') : (v ? '#' : '.'));
            }
            putchar('\n');
        }
    }
}

static bool check(const char *name, const bitmap *bm, bool binary)
{
    char path[512];
    path_for(path, sizeof path, name, "");

    if (accepting()) {
        bool ok = binary ? pbm_write_p4(path, bm) : pbm_write_p1(path, bm);
        printf("  Sollbild %s %s\n", name, ok ? "angenommen" : "NICHT SCHREIBBAR");
        return ok;
    }

    bitmap want;
    if (!pbm_read(path, &want)) {
        char actual[512];
        path_for(actual, sizeof actual, name, ".actual");
        if (binary) pbm_write_p4(actual, bm); else pbm_write_p1(actual, bm);
        printf("  Sollbild %s fehlt. Aktuelles Bild liegt in %s.\n", name, actual);
        printf("  Wenn es stimmt: make test ACCEPT=1\n");
        return false;
    }

    bool same = bitmap_equal(&want, bm);
    char actual[512];
    path_for(actual, sizeof actual, name, ".actual");

    if (same) {
        /* Eine Abweichungsdatei aus einem früheren Lauf wäre jetzt irreführend. */
        remove(actual);
    } else {
        if (binary) pbm_write_p4(actual, bm); else pbm_write_p1(actual, bm);
        printf("  Sollbild %s weicht ab. Aktuelles Bild liegt in %s.\n", name, actual);
        print_diff(&want, bm);
    }

    bitmap_free(&want);
    return same;
}

bool golden_check(const char *name, const bitmap *bm)
{
    if ((long)bm->w * bm->h > GOLDEN_MAX_P1_PIXELS) {
        printf("  %s ist mit %dx%d zu groß für ein Textsollbild.\n",
               name, bm->w, bm->h);
        printf("  Nimm einen Ausschnitt oder golden_check_full() für P4.\n");
        return false;
    }
    return check(name, bm, false);
}

bool golden_check_full(const char *name, const bitmap *bm)
{
    return check(name, bm, true);
}
