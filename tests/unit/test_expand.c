/* Das Ausklappen nach RGB565.
 *
 * Dieser Code läuft später auf dem ESP32 im heißesten Pfad überhaupt. Dass er
 * hier auf dem Arbeitsplatz mitgetestet wird, ist der eigentliche Zweck: der
 * für M16 heikelste Teil ist schon ab M4 abgesichert.
 */

#include "test.h"

#include <string.h>

#include "gfx/bitmap.h"
#include "plat/expand.h"

#define ON  0xF81Fu     /* auffällige Werte, damit Verwechslungen auffallen */
#define OFF 0x07E0u
#define UNTOUCHED 0xAAAAu

TEST(expand_table_is_msb_left)
{
    expand_table t;
    expand_table_init(&t, ON, OFF);

    CHECK_EQ(t.px[0x80][0], ON);
    for (int i = 1; i < 8; i++) CHECK_EQ(t.px[0x80][i], OFF);

    CHECK_EQ(t.px[0x01][7], ON);
    for (int i = 0; i < 7; i++) CHECK_EQ(t.px[0x01][i], OFF);
}

/* Gegenrechnung mit einer offensichtlichen Schleife, nicht mit derselben
 * Formel wie in expand.c - sonst prüfte der Test nur sich selbst. */
TEST(expand_table_matches_reference)
{
    expand_table t;
    expand_table_init(&t, ON, OFF);

    for (int b = 0; b < 256; b++) {
        int bit[8];
        int v = b;
        for (int i = 7; i >= 0; i--) { bit[i] = v & 1; v >>= 1; }

        for (int i = 0; i < 8; i++)
            CHECK_EQ(t.px[b][i], bit[i] ? ON : OFF);
    }
}

TEST(expand_rows_matches_bitmap_pixels)
{
    expand_table t;
    expand_table_init(&t, ON, OFF);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 24, 5));
    for (int y = 0; y < bm.h; y++)
        for (int x = 0; x < bm.w; x++)
            bitmap_set(&bm, x, y, (x * 3 + y) % 5 == 0);

    uint16_t dst[24 * 5];
    expand_rows(&t, &bm, 0, bm.h - 1, dst, 24);

    for (int y = 0; y < bm.h; y++)
        for (int x = 0; x < bm.w; x++)
            CHECK_EQ(dst[y * 24 + x], bitmap_get(&bm, x, y) ? ON : OFF);

    bitmap_free(&bm);
}

/* Der wichtigste Test der Datei: bei einer Breite, die nicht auf einer
 * Bytegrenze endet, dürfen die Füllbits keine Pixel im Zielbild erzeugen -
 * dort stehen fremde Bildpunkte. */
TEST(expand_rows_leaves_padding_columns_untouched)
{
    expand_table t;
    expand_table_init(&t, ON, OFF);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 12, 3));
    bitmap_clear(&bm, 1);

    const int stride = 20;
    uint16_t dst[20 * 3];
    for (int i = 0; i < 20 * 3; i++) dst[i] = UNTOUCHED;

    expand_rows(&t, &bm, 0, bm.h - 1, dst, stride);

    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 12; x++) CHECK_EQ(dst[y * stride + x], ON);
        for (int x = 12; x < stride; x++) CHECK_EQ(dst[y * stride + x], UNTOUCHED);
    }

    bitmap_free(&bm);
}

TEST(expand_rows_partial_range_leaves_other_rows)
{
    expand_table t;
    expand_table_init(&t, ON, OFF);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 16, 4));
    bitmap_clear(&bm, 1);

    uint16_t dst[16 * 4];
    for (int i = 0; i < 16 * 4; i++) dst[i] = UNTOUCHED;

    expand_rows(&t, &bm, 1, 2, dst, 16);

    for (int x = 0; x < 16; x++) {
        CHECK_EQ(dst[0 * 16 + x], UNTOUCHED);
        CHECK_EQ(dst[1 * 16 + x], ON);
        CHECK_EQ(dst[2 * 16 + x], ON);
        CHECK_EQ(dst[3 * 16 + x], UNTOUCHED);
    }

    bitmap_free(&bm);
}

TEST(expand_rows_clamps_range_outside_bitmap)
{
    expand_table t;
    expand_table_init(&t, ON, OFF);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 8, 2));
    bitmap_clear(&bm, 1);

    uint16_t dst[8 * 2];
    for (int i = 0; i < 16; i++) dst[i] = UNTOUCHED;

    expand_rows(&t, &bm, -5, 99, dst, 8);        /* darf nicht danebengreifen */
    for (int i = 0; i < 16; i++) CHECK_EQ(dst[i], ON);

    for (int i = 0; i < 16; i++) dst[i] = UNTOUCHED;
    expand_rows(&t, &bm, 5, 9, dst, 8);          /* ganz außerhalb */
    for (int i = 0; i < 16; i++) CHECK_EQ(dst[i], UNTOUCHED);

    bitmap_free(&bm);
}

int main(void)
{
    RUN(expand_table_is_msb_left);
    RUN(expand_table_matches_reference);
    RUN(expand_rows_matches_bitmap_pixels);
    RUN(expand_rows_leaves_padding_columns_untouched);
    RUN(expand_rows_partial_range_leaves_other_rows);
    RUN(expand_rows_clamps_range_outside_bitmap);
    return test_summary();
}
