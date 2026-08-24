/* Der gemeinsame Zeilenleser, siehe core/lines.h.
 *
 * Er trägt drei Dateiformate. Was hier stimmt, stimmt für Thema,
 * Tastenbelegung und Sortiertabelle gleichermaßen - das ist der Zweck.
 */
#include "test.h"

#include <stdio.h>
#include <string.h>

#include "core/lines.h"

static const char *write_temp(const char *name, const char *content)
{
    static char path[512];
    snprintf(path, sizeof path, "/tmp/pda_lines_%s", name);

    FILE *fp = fopen(path, "wb");
    if (!fp) return NULL;
    fputs(content, fp);
    fclose(fp);
    return path;
}

TEST(words_come_out_one_line_at_a_time)
{
    const char *path = write_temp("worte", "eins zwei drei\nvier fuenf\n");
    REQUIRE(path != NULL);

    linereader r;
    char       err[256] = "";
    REQUIRE(lines_open(&r, path, err, sizeof err));

    REQUIRE(lines_next(&r));
    CHECK_EQ(r.count, 3);
    CHECK_STR(r.word[0], "eins");
    CHECK_STR(r.word[2], "drei");
    CHECK_EQ(r.line, 1);

    REQUIRE(lines_next(&r));
    CHECK_EQ(r.count, 2);
    CHECK_EQ(r.line, 2);

    CHECK(!lines_next(&r));
    lines_close(&r);
}

TEST(comments_and_empty_lines_do_not_count)
{
    /* Die Zeilennummer zählt trotzdem mit - sonst zeigte eine Fehlermeldung
     * auf die falsche Zeile, und das ist schlimmer als gar keine. */
    const char *path = write_temp("kommentare",
        "# ganz oben\n"
        "\n"
        "   \n"
        "eins zwei   # dahinter\n"
        "# noch einer\n"
        "drei vier\n");
    REQUIRE(path != NULL);

    linereader r;
    char       err[256] = "";
    REQUIRE(lines_open(&r, path, err, sizeof err));

    REQUIRE(lines_next(&r));
    CHECK_EQ(r.count, 2);
    CHECK_STR(r.word[1], "zwei");
    CHECK_EQ(r.line, 4);

    REQUIRE(lines_next(&r));
    CHECK_EQ(r.line, 6);

    CHECK(!lines_next(&r));
    lines_close(&r);
}

TEST(a_hash_ends_the_line_wherever_it_stands)
{
    /* Das ist die eine Regel, die alle drei Formate teilen sollen. Vorher
     * hatte jedes seine eigene Antwort darauf. */
    const char *path = write_temp("raute", "wert 12#kommentar\nzwei 3\n");
    REQUIRE(path != NULL);

    linereader r;
    char       err[256] = "";
    REQUIRE(lines_open(&r, path, err, sizeof err));

    REQUIRE(lines_next(&r));
    CHECK_EQ(r.count, 2);
    CHECK_STR(r.word[1], "12");

    lines_close(&r);
}

TEST(tabs_separate_like_spaces)
{
    const char *path = write_temp("tabs", "eins\tzwei \t drei\n");
    REQUIRE(path != NULL);

    linereader r;
    char       err[256] = "";
    REQUIRE(lines_open(&r, path, err, sizeof err));

    REQUIRE(lines_next(&r));
    CHECK_EQ(r.count, 3);
    CHECK_STR(r.word[1], "zwei");

    lines_close(&r);
}

TEST(a_line_without_a_trailing_newline_still_counts)
{
    const char *path = write_temp("ohne_umbruch", "eins zwei");
    REQUIRE(path != NULL);

    linereader r;
    char       err[256] = "";
    REQUIRE(lines_open(&r, path, err, sizeof err));

    REQUIRE(lines_next(&r));
    CHECK_EQ(r.count, 2);
    CHECK_STR(r.word[1], "zwei");

    lines_close(&r);
}

TEST(too_many_words_are_cut_off_not_refused)
{
    /* Der Leser urteilt nicht - er zerlegt. Wie viele Wörter erlaubt sind,
     * weiß nur das Format, und das prüft der Aufrufer an r.count. */
    char text[256] = "";
    for (int i = 0; i < LINES_MAX_WORDS + 4; i++)
        snprintf(text + strlen(text), sizeof text - strlen(text), "w%d ", i);
    snprintf(text + strlen(text), sizeof text - strlen(text), "\n");

    const char *path = write_temp("viele", text);
    REQUIRE(path != NULL);

    linereader r;
    char       err[256] = "";
    REQUIRE(lines_open(&r, path, err, sizeof err));

    REQUIRE(lines_next(&r));
    CHECK_EQ(r.count, LINES_MAX_WORDS);

    lines_close(&r);
}

TEST(a_missing_file_says_so)
{
    linereader r;
    char       err[256] = "";

    CHECK(!lines_open(&r, "/tmp/gibt-es-nicht-pda-lines", err, sizeof err));
    CHECK(strstr(err, "gibt-es-nicht-pda-lines") != NULL);
    CHECK(strstr(err, "nicht lesbar") != NULL);
}

TEST(a_message_names_file_and_line)
{
    const char *path = write_temp("meldung", "eins\nzwei\ndrei\n");
    REQUIRE(path != NULL);

    linereader r;
    char       err[256] = "";
    REQUIRE(lines_open(&r, path, err, sizeof err));

    REQUIRE(lines_next(&r));
    REQUIRE(lines_next(&r));

    CHECK(!lines_fail(&r, err, sizeof err, "so nicht: %d", 42));
    CHECK(strstr(err, ":2:") != NULL);
    CHECK(strstr(err, "so nicht: 42") != NULL);

    /* Ohne Zeilennummer für Fehler, die die ganze Datei betreffen. */
    CHECK(!lines_fail_file(path, err, sizeof err, "insgesamt kaputt"));
    CHECK(strstr(err, ":2:") == NULL);
    CHECK(strstr(err, "insgesamt kaputt") != NULL);

    lines_close(&r);
}

TEST(closing_twice_is_safe)
{
    const char *path = write_temp("zumachen", "eins\n");
    REQUIRE(path != NULL);

    linereader r;
    char       err[256] = "";
    REQUIRE(lines_open(&r, path, err, sizeof err));

    lines_close(&r);
    lines_close(&r);
}

int main(void)
{
    RUN(words_come_out_one_line_at_a_time);
    RUN(comments_and_empty_lines_do_not_count);
    RUN(a_hash_ends_the_line_wherever_it_stands);
    RUN(tabs_separate_like_spaces);
    RUN(a_line_without_a_trailing_newline_still_counts);
    RUN(too_many_words_are_cut_off_not_refused);
    RUN(a_missing_file_says_so);
    RUN(a_message_names_file_and_line);
    RUN(closing_twice_is_safe);

    return test_summary();
}
