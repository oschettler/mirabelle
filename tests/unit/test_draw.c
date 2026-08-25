/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Tests für die Zeichenprimitive aus gfx/draw.c und gfx/pattern.c. */

#include "test.h"

#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/pattern.h"
#include "support/golden.h"

/* Zeichnet genau ein Pixel in eine frische bg-Fläche und liefert das Ergebnis
 * zurück. srcbit steuert das Muster (PAT_BLACK liefert überall 1, PAT_WHITE
 * überall 0), so lässt sich die Wahrheitstafel jedes Modus knapp prüfen. */
static int draw_one(gfx_mode mode, int bg, int srcbit)
{
    bitmap bm;
    gc     g;

    bitmap_init(&bm, 2, 2);
    bitmap_clear(&bm, bg);
    gc_init(&g, &bm);
    g.pat  = srcbit ? PAT_BLACK : PAT_WHITE;
    g.mode = mode;
    gfx_pset(&g, 0, 0);

    int result = bitmap_get(&bm, 0, 0);
    bitmap_free(&bm);
    return result;
}

TEST(mode_copy)
{
    CHECK_EQ(draw_one(GFX_COPY, 0, 1), 1);
    CHECK_EQ(draw_one(GFX_COPY, 0, 0), 0);
    CHECK_EQ(draw_one(GFX_COPY, 1, 1), 1);
    CHECK_EQ(draw_one(GFX_COPY, 1, 0), 0);
}

TEST(mode_or)
{
    CHECK_EQ(draw_one(GFX_OR, 0, 1), 1);
    CHECK_EQ(draw_one(GFX_OR, 0, 0), 0);
    CHECK_EQ(draw_one(GFX_OR, 1, 1), 1);
    CHECK_EQ(draw_one(GFX_OR, 1, 0), 1);
}

TEST(mode_xor)
{
    CHECK_EQ(draw_one(GFX_XOR, 0, 1), 1);
    CHECK_EQ(draw_one(GFX_XOR, 0, 0), 0);
    CHECK_EQ(draw_one(GFX_XOR, 1, 1), 0);
    CHECK_EQ(draw_one(GFX_XOR, 1, 0), 1);
}

TEST(mode_clear)
{
    CHECK_EQ(draw_one(GFX_CLEAR, 0, 1), 0);
    CHECK_EQ(draw_one(GFX_CLEAR, 0, 0), 0);
    CHECK_EQ(draw_one(GFX_CLEAR, 1, 1), 0);
    CHECK_EQ(draw_one(GFX_CLEAR, 1, 0), 1);
}

TEST(mode_notcopy)
{
    CHECK_EQ(draw_one(GFX_NOTCOPY, 0, 1), 0);
    CHECK_EQ(draw_one(GFX_NOTCOPY, 0, 0), 1);
    CHECK_EQ(draw_one(GFX_NOTCOPY, 1, 1), 0);
    CHECK_EQ(draw_one(GFX_NOTCOPY, 1, 0), 1);
}

/* Dasselbe Bildrechteck (4,4,8,8) über zwei verschiedene origin ansteuern.
 * Haftet das Muster am Zielbild statt am Objekt, sind beide Bitmaps identisch.
 *
 * Die Verschiebung muss die Musterphase tatsächlich verändern können, sonst
 * prüft der Test nichts. Bei PAT_GRAY50 ist das Bit (x+y) & 1, also ist das
 * Muster gegen jede Verschiebung mit gerader Quersumme invariant. Deshalb
 * (3,4) mit ungerader Summe - und zur Sicherheit ein zweiter Durchgang mit
 * PAT_HATCH, dessen Phase auch waagerecht wandert. */
static void pattern_anchoring_case(pattern pat, int ox, int oy)
{
    bitmap a, b;
    gc     g;

    CHECK(bitmap_init(&a, 20, 20));
    gc_init(&g, &a);
    g.pat    = pat;
    g.origin = (point){ 0, 0 };
    gfx_fill_rect(&g, rect_make(4, 4, 8, 8));

    CHECK(bitmap_init(&b, 20, 20));
    gc_init(&g, &b);
    g.pat    = pat;
    g.origin = (point){ ox, oy };
    gfx_fill_rect(&g, rect_make(4 - ox, 4 - oy, 8, 8));

    CHECK(bitmap_equal(&a, &b));

    bitmap_free(&a);
    bitmap_free(&b);
}

TEST(pattern_anchors_to_target_image)
{
    pattern_anchoring_case(PAT_GRAY50, 3, 4);
    pattern_anchoring_case(PAT_HATCH,  1, 0);
    pattern_anchoring_case(PAT_HATCH,  0, 1);
}

/* Rahmen und Linie ragen weit über den Clip hinaus. Die untere Bildhälfte
 * liegt vollständig außerhalb des Clips und muss byteweise identisch mit
 * einer nie bezeichneten Referenzbitmap bleiben. */
TEST(clip_sets_no_pixel_outside)
{
    bitmap bm, ref;
    gc     g;

    CHECK(bitmap_init(&bm, 8, 16));
    CHECK(bitmap_init(&ref, 8, 16));
    gc_init(&g, &bm);
    gc_clip(&g, rect_make(0, 0, 8, 8));

    gfx_frame_rect(&g, rect_make(0, 0, 8, 16));
    gfx_line(&g, 0, 15, 7, 0);

    size_t outside_off = (size_t)8 * (size_t)bm.stride;
    size_t outside_n   = bitmap_bytes(&bm) - outside_off;
    CHECK_MEM(bm.bits + outside_off, ref.bits + outside_off, outside_n);

    bitmap_free(&bm);
    bitmap_free(&ref);
}

TEST(gc_clip_intersect_only_shrinks)
{
    bitmap bm;
    gc     g;

    CHECK(bitmap_init(&bm, 20, 20));
    gc_init(&g, &bm);

    gc_clip(&g, rect_make(2, 2, 10, 10));
    gc_clip_intersect(&g, rect_make(5, 5, 10, 10));
    CHECK_EQ(g.clip.x, 5);
    CHECK_EQ(g.clip.y, 5);
    CHECK_EQ(g.clip.w, 7);
    CHECK_EQ(g.clip.h, 7);

    /* Ein größeres Rechteck als der Clip darf den Clip nicht wachsen lassen. */
    gc_clip_intersect(&g, rect_make(0, 0, 100, 100));
    CHECK_EQ(g.clip.x, 5);
    CHECK_EQ(g.clip.y, 5);
    CHECK_EQ(g.clip.w, 7);
    CHECK_EQ(g.clip.h, 7);

    /* Disjunktes Rechteck ergibt einen leeren Clip. */
    gc_clip_intersect(&g, rect_make(50, 50, 5, 5));
    CHECK(rect_empty(g.clip));

    bitmap_free(&bm);
}

/* r = {0,0,10,10}: die vier Ecken (0,0) (9,0) (0,9) (9,9) müssen nach dem
 * Rahmen gesetzt sein. Löschen sich waagerechter und senkrechter Teil an
 * einer Ecke gegenseitig aus, bleibt sie 0 - der klassische Fehler. */
TEST(frame_rect_xor_keeps_corners)
{
    bitmap bm;
    gc     g;

    CHECK(bitmap_init(&bm, 10, 10));
    gc_init(&g, &bm);
    g.mode = GFX_XOR;
    g.pat  = PAT_BLACK;

    gfx_frame_rect(&g, rect_make(0, 0, 10, 10));

    CHECK_EQ(bitmap_get(&bm, 0, 0), 1);
    CHECK_EQ(bitmap_get(&bm, 9, 0), 1);
    CHECK_EQ(bitmap_get(&bm, 0, 9), 1);
    CHECK_EQ(bitmap_get(&bm, 9, 9), 1);

    bitmap_free(&bm);
}

TEST(line_horizontal_vertical_diagonal)
{
    bitmap bm;
    gc     g;

    CHECK(bitmap_init(&bm, 20, 20));
    gc_init(&g, &bm);
    g.pat = PAT_BLACK;

    gfx_line(&g, 2, 2, 2, 2); /* ein einzelner Punkt */
    CHECK_EQ(bitmap_get(&bm, 2, 2), 1);

    gfx_line(&g, 5, 5, 10, 5); /* waagerecht */
    for (int x = 5; x <= 10; x++)
        CHECK_EQ(bitmap_get(&bm, x, 5), 1);

    gfx_line(&g, 12, 5, 8, 5); /* waagerecht, rückwärts */
    for (int x = 8; x <= 12; x++)
        CHECK_EQ(bitmap_get(&bm, x, 5), 1);

    gfx_line(&g, 3, 8, 3, 13); /* senkrecht */
    for (int y = 8; y <= 13; y++)
        CHECK_EQ(bitmap_get(&bm, 3, y), 1);

    gfx_line(&g, 15, 15, 15, 10); /* senkrecht, rückwärts */
    for (int y = 10; y <= 15; y++)
        CHECK_EQ(bitmap_get(&bm, 15, y), 1);

    gfx_line(&g, 0, 0, 6, 6); /* 45 Grad */
    for (int i = 0; i <= 6; i++)
        CHECK_EQ(bitmap_get(&bm, i, i), 1);

    bitmap_free(&bm);
}

/* Flacher Anstieg (dx=7, dy=2): von Hand nachgerechnete Bresenham-Folge, die
 * genau max(|dx|,|dy|)+1 = 8 Pixel enthält, beide Endpunkte eingeschlossen. */
TEST(line_shallow_slope)
{
    bitmap bm;
    gc     g;
    static const int px[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    static const int py[8] = { 0, 0, 1, 1, 1, 1, 2, 2 };

    CHECK(bitmap_init(&bm, 20, 20));
    gc_init(&g, &bm);
    g.pat = PAT_BLACK;

    gfx_line(&g, 0, 0, 7, 2);

    for (int i = 0; i < 8; i++)
        CHECK_EQ(bitmap_get(&bm, px[i], py[i]), 1);

    bitmap_free(&bm);
}

/* Trägt das Fensterziehen: derselbe XOR-Aufruf zweimal angewandt stellt den
 * Ausgangszustand exakt wieder her. */
TEST(xor_twice_restores_original)
{
    bitmap bm, vorher;
    gc     g;

    CHECK(bitmap_init(&bm, 20, 20));
    gc_init(&g, &bm);
    g.pat = PAT_GRAY50;
    gfx_fill_rect(&g, rect_make(0, 0, 20, 20));

    CHECK(bitmap_copy_rect(&vorher, &bm, rect_make(0, 0, 20, 20)));

    g.mode = GFX_XOR;
    g.pat  = PAT_GRAY25;
    gfx_fill_rect(&g, rect_make(3, 3, 10, 10));
    gfx_fill_rect(&g, rect_make(3, 3, 10, 10));

    CHECK(bitmap_equal(&bm, &vorher));

    bitmap_free(&bm);
    bitmap_free(&vorher);
}

TEST(invert_rect_ignores_pattern_and_mode)
{
    bitmap bm;
    gc     g;

    CHECK(bitmap_init(&bm, 10, 10));
    bitmap_set(&bm, 1, 1, 1);
    gc_init(&g, &bm);
    g.mode = GFX_XOR;  /* muss ignoriert werden */
    g.pat  = PAT_WHITE; /* muss ignoriert werden */

    gfx_invert_rect(&g, rect_make(0, 0, 5, 5));

    CHECK_EQ(bitmap_get(&bm, 0, 0), 1); /* war 0, gekippt */
    CHECK_EQ(bitmap_get(&bm, 1, 1), 0); /* war 1, gekippt */
    CHECK_EQ(bitmap_get(&bm, 5, 5), 0); /* außerhalb von r, unberührt */

    bitmap_free(&bm);
}

TEST(blit_partially_outside_clip)
{
    bitmap src, dst;
    gc     g;

    CHECK(bitmap_init(&src, 4, 4));
    bitmap_clear(&src, 1);
    CHECK(bitmap_init(&dst, 10, 10));
    gc_init(&g, &dst);
    gc_clip(&g, rect_make(0, 0, 6, 6));

    gfx_blit(&g, &src, 4, 4); /* src deckt Bild (4,4)-(7,7) ab, Clip endet bei 6 */

    CHECK_EQ(bitmap_get(&dst, 4, 4), 1);
    CHECK_EQ(bitmap_get(&dst, 5, 5), 1);
    CHECK_EQ(bitmap_get(&dst, 6, 4), 0);
    CHECK_EQ(bitmap_get(&dst, 4, 6), 0);
    CHECK_EQ(bitmap_get(&dst, 7, 7), 0);

    bitmap_free(&src);
    bitmap_free(&dst);
}

TEST(degenerate_input_draws_nothing)
{
    bitmap bm, leer;
    gc     g;

    CHECK(bitmap_init(&bm, 10, 10));
    gc_init(&g, &bm);
    g.pat = PAT_BLACK;

    gfx_fill_rect(&g, rect_make(2, 2, 0, 5));
    gfx_fill_rect(&g, rect_make(2, 2, -3, 5));
    gfx_fill_rect(&g, rect_make(2, 2, 5, 0));
    gfx_hline(&g, 2, 2, 0);
    gfx_hline(&g, 2, 2, -5);
    gfx_vline(&g, 2, 2, 0);
    gfx_vline(&g, 2, 2, -5);
    gfx_frame_rect(&g, rect_make(0, 0, 0, 0));
    gfx_frame_rect(&g, rect_make(1, 1, -4, 4));
    gfx_fill_oval(&g, rect_make(1, 1, -2, -2));
    gfx_frame_oval(&g, rect_make(1, 1, 0, 4));
    gfx_invert_rect(&g, rect_make(1, 1, -1, 4));

    gc_clip(&g, rect_make(0, 0, 0, 0));
    CHECK(rect_empty(g.clip));
    gfx_fill_rect(&g, rect_make(0, 0, 10, 10));
    gfx_pset(&g, 3, 3);

    CHECK(bitmap_init(&leer, 10, 10));
    CHECK(bitmap_equal(&bm, &leer));

    bitmap_free(&bm);
    bitmap_free(&leer);
}

TEST(golden_pattern_gray50_desk)
{
    bitmap bm;
    gc     g;

    CHECK(bitmap_init(&bm, 64, 48));
    gc_init(&g, &bm);
    g.pat = PAT_GRAY50;
    gfx_fill_rect(&g, rect_make(0, 0, bm.w, bm.h));

    CHECK(golden_check("pattern_gray50_desk", &bm));

    bitmap_free(&bm);
}

TEST(golden_draw_shapes)
{
    bitmap bm;
    gc     g;

    CHECK(bitmap_init(&bm, 64, 48));
    gc_init(&g, &bm);
    g.pat = PAT_BLACK;

    gfx_frame_rect(&g, rect_make(2, 2, 20, 16));
    gfx_fill_rect(&g, rect_make(30, 4, 14, 10));
    gfx_fill_oval(&g, rect_make(4, 26, 20, 14));
    gfx_line(&g, 30, 20, 60, 44);
    gfx_line(&g, 60, 20, 30, 44);

    CHECK(golden_check("draw_shapes", &bm));

    bitmap_free(&bm);
}

int main(void)
{
    RUN(mode_copy);
    RUN(mode_or);
    RUN(mode_xor);
    RUN(mode_clear);
    RUN(mode_notcopy);
    RUN(pattern_anchors_to_target_image);
    RUN(clip_sets_no_pixel_outside);
    RUN(gc_clip_intersect_only_shrinks);
    RUN(frame_rect_xor_keeps_corners);
    RUN(line_horizontal_vertical_diagonal);
    RUN(line_shallow_slope);
    RUN(xor_twice_restores_original);
    RUN(invert_rect_ignores_pattern_and_mode);
    RUN(blit_partially_outside_clip);
    RUN(degenerate_input_draws_nothing);
    RUN(golden_pattern_gray50_desk);
    RUN(golden_draw_shapes);
    return test_summary();
}
