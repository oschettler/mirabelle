/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test.h"

#include "gfx/bitmap.h"

#include <string.h>

/* Prüft Breite, Höhe und die daraus abgeleitete Bytebreite je Zeile. */
TEST(bitmap_init_dimensions)
{
    struct {
        int w, h, stride;
    } cases[] = {
        {1,   1, 1},
        {8,   1, 1},
        {9,   1, 2},
        {800, 1, 100},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        bitmap bm;
        CHECK(bitmap_init(&bm, cases[i].w, cases[i].h));
        CHECK_EQ(bm.w, cases[i].w);
        CHECK_EQ(bm.h, cases[i].h);
        CHECK_EQ(bm.stride, cases[i].stride);
        bitmap_free(&bm);
    }
}

TEST(bitmap_init_is_white)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 13, 5));

    for (int y = 0; y < bm.h; y++)
        for (int x = 0; x < bm.w; x++)
            CHECK_EQ(bitmap_get(&bm, x, y), 0);

    bitmap_free(&bm);
}

TEST(bitmap_init_rejects_non_positive_size)
{
    bitmap bm;
    CHECK(!bitmap_init(&bm, 0, 5));
    CHECK(!bitmap_init(&bm, 5, 0));
    CHECK(!bitmap_init(&bm, -1, 5));
    CHECK(!bitmap_init(&bm, 5, -1));
    CHECK(!bitmap_init(&bm, 0, 0));
}

TEST(bitmap_set_get_corners)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 10, 6));

    bitmap_set(&bm, 0, 0, 1);
    bitmap_set(&bm, 9, 0, 1);
    bitmap_set(&bm, 0, 5, 1);
    bitmap_set(&bm, 9, 5, 1);

    CHECK_EQ(bitmap_get(&bm, 0, 0), 1);
    CHECK_EQ(bitmap_get(&bm, 9, 0), 1);
    CHECK_EQ(bitmap_get(&bm, 0, 5), 1);
    CHECK_EQ(bitmap_get(&bm, 9, 5), 1);

    /* Nachbarpixel dürfen nicht mit betroffen sein. */
    CHECK_EQ(bitmap_get(&bm, 1, 0), 0);
    CHECK_EQ(bitmap_get(&bm, 8, 5), 0);

    bitmap_free(&bm);
}

/* Die Bitreihenfolge: das linkeste Pixel liegt im höchstwertigen Bit. */
TEST(bitmap_set_bit_order)
{
    bitmap bm;

    CHECK(bitmap_init(&bm, 8, 1));
    bitmap_set(&bm, 0, 0, 1);
    CHECK_EQ(bm.bits[0], 0x80);
    bitmap_free(&bm);

    CHECK(bitmap_init(&bm, 8, 1));
    bitmap_set(&bm, 7, 0, 1);
    CHECK_EQ(bm.bits[0], 0x01);
    bitmap_free(&bm);
}

TEST(bitmap_out_of_bounds_access)
{
    bitmap bm, before;
    CHECK(bitmap_init(&bm, 10, 6));
    bitmap_set(&bm, 3, 2, 1);
    bitmap_set(&bm, 9, 5, 1);

    CHECK(bitmap_init(&before, 10, 6));
    memcpy(before.bits, bm.bits, bitmap_bytes(&bm));

    CHECK_EQ(bitmap_get(&bm, -1, 0), 0);
    CHECK_EQ(bitmap_get(&bm, 0, -1), 0);
    CHECK_EQ(bitmap_get(&bm, 10, 0), 0);
    CHECK_EQ(bitmap_get(&bm, 0, 6), 0);
    CHECK_EQ(bitmap_get(&bm, 1000, 1000), 0);

    bitmap_set(&bm, -1, 0, 1);
    bitmap_set(&bm, 0, -1, 1);
    bitmap_set(&bm, 10, 0, 1);
    bitmap_set(&bm, 0, 6, 1);
    bitmap_set(&bm, -5, -5, 1);
    bitmap_set(&bm, 1000, 1000, 1);

    /* Nichts davon darf die Bitmap verändert haben. */
    CHECK_MEM(bm.bits, before.bits, bitmap_bytes(&bm));

    bitmap_free(&bm);
    bitmap_free(&before);
}

/* Wichtigster Test der Datei: die Füllbits am Zeilenende bleiben 0,
 * auch wenn die Zeile mit schwarz gefüllt wird. Breite 12 endet nicht
 * auf einer Bytegrenze, das letzte Byte je Zeile hat daher 4 Füllbits. */
TEST(bitmap_clear_black_masks_padding)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 12, 4));
    bitmap_clear(&bm, 1);

    for (int y = 0; y < bm.h; y++) {
        for (int x = 0; x < bm.w; x++)
            CHECK_EQ(bitmap_get(&bm, x, y), 1);

        uint8_t last = bm.bits[(size_t)y * bm.stride + bm.stride - 1];
        CHECK_EQ(last, 0xF0);
    }

    bitmap_free(&bm);
}

TEST(bitmap_equal_variants)
{
    bitmap a, b, c;
    CHECK(bitmap_init(&a, 10, 5));
    CHECK(bitmap_init(&b, 10, 5));
    bitmap_set(&a, 2, 2, 1);
    bitmap_set(&b, 2, 2, 1);
    CHECK(bitmap_equal(&a, &b));

    CHECK(bitmap_init(&c, 11, 5));
    CHECK(!bitmap_equal(&a, &c));
    bitmap_free(&c);

    bitmap_set(&b, 5, 3, 1);
    CHECK(!bitmap_equal(&a, &b));

    bitmap_free(&a);
    bitmap_free(&b);
}

TEST(bitmap_copy_rect_inside)
{
    bitmap src, dst;
    CHECK(bitmap_init(&src, 20, 10));

    for (int i = 0; i < 10; i++)
        bitmap_set(&src, i, i, 1);
    bitmap_set(&src, 15, 3, 1);

    rect r = {5, 2, 8, 6};
    CHECK(bitmap_copy_rect(&dst, &src, r));
    CHECK_EQ(dst.w, r.w);
    CHECK_EQ(dst.h, r.h);

    for (int y = 0; y < r.h; y++)
        for (int x = 0; x < r.w; x++)
            CHECK_EQ(bitmap_get(&dst, x, y), bitmap_get(&src, r.x + x, r.y + y));

    bitmap_free(&src);
    bitmap_free(&dst);
}

TEST(bitmap_copy_rect_clips_to_white)
{
    bitmap src, dst;
    CHECK(bitmap_init(&src, 10, 10));
    bitmap_clear(&src, 1);   /* ganz schwarz, damit Weiß im Rand auffällt */

    rect r = {6, 6, 8, 8};   /* ragt bei x=10 und y=10 über src hinaus */
    CHECK(bitmap_copy_rect(&dst, &src, r));

    for (int y = 0; y < r.h; y++) {
        for (int x = 0; x < r.w; x++) {
            int sx = r.x + x, sy = r.y + y;
            int expect = (sx < src.w && sy < src.h) ? 1 : 0;
            CHECK_EQ(bitmap_get(&dst, x, y), expect);
        }
    }

    bitmap_free(&src);
    bitmap_free(&dst);
}

TEST(bitmap_bytes_matches_stride_times_height)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 17, 9));
    CHECK_EQ(bitmap_bytes(&bm), (size_t)bm.stride * (size_t)bm.h);
    bitmap_free(&bm);
}

int main(void)
{
    RUN(bitmap_init_dimensions);
    RUN(bitmap_init_is_white);
    RUN(bitmap_init_rejects_non_positive_size);
    RUN(bitmap_set_get_corners);
    RUN(bitmap_set_bit_order);
    RUN(bitmap_out_of_bounds_access);
    RUN(bitmap_clear_black_masks_padding);
    RUN(bitmap_equal_variants);
    RUN(bitmap_copy_rect_inside);
    RUN(bitmap_copy_rect_clips_to_white);
    RUN(bitmap_bytes_matches_stride_times_height);
    return test_summary();
}
