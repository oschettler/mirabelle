/* Prüft den Sollbild-Mechanismus selbst: die beiden Formate und die Größenregel.
 * Ab M2 benutzen ihn die Zeichentests, hier steht er selbst auf dem Prüfstand. */

#include "test.h"

#include "gfx/bitmap.h"
#include "support/golden.h"

/* Rahmen, Diagonale und ein gefülltes Quadrat: genug Struktur, dass eine
 * Verschiebung um ein Pixel im Textsollbild sofort auffällt. */
static void draw_probe(bitmap *bm)
{
    for (int x = 0; x < bm->w; x++) {
        bitmap_set(bm, x, 0, 1);
        bitmap_set(bm, x, bm->h - 1, 1);
    }
    for (int y = 0; y < bm->h; y++) {
        bitmap_set(bm, 0, y, 1);
        bitmap_set(bm, bm->w - 1, y, 1);
    }
    for (int i = 0; i < bm->w && i < bm->h; i++)
        bitmap_set(bm, i, i, 1);
    for (int y = 4; y < 8; y++)
        for (int x = 10; x < 14; x++)
            bitmap_set(bm, x, y, 1);
}

TEST(golden_region_as_p1)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 32, 24));
    draw_probe(&bm);
    CHECK(golden_check("probe_32x24", &bm));
    bitmap_free(&bm);
}

TEST(golden_fullscreen_as_p4)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 800, 480));
    draw_probe(&bm);
    CHECK(golden_check_full("probe_fullscreen", &bm));
    bitmap_free(&bm);
}

/* Die Größenregel steht im Code, nicht nur in der Dokumentation: ein Vollbild
 * als Textsollbild wäre rund 750 kB und wird abgewiesen. */
TEST(golden_rejects_oversized_text_image)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 800, 480));
    printf("  (die folgende Meldung ist erwartet)\n");
    CHECK(!golden_check("darf_es_nicht_geben", &bm));
    bitmap_free(&bm);
}

int main(void)
{
    RUN(golden_region_as_p1);
    RUN(golden_fullscreen_as_p4);
    RUN(golden_rejects_oversized_text_image);
    return test_summary();
}
