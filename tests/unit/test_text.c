/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Textsatz und der Zeichenvorrat-Vertrag.
 *
 * Der Test gegen required.set ist der wichtigste hier: er ist die Stelle, an
 * der auffällt, wenn eine Schrift ein Zeichen nicht hat, das die Oberfläche
 * braucht. Ohne ihn merkt man es erst, wenn ein Kästchen auf dem Schirm steht.
 */

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/utf8.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/text.h"
#include "support/golden.h"

extern const font system12;

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* --- Zeichenvorrat ------------------------------------------------------- */

/* Liest ein Feld wie "U+00E4" und liefert den Wert, oder 0 wenn die Zeichenkette
 * dort nicht so anfängt. Beendet wird bei *end. */
static uint32_t parse_codepoint(const char *s, const char **end)
{
    if (s[0] != 'U' || s[1] != '+') return 0;

    uint32_t v = 0;
    int digits = 0;
    s += 2;
    while (*s && strchr("0123456789ABCDEFabcdef", *s)) {
        char c = *s;
        int d = (c <= '9') ? c - '0' : (c | 32) - 'a' + 10;
        v = v * 16 + (uint32_t)d;
        digits++;
        s++;
    }

    *end = s;
    return digits >= 4 ? v : 0;
}

static int check_required_set(const font *f, int *missing_out)
{
    char path[512];
    snprintf(path, sizeof path, "%s/fonts/required.set", PDA_DATA_DIR);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("  required.set nicht lesbar: %s\n", path);
        return 0;
    }

    int checked = 0, missing = 0;
    char line[512];

    while (fgets(line, sizeof line, fp)) {
        const char *s = line;

        while (*s) {
            while (*s == ' ' || *s == '\t') s++;
            if (*s == '#' || *s == '\n' || *s == '\r' || *s == '\0') break;

            const char *end;
            uint32_t lo = parse_codepoint(s, &end);
            if (lo == 0) break;          /* ab hier ist die Zeile Fließtext */

            uint32_t hi = lo;
            if (end[0] == '.' && end[1] == '.') {
                const char *end2;
                uint32_t v = parse_codepoint(end + 2, &end2);
                if (v == 0) break;
                hi  = v;
                end = end2;
            }

            for (uint32_t cp = lo; cp <= hi; cp++) {
                checked++;
                const glyph *g = font_find(f, cp);
                if (!g || g->codepoint != cp) {
                    if (missing < 12)
                        printf("  fehlt: U+%04X\n", cp);
                    missing++;
                }
            }
            s = end;
        }
    }

    fclose(fp);
    *missing_out = missing;
    return checked;
}

TEST(font_covers_required_set)
{
    int missing = 0;
    int checked = check_required_set(&system12, &missing);

    CHECK(checked > 100);
    CHECK_EQ(missing, 0);
}


/* --- Abgeleitete Glyphen -------------------------------------------------- */

static int glyph_pixel(const font *f, const glyph *g, int x, int y)
{
    const uint8_t *row = f->bits + g->offset + (size_t)y * g->stride;
    return (row[x / 8] >> (7 - (x & 7))) & 1;
}

/* Jedes Pixel des Grundbuchstabens muss auch im abgeleiteten Zeichen gesetzt
 * sein. Obermenge statt Gleichheit, weil ø und Ø einen Durchstrich zufügen. */
TEST(derived_glyphs_contain_their_base)
{
    char path[512];
    snprintf(path, sizeof path, "%s/fonts/derived.map", PDA_DATA_DIR);

    FILE *fp = fopen(path, "rb");
    REQUIRE(fp != NULL);

    int pairs = 0;
    char line[512];

    while (fgets(line, sizeof line, fp)) {
        const char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == '\n' || *s == '\r' || *s == '\0') continue;

        const char *end;
        uint32_t dcp = parse_codepoint(s, &end);
        if (dcp == 0) continue;
        while (*end == ' ' || *end == '\t') end++;
        uint32_t bcp = parse_codepoint(end, &end);
        if (bcp == 0) continue;

        const glyph *d = font_find(&system12, dcp);
        const glyph *b = font_find(&system12, bcp);
        if (!d || !b || d->codepoint != dcp || b->codepoint != bcp) {
            printf("  U+%04X oder U+%04X fehlt im Zeichensatz\n", dcp, bcp);
            CHECK(false);
            continue;
        }

        pairs++;

        if (d->width != b->width) {
            printf("  U+%04X ist %d breit, Grundbuchstabe U+%04X aber %d\n",
                   dcp, d->width, bcp, b->width);
            CHECK(false);
            continue;
        }

        for (int y = 0; y < system12.size; y++) {
            for (int x = 0; x < b->width; x++) {
                if (glyph_pixel(&system12, b, x, y) &&
                    !glyph_pixel(&system12, d, x, y)) {
                    printf("  U+%04X fehlt ein Pixel des Grundbuchstabens "
                           "U+%04X bei x %d, y %d\n", dcp, bcp, x, y);
                    CHECK(false);
                    y = system12.size;      /* eine Meldung je Paar genügt */
                    break;
                }
            }
        }
    }

    fclose(fp);
    CHECK(pairs >= 20);
}

/* --- Glyphensuche -------------------------------------------------------- */

TEST(font_find_hits_first_last_and_middle)
{
    const glyph *first = font_find(&system12, system12.glyphs[0].codepoint);
    const glyph *last  = font_find(&system12, system12.glyphs[system12.count - 1].codepoint);
    const glyph *mid   = font_find(&system12, 'M');

    REQUIRE(first && last && mid);
    CHECK_EQ(first->codepoint, system12.glyphs[0].codepoint);
    CHECK_EQ(last->codepoint,  system12.glyphs[system12.count - 1].codepoint);
    CHECK_EQ(mid->codepoint,   'M');
}

/* Ein fehlendes Zeichen wird sichtbar ersetzt, nicht verschluckt. */
TEST(font_find_falls_back_to_replacement)
{
    const glyph *g = font_find(&system12, 0x4E2D);   /* nicht im Vorrat */
    REQUIRE(g != NULL);
    CHECK_EQ(g->codepoint, 0xFFFDu);
}

TEST(font_glyphs_are_sorted)
{
    for (int i = 1; i < system12.count; i++)
        CHECK(system12.glyphs[i - 1].codepoint < system12.glyphs[i].codepoint);
}

/* --- Breiten ------------------------------------------------------------- */

TEST(text_width_sums_glyph_widths)
{
    const char *s = "Müller";
    int expect = 0;
    const char *p = s;

    while (*p) {
        uint32_t cp = utf8_next(&p);
        const glyph *g = font_find(&system12, cp);
        REQUIRE(g != NULL);
        expect += g->width;
    }

    CHECK_EQ(text_width(&system12, s), expect);
    CHECK_EQ(text_width(&system12, ""), 0);
}

/* Ein Umlaut ist ein Zeichen, keine zwei - die Breite darf nicht von der
 * Bytelänge abhängen. */
TEST(text_width_counts_codepoints_not_bytes)
{
    CHECK_EQ(strlen("äöü"), 6u);
    CHECK_EQ(utf8_count("äöü"), 3u);

    const glyph *a = font_find(&system12, 0xE4);
    const glyph *o = font_find(&system12, 0xF6);
    const glyph *u = font_find(&system12, 0xFC);
    REQUIRE(a && o && u);

    CHECK_EQ(text_width(&system12, "äöü"), a->width + o->width + u->width);
}

TEST(gfx_text_returns_same_width_as_text_width)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 200, 20));
    gc g;
    gc_init(&g, &bm);

    const char *s = "Grüße!";
    CHECK_EQ(gfx_text(&g, &system12, 2, 12, s), text_width(&system12, s));

    bitmap_free(&bm);
}

/* --- Zeichnen ------------------------------------------------------------ */

TEST(gfx_text_respects_clip)
{
    bitmap bm, ref;
    CHECK(bitmap_init(&bm, 64, 32));
    CHECK(bitmap_init(&ref, 64, 32));

    gc g;
    gc_init(&g, &bm);
    gc_clip(&g, rect_make(0, 0, 64, 8));
    gfx_text(&g, &system12, 0, 8 + system12.ascent, "Hgjpq");   /* liegt ganz unter dem Clip */

    size_t off = (size_t)8 * (size_t)bm.stride;
    CHECK_MEM(bm.bits + off, ref.bits + off, bitmap_bytes(&bm) - off);

    bitmap_free(&bm);
    bitmap_free(&ref);
}

/* y ist die Grundlinie: derselbe Text, einmal über y und einmal über die
 * Oberkante gerechnet, muss dasselbe Bild ergeben. */
TEST(gfx_text_y_is_the_baseline)
{
    bitmap a, b;
    CHECK(bitmap_init(&a, 80, 20));
    CHECK(bitmap_init(&b, 80, 20));

    gc g;
    gc_init(&g, &a);
    gfx_text(&g, &system12, 2, 3 + system12.ascent, "Äpfel");

    gc_init(&g, &b);
    g.origin = (point){ 0, 3 };
    gfx_text(&g, &system12, 2, system12.ascent, "Äpfel");

    CHECK(bitmap_equal(&a, &b));

    bitmap_free(&a);
    bitmap_free(&b);
}

/* --- Sollbilder ---------------------------------------------------------- */

TEST(golden_german_sentence)
{
    bitmap bm;
    CHECK(bitmap_init(&bm, 232, 16));
    gc g;
    gc_init(&g, &bm);

    gfx_text(&g, &system12, 2, 2 + system12.ascent, "Grüße aus Köln, Fräulein Müller!");

    CHECK(golden_check("text_umlauts", &bm));
    bitmap_free(&bm);
}

/* Musterblatt über den ganzen Zeichenvorrat. Ändert sich eine einzige Glyphe,
 * zeigt git diff genau sie. */
TEST(golden_specimen)
{
    static const char *lines[] = {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        "abcdefghijklmnopqrstuvwxyz",
        "0123456789 !?.,:;'\"()[]{}",
        "+-*/=_|\\^~`@#$%&<>",
        "ÄÖÜäöüß ÉÈÀÇÑÅØ éèàçñåø",
        "„Anführung\" ‚einfach' – … € «» °",
    };
    const int n = (int)(sizeof lines / sizeof lines[0]);

    bitmap bm;
    CHECK(bitmap_init(&bm, 228, n * 13 + 2));
    gc g;
    gc_init(&g, &bm);

    for (int i = 0; i < n; i++)
        gfx_text(&g, &system12, 2, 1 + i * 13 + system12.ascent, lines[i]);

    CHECK(golden_check("font_specimen", &bm));
    bitmap_free(&bm);
}

int main(void)
{
    RUN(font_covers_required_set);
    RUN(derived_glyphs_contain_their_base);
    RUN(font_find_hits_first_last_and_middle);
    RUN(font_find_falls_back_to_replacement);
    RUN(font_glyphs_are_sorted);
    RUN(text_width_sums_glyph_widths);
    RUN(text_width_counts_codepoints_not_bytes);
    RUN(gfx_text_returns_same_width_as_text_width);
    RUN(gfx_text_respects_clip);
    RUN(gfx_text_y_is_the_baseline);
    RUN(golden_german_sentence);
    RUN(golden_specimen);
    return test_summary();
}
