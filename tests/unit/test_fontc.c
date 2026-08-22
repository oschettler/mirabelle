#include "test.h"

#include "tools/fontc.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Baut einen Pfad im temporären Verzeichnis der Umgebung. Wie in test_pbm.c. */
static void make_temp_path(char *buf, size_t bufsize, const char *name)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/')
        snprintf(buf, bufsize, "%s%s", dir, name);
    else
        snprintf(buf, bufsize, "%s/%s", dir, name);
}

static bool write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fputs(content, f) >= 0;
    return fclose(f) == 0 && ok;
}

/* Liest eine (kleine) Datei komplett in buf, NUL-terminiert. -1 bei Fehler. */
static long read_whole_file(const char *path, char *buf, size_t bufsize)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    size_t n = fread(buf, 1, bufsize - 1, f);
    bool   truncated = (fgetc(f) != EOF);
    fclose(f);
    if (truncated) return -1;

    buf[n] = '\0';
    return (long)n;
}

/* 1-basierte Zeile des ersten Vorkommens von needle in text, -1 wenn nicht
 * gefunden. Damit müssen erwartete Zeilennummern in Tests nicht von Hand
 * gezählt werden - das Fixture ist die einzige Quelle der Wahrheit. */
static int line_of(const char *text, const char *needle)
{
    const char *pos = strstr(text, needle);
    if (!pos) return -1;

    int line = 1;
    for (const char *p = text; p < pos; p++)
        if (*p == '\n') line++;
    return line;
}

/* Gesamtzahl Zeilen; für Fehler, die erst am Dateiende auffallen. Fixtures
 * enden immer mit '\n' nach der letzten Inhaltszeile. */
static int total_lines(const char *text)
{
    int n = 0;
    for (const char *p = text; *p; p++)
        if (*p == '\n') n++;
    return n;
}

static bool errbuf_has_location(const char *errbuf, const char *path, int line)
{
    char prefix[300];
    snprintf(prefix, sizeof prefix, "%s:%d:", path, line);
    return strstr(errbuf, prefix) != NULL;
}

TEST(fontc_translates_minimal_valid_file)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_minimal.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_minimal.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+0041 width 1\n"
        "#\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    int  rc = fontc_run(in_path, out_path, "my_font", errbuf, sizeof errbuf);
    CHECK_EQ(rc, 0);

    char out[8192];
    long n = read_whole_file(out_path, out, sizeof out);
    CHECK(n > 0);
    if (n > 0) {
        CHECK(strstr(out, "#include \"gfx/font.h\"") != NULL);
        CHECK(strstr(out, "my_font_bits[]") != NULL);
        CHECK(strstr(out, "my_font_glyphs[]") != NULL);
        CHECK(strstr(out, "const font my_font =") != NULL);
    }

    remove(in_path);
    remove(out_path);
}

TEST(fontc_packs_bits_msb_left_with_zero_padding)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_bits.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_bits.c");

    /* width 9 -> stride 2. Zeile 0: 11100110 10000000, Zeile 1: 00000000
     * 10000000, Zeile 2: 01010101 00000000 - prüft MSB-links-Packung und
     * dass die Füllbits im zweiten Byte (Spalten 9..15) immer 0 bleiben. */
    const char *fnt =
        "name Test\n"
        "size 3\n"
        "ascent 2\n"
        "\n"
        "glyph U+0041 width 9\n"
        "###..##.#\n"
        "........#\n"
        ".#.#.#.#.\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n"
        "#\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "bitfont", errbuf, sizeof errbuf), 0);

    char out[8192];
    long n = read_whole_file(out_path, out, sizeof out);
    CHECK(n > 0);
    if (n > 0)
        CHECK(strstr(out, "0xE6, 0x80, 0x00, 0x80, 0x55, 0x00,") != NULL);

    remove(in_path);
    remove(out_path);
}

TEST(fontc_sorts_glyphs_by_codepoint)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_sort.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_sort.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+0042 width 1\n"
        "#\n"
        "\n"
        "glyph U+0041 width 1\n"
        "#\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "sortfont", errbuf, sizeof errbuf), 0);

    char out[8192];
    long n = read_whole_file(out_path, out, sizeof out);
    CHECK(n > 0);
    if (n > 0) {
        const char *p41   = strstr(out, "0x0041");
        const char *p42   = strstr(out, "0x0042");
        const char *pfffd = strstr(out, "0xFFFD");
        CHECK(p41 != NULL && p42 != NULL && pfffd != NULL);
        if (p41 && p42 && pfffd) {
            CHECK(p41 < p42);
            CHECK(p42 < pfffd);
        }
    }

    remove(in_path);
    remove(out_path);
}

TEST(fontc_unknown_keyword_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_unknown.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_unknown.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "foo bar\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "foo bar")));
    CHECK(strstr(errbuf, "foo") != NULL);

    remove(in_path);
    remove(out_path);
}

TEST(fontc_missing_name_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_noname.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_noname.c");

    const char *fnt =
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "glyph U+FFFD width 1")));
    CHECK(strstr(errbuf, "name") != NULL);

    remove(in_path);
    remove(out_path);
}

TEST(fontc_header_after_first_glyph_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_late_header.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_late_header.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n"
        "\n"
        "size 2\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "size 2")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_size_zero_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_size0.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_size0.c");

    const char *fnt =
        "name Test\n"
        "size 0\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "size 0")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_ascent_negative_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_ascentneg.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_ascentneg.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent -1\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "ascent -1")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_ascent_greater_than_size_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_ascentbig.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_ascentbig.c");

    const char *fnt =
        "name Test\n"
        "size 2\n"
        "ascent 5\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "ascent 5")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_bad_codepoint_syntax_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_badcp.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_badcp.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+41 width 1\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "glyph U+41 width 1")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_codepoint_above_max_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_cpmax.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_cpmax.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+110000 width 1\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "glyph U+110000 width 1")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_codepoint_in_surrogate_range_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_surrogate.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_surrogate.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+D800 width 1\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "glyph U+D800 width 1")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_duplicate_codepoint_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_dup.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_dup.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+0041 width 1\n"
        "#\n"
        "\n"
        "glyph U+0041 width 1  # doppelt\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    int first_line = line_of(fnt, "glyph U+0041 width 1\n");
    int dup_line    = line_of(fnt, "# doppelt");
    CHECK(first_line > 0 && dup_line > 0);

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, dup_line));

    char zeile_marker[32];
    snprintf(zeile_marker, sizeof zeile_marker, "Zeile %d", first_line);
    CHECK(strstr(errbuf, zeile_marker) != NULL);

    remove(in_path);
    remove(out_path);
}

TEST(fontc_width_zero_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_width0.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_width0.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 0\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "glyph U+FFFD width 0")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_width_too_large_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_width65.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_width65.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 65\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "glyph U+FFFD width 65")));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_pixel_row_wrong_length_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_rowlen.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_rowlen.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 3\n"
        "##\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "##")));
    CHECK(strstr(errbuf, "erwartet 3, 2 gefunden") != NULL);

    remove(in_path);
    remove(out_path);
}

TEST(fontc_pixel_row_invalid_character_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_badchar.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_badchar.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 3\n"
        "#x#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, line_of(fnt, "#x#")));
    CHECK(strstr(errbuf, "'x'") != NULL);
    CHECK(strstr(errbuf, "Spalte 2") != NULL);

    remove(in_path);
    remove(out_path);
}

TEST(fontc_too_few_pixel_rows_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_fewrows.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_fewrows.c");

    const char *fnt =
        "name Test\n"
        "size 2\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, total_lines(fnt)));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_too_many_pixel_rows_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_manyrows.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_manyrows.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, total_lines(fnt)));

    remove(in_path);
    remove(out_path);
}

TEST(fontc_missing_replacement_char_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_nofffd.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_nofffd.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+0041 width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, total_lines(fnt)));
    CHECK(strstr(errbuf, "FFFD") != NULL);

    remove(in_path);
    remove(out_path);
}

TEST(fontc_missing_input_file_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_missing.fnt");
    make_temp_path(out_path, sizeof out_path, "pda_test_fontc_missing.c");
    remove(in_path);   /* sicherstellen, dass sie wirklich nicht existiert */

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, in_path, 0));

    remove(out_path);
}

TEST(fontc_unwritable_output_is_rejected)
{
    char in_path[256], out_path[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_unwritable.fnt");
    make_temp_path(out_path, sizeof out_path,
                    "pda_test_fontc_missing_dir_xyz/out.c");

    const char *fnt =
        "name Test\n"
        "size 1\n"
        "ascent 1\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path, "f", errbuf, sizeof errbuf), 1);
    CHECK(errbuf_has_location(errbuf, out_path, 0));

    remove(in_path);
}

TEST(fontc_same_input_produces_identical_output)
{
    char in_path[256], out_path1[256], out_path2[256];
    make_temp_path(in_path, sizeof in_path, "pda_test_fontc_determ.fnt");
    make_temp_path(out_path1, sizeof out_path1, "pda_test_fontc_determ1.c");
    make_temp_path(out_path2, sizeof out_path2, "pda_test_fontc_determ2.c");

    const char *fnt =
        "name Test\n"
        "size 3\n"
        "ascent 2\n"
        "\n"
        "glyph U+0041 width 9\n"
        "###..##.#\n"
        "........#\n"
        ".#.#.#.#.\n"
        "\n"
        "glyph U+FFFD width 1\n"
        "#\n"
        "#\n"
        "#\n";
    CHECK(write_text_file(in_path, fnt));

    char errbuf[512];
    CHECK_EQ(fontc_run(in_path, out_path1, "detfont", errbuf, sizeof errbuf), 0);
    CHECK_EQ(fontc_run(in_path, out_path2, "detfont", errbuf, sizeof errbuf), 0);

    char out1[8192], out2[8192];
    long n1 = read_whole_file(out_path1, out1, sizeof out1);
    long n2 = read_whole_file(out_path2, out2, sizeof out2);
    CHECK(n1 > 0);
    CHECK_EQ(n1, n2);
    if (n1 > 0 && n1 == n2) CHECK_MEM(out1, out2, n1);

    remove(in_path);
    remove(out_path1);
    remove(out_path2);
}

int main(void)
{
    RUN(fontc_translates_minimal_valid_file);
    RUN(fontc_packs_bits_msb_left_with_zero_padding);
    RUN(fontc_sorts_glyphs_by_codepoint);
    RUN(fontc_unknown_keyword_is_rejected);
    RUN(fontc_missing_name_is_rejected);
    RUN(fontc_header_after_first_glyph_is_rejected);
    RUN(fontc_size_zero_is_rejected);
    RUN(fontc_ascent_negative_is_rejected);
    RUN(fontc_ascent_greater_than_size_is_rejected);
    RUN(fontc_bad_codepoint_syntax_is_rejected);
    RUN(fontc_codepoint_above_max_is_rejected);
    RUN(fontc_codepoint_in_surrogate_range_is_rejected);
    RUN(fontc_duplicate_codepoint_is_rejected);
    RUN(fontc_width_zero_is_rejected);
    RUN(fontc_width_too_large_is_rejected);
    RUN(fontc_pixel_row_wrong_length_is_rejected);
    RUN(fontc_pixel_row_invalid_character_is_rejected);
    RUN(fontc_too_few_pixel_rows_is_rejected);
    RUN(fontc_too_many_pixel_rows_is_rejected);
    RUN(fontc_missing_replacement_char_is_rejected);
    RUN(fontc_missing_input_file_is_rejected);
    RUN(fontc_unwritable_output_is_rejected);
    RUN(fontc_same_input_produces_identical_output);
    return test_summary();
}
