#include "test.h"

#include "gfx/bitmap.h"
#include "gfx/pbm.h"

#include <string.h>

#include <stdio.h>
#include <stdlib.h>

/* Baut einen Pfad im temporären Verzeichnis der Umgebung. */
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

/* Ein einfaches, nicht triviales Muster - trifft auch die Ränder. */
static void fill_pattern(bitmap *bm)
{
    for (int y = 0; y < bm->h; y++)
        for (int x = 0; x < bm->w; x++)
            bitmap_set(bm, x, y, (x + y) % 3 == 0);
}

TEST(pbm_roundtrip_p1)
{
    char path[256];
    make_temp_path(path, sizeof(path), "pda_test_p1.pbm");

    bitmap bm;
    CHECK(bitmap_init(&bm, 10, 7));
    fill_pattern(&bm);
    CHECK(pbm_write_p1(path, &bm));

    bitmap read_bm;
    CHECK(pbm_read(path, &read_bm));
    CHECK(bitmap_equal(&bm, &read_bm));

    bitmap_free(&bm);
    bitmap_free(&read_bm);
    remove(path);
}

TEST(pbm_roundtrip_p4)
{
    char path[256];
    make_temp_path(path, sizeof(path), "pda_test_p4.pbm");

    bitmap bm;
    CHECK(bitmap_init(&bm, 16, 9));
    fill_pattern(&bm);
    CHECK(pbm_write_p4(path, &bm));

    bitmap read_bm;
    CHECK(pbm_read(path, &read_bm));
    CHECK(bitmap_equal(&bm, &read_bm));

    bitmap_free(&bm);
    bitmap_free(&read_bm);
    remove(path);
}

/* Breite 12 endet nicht auf einer Bytegrenze - testet, dass die Füllbits
 * beim Schreiben/Lesen als binäres P4 nicht durcheinandergeraten. */
TEST(pbm_roundtrip_p4_unaligned_width)
{
    char path[256];
    make_temp_path(path, sizeof(path), "pda_test_p4_unaligned.pbm");

    bitmap bm;
    CHECK(bitmap_init(&bm, 12, 5));
    fill_pattern(&bm);
    CHECK(pbm_write_p4(path, &bm));

    bitmap read_bm;
    CHECK(pbm_read(path, &read_bm));
    CHECK(bitmap_equal(&bm, &read_bm));

    bitmap_free(&bm);
    bitmap_free(&read_bm);
    remove(path);
}

TEST(pbm_read_detects_format_automatically)
{
    char path1[256], path4[256];
    make_temp_path(path1, sizeof(path1), "pda_test_detect_p1.pbm");
    make_temp_path(path4, sizeof(path4), "pda_test_detect_p4.pbm");

    bitmap bm;
    CHECK(bitmap_init(&bm, 9, 4));
    fill_pattern(&bm);
    CHECK(pbm_write_p1(path1, &bm));
    CHECK(pbm_write_p4(path4, &bm));

    bitmap r1, r4;
    CHECK(pbm_read(path1, &r1));
    CHECK(pbm_read(path4, &r4));
    CHECK(bitmap_equal(&bm, &r1));
    CHECK(bitmap_equal(&bm, &r4));

    bitmap_free(&bm);
    bitmap_free(&r1);
    bitmap_free(&r4);
    remove(path1);
    remove(path4);
}

/* Der P1-Kopf ist genau "P1\n<w> <h>\n", danach folgt je Bildzeile eine
 * Textzeile mit genau w Zeichen. Wird direkt am geschriebenen Text geprüft. */
TEST(pbm_p1_header_is_exact)
{
    char path[256];
    make_temp_path(path, sizeof(path), "pda_test_p1_header.pbm");

    bitmap bm;
    CHECK(bitmap_init(&bm, 5, 3));
    fill_pattern(&bm);
    CHECK(pbm_write_p1(path, &bm));

    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    if (!f) {
        bitmap_free(&bm);
        remove(path);
        return;
    }

    char expected[32];
    int  header_len = snprintf(expected, sizeof(expected), "P1\n%d %d\n", bm.w, bm.h);

    char header[32] = {0};
    size_t got = fread(header, 1, (size_t)header_len, f);
    CHECK_EQ(got, (size_t)header_len);
    CHECK_MEM(header, expected, header_len);

    int  line_count = 0;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        CHECK_EQ(len, (size_t)bm.w + 1);
        CHECK(len > 0 && line[len - 1] == '\n');
        line_count++;
    }
    CHECK_EQ(line_count, bm.h);

    fclose(f);
    bitmap_free(&bm);
    remove(path);
}

TEST(pbm_read_missing_file_fails)
{
    char path[256];
    make_temp_path(path, sizeof(path), "pda_test_missing_file.pbm");
    remove(path);   /* sicherstellen, dass sie wirklich nicht existiert */

    bitmap bm;
    CHECK(!pbm_read(path, &bm));
}

TEST(pbm_read_broken_content_fails)
{
    char path[256];
    make_temp_path(path, sizeof(path), "pda_test_broken.pbm");

    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        fputs("P9\n1 1\n0\n", f);
        fclose(f);
    }

    bitmap bm;
    CHECK(!pbm_read(path, &bm));

    remove(path);
}

TEST(pbm_read_skips_header_comments)
{
    char path[256];
    make_temp_path(path, sizeof(path), "pda_test_comment.pbm");

    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        fputs("P1\n# Kommentar in der Kopfzeile\n3 2\n101\n010\n", f);
        fclose(f);
    }

    bitmap bm;
    CHECK(pbm_read(path, &bm));
    CHECK_EQ(bm.w, 3);
    CHECK_EQ(bm.h, 2);
    CHECK_EQ(bitmap_get(&bm, 0, 0), 1);
    CHECK_EQ(bitmap_get(&bm, 1, 0), 0);
    CHECK_EQ(bitmap_get(&bm, 2, 0), 1);
    CHECK_EQ(bitmap_get(&bm, 0, 1), 0);
    CHECK_EQ(bitmap_get(&bm, 1, 1), 1);
    CHECK_EQ(bitmap_get(&bm, 2, 1), 0);

    bitmap_free(&bm);
    remove(path);
}

/* Der Kopf verspricht mehr Daten, als die Datei hat. Muss auffallen, sonst
 * liest ein beschädigtes Sollbild als halb leeres Bild durch. */
TEST(pbm_read_truncated_p4_fails)
{
    char path[256];
    make_temp_path(path, sizeof(path), "pda_test_truncated.pbm");

    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        fputs("P4\n16 4\n", f);
        fputc(0xFF, f);          /* statt 8 Bytes nur eines */
        fclose(f);
    }

    bitmap bm;
    CHECK(!pbm_read(path, &bm));

    remove(path);
}

int main(void)
{
    RUN(pbm_roundtrip_p1);
    RUN(pbm_roundtrip_p4);
    RUN(pbm_roundtrip_p4_unaligned_width);
    RUN(pbm_read_detects_format_automatically);
    RUN(pbm_p1_header_is_exact);
    RUN(pbm_read_missing_file_fails);
    RUN(pbm_read_broken_content_fails);
    RUN(pbm_read_skips_header_comments);
    RUN(pbm_read_truncated_p4_fails);
    return test_summary();
}
