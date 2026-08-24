#include "test.h"

#include "store/frontmatter.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Für alle Tests gebraucht als "Dateiname" in den Fehlermeldungen - ein
 * frei erfundener Name reicht, frontmatter_parse liest nie wirklich von der
 * Platte. */
static const char *NAME = "block.md";

/* Wie in test_i18n.c: prüft, ob die Meldung mit "name:zeile:" beginnt. */
static bool errbuf_has_location(const char *err, const char *name, int line)
{
    char prefix[300];
    snprintf(prefix, sizeof prefix, "%s:%d:", name, line);
    return strstr(err, prefix) != NULL;
}

/* --- Der gute Fall ----------------------------------------------------------- */

TEST(parses_full_block_with_scalars_and_a_list)
{
    const char *text =
        "---\n"
        "title: Mein Titel\n"
        "tags: [eins, zwei, drei]\n"
        "date: 2024-01-01\n"
        "---\n"
        "Körper geht hier weiter.\n"
        "Zweite Zeile.\n";
    size_t len = strlen(text);

    char   err[256]    = "";
    size_t body_offset = 999;
    frontmatter *fm = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(frontmatter_count(fm), 3);

    CHECK_STR(frontmatter_get(fm, "title"), "Mein Titel");
    CHECK_STR(frontmatter_get(fm, "date"), "2024-01-01");

    CHECK_EQ(frontmatter_list_count(fm, "tags"), 3);
    CHECK_STR(frontmatter_list_at(fm, "tags", 0), "eins");
    CHECK_STR(frontmatter_list_at(fm, "tags", 1), "zwei");
    CHECK_STR(frontmatter_list_at(fm, "tags", 2), "drei");

    const char *expected_body = "Körper geht hier weiter.\nZweite Zeile.\n";
    size_t      body_len      = len - body_offset;
    CHECK_EQ(body_len, strlen(expected_body));
    CHECK_MEM(text + body_offset, expected_body, body_len);

    frontmatter_free(fm);
}

/* --- Ohne Front Matter -------------------------------------------------------- */

TEST(text_without_leading_delimiter_has_no_frontmatter)
{
    const char *text = "Einfach nur Gemtext, kein Front Matter.\n";
    size_t      len  = strlen(text);

    char   err[256]    = "";
    size_t body_offset = 999;
    frontmatter *fm = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(frontmatter_count(fm), 0);
    CHECK_EQ(body_offset, 0);

    frontmatter_free(fm);
}

TEST(empty_text_has_no_frontmatter)
{
    char   err[256]    = "";
    size_t body_offset = 999;
    frontmatter *fm = frontmatter_parse("", 0, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(frontmatter_count(fm), 0);
    CHECK_EQ(body_offset, 0);

    frontmatter_free(fm);
}

/* --- Werte --------------------------------------------------------------------- */

TEST(get_on_unknown_key_returns_null)
{
    const char *text = "---\nkey: wert\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    CHECK(frontmatter_get(fm, "unbekannt") == NULL);

    frontmatter_free(fm);
}

TEST(get_on_list_returns_first_entry)
{
    const char *text = "---\nliste: [a, b, c]\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    CHECK_STR(frontmatter_get(fm, "liste"), "a");

    frontmatter_free(fm);
}

TEST(scalar_counts_as_list_with_one_entry)
{
    const char *text = "---\nwert: hallo\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    CHECK_EQ(frontmatter_list_count(fm, "wert"), 1);
    CHECK_STR(frontmatter_list_at(fm, "wert", 0), "hallo");

    frontmatter_free(fm);
}

TEST(empty_list_has_zero_entries)
{
    const char *text = "---\nliste: []\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    CHECK_EQ(frontmatter_list_count(fm, "liste"), 0);
    CHECK(frontmatter_list_at(fm, "liste", 0) == NULL);

    frontmatter_free(fm);
}

TEST(list_with_empty_middle_entry_has_three_entries)
{
    const char *text = "---\nliste: [a, , b]\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    CHECK_EQ(frontmatter_list_count(fm, "liste"), 3);
    CHECK_STR(frontmatter_list_at(fm, "liste", 0), "a");
    CHECK_STR(frontmatter_list_at(fm, "liste", 1), "");
    CHECK_STR(frontmatter_list_at(fm, "liste", 2), "b");

    frontmatter_free(fm);
}

TEST(empty_value_is_empty_string_not_null)
{
    const char *text = "---\nleer:\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    CHECK(frontmatter_has(fm, "leer"));
    CHECK_STR(frontmatter_get(fm, "leer"), "");

    frontmatter_free(fm);
}

TEST(whitespace_around_colon_is_removed_but_kept_inside_value)
{
    /* Ohne führenden Leerraum: der bedeutet in YAML Einrückung und damit
     * Verschachtelung, und die weist der Parser ausdrücklich ab. Getrimmt wird
     * der Leerraum UM den Doppelpunkt und am Ende des Wertes. */
    const char *text = "---\nspacey   :   a   b   c   \n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');

    CHECK(frontmatter_has(fm, "spacey"));
    CHECK_STR(frontmatter_get(fm, "spacey"), "a   b   c");

    frontmatter_free(fm);
}

TEST(umlauts_and_typographic_quotes_survive_unmodified)
{
    const char *text = "---\ntitel: Straße „Eins“\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    CHECK_STR(frontmatter_get(fm, "titel"), "Straße „Eins“");

    frontmatter_free(fm);
}

TEST(list_at_with_out_of_range_index_returns_null)
{
    const char *text = "---\nliste: [a, b]\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    CHECK(frontmatter_list_at(fm, "liste", -1) == NULL);
    CHECK(frontmatter_list_at(fm, "liste", 2) == NULL);
    CHECK(frontmatter_list_at(fm, "liste", 100) == NULL);

    frontmatter_free(fm);
}

/* --- Rundlauf -------------------------------------------------------------------- */

TEST(write_fails_with_small_buffer_and_reports_needed_size)
{
    const char *text = "---\nkey: wert\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);

    char   tiny[4];
    size_t needed = 0;
    CHECK(!frontmatter_write(fm, tiny, sizeof tiny, &needed));
    CHECK(needed > sizeof tiny);

    char *buf = malloc(needed);
    REQUIRE(buf != NULL);
    size_t needed2 = 0;
    CHECK(frontmatter_write(fm, buf, needed, &needed2));
    CHECK_EQ(needed2, needed);
    CHECK_EQ(strlen(buf), needed - 1);

    free(buf);
    frontmatter_free(fm);
}

/* Der wichtigste Test dieser Datei: parsen, schreiben, wieder parsen - die
 * zweite Struktur muss dieselben Schlüssel in derselben Reihenfolge und
 * mit denselben Werten enthalten wie die erste. Die Reihenfolge lässt sich
 * über die öffentliche Schnittstelle nicht direkt abfragen, deshalb wird sie
 * anhand der Position der Schlüssel im geschriebenen Text geprüft. */
TEST(round_trip_preserves_keys_in_order_with_same_values)
{
    const char *text =
        "---\n"
        "title: Erster Eintrag\n"
        "tags: [eins, zwei, drei]\n"
        "author: \n"
        "rating: 5\n"
        "---\n"
        "Körper.\n";
    size_t len = strlen(text);

    char   err[256]    = "";
    size_t body_offset;
    frontmatter *fm1 = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm1 != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(frontmatter_count(fm1), 4);

    size_t needed1 = 0;
    CHECK(!frontmatter_write(fm1, NULL, 0, &needed1));
    CHECK(needed1 > 0);

    char *out1 = malloc(needed1);
    REQUIRE(out1 != NULL);
    size_t written1 = 0;
    REQUIRE(frontmatter_write(fm1, out1, needed1, &written1));
    CHECK_EQ(written1, needed1);

    const char *p_title  = strstr(out1, "title:");
    const char *p_tags   = strstr(out1, "tags:");
    const char *p_author = strstr(out1, "author:");
    const char *p_rating = strstr(out1, "rating:");
    REQUIRE(p_title != NULL);
    REQUIRE(p_tags != NULL);
    REQUIRE(p_author != NULL);
    REQUIRE(p_rating != NULL);
    CHECK(p_title < p_tags);
    CHECK(p_tags < p_author);
    CHECK(p_author < p_rating);

    char   err2[256]    = "";
    size_t body_offset2 = 999;
    frontmatter *fm2 = frontmatter_parse(out1, strlen(out1), NAME, &body_offset2, err2, sizeof err2);
    REQUIRE(fm2 != NULL);
    CHECK_EQ(err2[0], '\0');
    CHECK_EQ(frontmatter_count(fm2), frontmatter_count(fm1));
    CHECK_EQ(body_offset2, strlen(out1));   /* der Block ist hier der ganze Text */

    CHECK_STR(frontmatter_get(fm2, "title"), "Erster Eintrag");
    CHECK_EQ(frontmatter_list_count(fm2, "tags"), 3);
    CHECK_STR(frontmatter_list_at(fm2, "tags", 0), "eins");
    CHECK_STR(frontmatter_list_at(fm2, "tags", 1), "zwei");
    CHECK_STR(frontmatter_list_at(fm2, "tags", 2), "drei");
    CHECK_STR(frontmatter_get(fm2, "author"), "");
    CHECK_STR(frontmatter_get(fm2, "rating"), "5");

    free(out1);
    frontmatter_free(fm1);
    frontmatter_free(fm2);
}

/* Ergänzt den vorigen Test: ein zweiter Schreib-Lese-Durchlauf auf der schon
 * eingelesenen Struktur muss byteweise denselben Text ergeben wie der erste -
 * sonst würde sich die Datei eines Nutzers bei jedem Speichern unbemerkt
 * verändern. */
TEST(second_write_after_round_trip_matches_the_first_byte_for_byte)
{
    const char *text =
        "---\n"
        "b: zwei\n"
        "a: [x, y]\n"
        "c: eins\n"
        "---\n";
    size_t len = strlen(text);

    char   err[256]    = "";
    size_t body_offset;
    frontmatter *fm1 = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm1 != NULL);

    size_t needed1 = 0;
    CHECK(!frontmatter_write(fm1, NULL, 0, &needed1));
    char *out1 = malloc(needed1);
    REQUIRE(out1 != NULL);
    REQUIRE(frontmatter_write(fm1, out1, needed1, &needed1));

    char   err2[256]    = "";
    size_t body_offset2;
    frontmatter *fm2 = frontmatter_parse(out1, strlen(out1), NAME, &body_offset2, err2, sizeof err2);
    REQUIRE(fm2 != NULL);
    CHECK_EQ(err2[0], '\0');

    size_t needed2 = 0;
    CHECK(!frontmatter_write(fm2, NULL, 0, &needed2));
    char *out2 = malloc(needed2);
    REQUIRE(out2 != NULL);
    REQUIRE(frontmatter_write(fm2, out2, needed2, &needed2));

    CHECK_EQ(needed1, needed2);
    CHECK_MEM(out1, out2, needed1);

    free(out1);
    free(out2);
    frontmatter_free(fm1);
    frontmatter_free(fm2);
}

/* --- Fehlerfälle ------------------------------------------------------------------ */

TEST(parse_rejects_missing_colon)
{
    const char *text = "---\nkeinwert\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 2));
}

TEST(parse_rejects_empty_key)
{
    const char *text = "---\n: wert\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 2));
}

TEST(parse_rejects_invalid_character_in_key)
{
    /* Großbuchstaben sind im winzigen Schlüsselalphabet nicht erlaubt. */
    const char *text = "---\nKey: wert\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 2));
}

TEST(parse_rejects_space_in_key)
{
    /* Der heimtückischste Fall: "foo bar" wird ohne diese Prüfung bis zum
     * Doppelpunkt gelesen und ließe sich danach von niemandem mehr
     * nachschlagen. */
    const char *text = "---\nfoo bar: wert\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 2));
}

TEST(parse_rejects_duplicate_key_and_names_both_lines)
{
    const char *text = "---\nkey: eins\nkey: zwei\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 3));
    CHECK(strstr(err, "Zeile 2") != NULL);
    CHECK(strstr(err, "key") != NULL);
}

TEST(parse_rejects_key_that_is_too_long)
{
    char   text[256];
    size_t pos = 0;
    memcpy(text + pos, "---\n", 4);
    pos += 4;
    memset(text + pos, 'a', 100);   /* deutlich über der Grenze */
    pos += 100;
    const char *rest = ": wert\n---\n";
    memcpy(text + pos, rest, strlen(rest) + 1);
    pos += strlen(rest);

    char   err[256]    = "";
    size_t body_offset;
    frontmatter *fm = frontmatter_parse(text, pos, NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 2));
}

TEST(parse_rejects_value_that_is_too_long)
{
    char        text[700];
    size_t      pos  = 0;
    const char *head = "---\nkey: ";
    memcpy(text + pos, head, strlen(head));
    pos += strlen(head);
    memset(text + pos, 'x', 600);   /* deutlich über der Grenze */
    pos += 600;
    const char *tail = "\n---\n";
    memcpy(text + pos, tail, strlen(tail) + 1);
    pos += strlen(tail);

    char   err[256]    = "";
    size_t body_offset;
    frontmatter *fm = frontmatter_parse(text, pos, NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 2));
}

TEST(parse_rejects_block_that_is_never_closed)
{
    const char *text = "---\nkey: wert\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 1));
    CHECK(strstr(err, "geschlossen") != NULL);
}

TEST(parse_rejects_nested_mapping)
{
    const char *text = "---\nkey: wert\n  nested: verschachtelt\n---\n";
    char        err[256]    = "";
    size_t      body_offset;
    frontmatter *fm = frontmatter_parse(text, strlen(text), NAME, &body_offset, err, sizeof err);
    CHECK(fm == NULL);
    CHECK(errbuf_has_location(err, NAME, 3));
    CHECK(strstr(err, "verschachtel") != NULL);
    CHECK(strstr(err, "nicht unterstützt") != NULL);
}

/* --- Sonstiges --------------------------------------------------------------------- */

TEST(crlf_line_endings_parse_like_lf)
{
    const char *text =
        "---\r\n"
        "title: Titel\r\n"
        "tags: [a, b]\r\n"
        "---\r\n"
        "Körper\r\n";
    size_t len = strlen(text);

    char   err[256]    = "";
    size_t body_offset = 999;
    frontmatter *fm = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(frontmatter_count(fm), 2);
    CHECK_STR(frontmatter_get(fm, "title"), "Titel");
    CHECK_EQ(frontmatter_list_count(fm, "tags"), 2);
    CHECK_STR(frontmatter_list_at(fm, "tags", 0), "a");
    CHECK_STR(frontmatter_list_at(fm, "tags", 1), "b");

    const char *expected_body = "Körper\r\n";
    size_t      body_len      = len - body_offset;
    CHECK_EQ(body_len, strlen(expected_body));
    CHECK_MEM(text + body_offset, expected_body, body_len);

    frontmatter_free(fm);
}

TEST(mixed_line_endings_parse_the_same_as_pure_lf)
{
    const char *text =
        "---\r\n"
        "title: Titel\n"
        "tags: [a, b]\r\n"
        "---\n"
        "Körper\n";
    size_t len = strlen(text);

    char   err[256]    = "";
    size_t body_offset = 999;
    frontmatter *fm = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(frontmatter_count(fm), 2);
    CHECK_STR(frontmatter_get(fm, "title"), "Titel");
    CHECK_EQ(frontmatter_list_count(fm, "tags"), 2);
    CHECK_STR(frontmatter_list_at(fm, "tags", 0), "a");
    CHECK_STR(frontmatter_list_at(fm, "tags", 1), "b");

    frontmatter_free(fm);
}

TEST(blank_lines_inside_block_are_skipped)
{
    const char *text = "---\n\ntitle: Titel\n\n\ntags: [a]\n\n---\n";
    size_t      len  = strlen(text);

    char   err[256]    = "";
    size_t body_offset = 999;
    frontmatter *fm = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(frontmatter_count(fm), 2);
    CHECK_STR(frontmatter_get(fm, "title"), "Titel");
    CHECK_EQ(frontmatter_list_count(fm, "tags"), 1);
    CHECK_STR(frontmatter_list_at(fm, "tags", 0), "a");

    frontmatter_free(fm);
}

TEST(block_at_end_of_text_leaves_no_body)
{
    const char *text = "---\nkey: wert\n---\n";
    size_t      len  = strlen(text);

    char   err[256]    = "";
    size_t body_offset = 999;
    frontmatter *fm = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(body_offset, len);

    frontmatter_free(fm);
}

TEST(block_at_end_of_text_without_trailing_newline_leaves_no_body)
{
    const char *text = "---\nkey: wert\n---";   /* keine Zeile, kein Zeilenende danach */
    size_t      len  = strlen(text);

    char   err[256]    = "";
    size_t body_offset = 999;
    frontmatter *fm = frontmatter_parse(text, len, NAME, &body_offset, err, sizeof err);
    REQUIRE(fm != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(body_offset, len);

    frontmatter_free(fm);
}

int main(void)
{
    RUN(parses_full_block_with_scalars_and_a_list);

    RUN(text_without_leading_delimiter_has_no_frontmatter);
    RUN(empty_text_has_no_frontmatter);

    RUN(get_on_unknown_key_returns_null);
    RUN(get_on_list_returns_first_entry);
    RUN(scalar_counts_as_list_with_one_entry);
    RUN(empty_list_has_zero_entries);
    RUN(list_with_empty_middle_entry_has_three_entries);
    RUN(empty_value_is_empty_string_not_null);
    RUN(whitespace_around_colon_is_removed_but_kept_inside_value);
    RUN(umlauts_and_typographic_quotes_survive_unmodified);
    RUN(list_at_with_out_of_range_index_returns_null);

    RUN(write_fails_with_small_buffer_and_reports_needed_size);
    RUN(round_trip_preserves_keys_in_order_with_same_values);
    RUN(second_write_after_round_trip_matches_the_first_byte_for_byte);

    RUN(parse_rejects_missing_colon);
    RUN(parse_rejects_empty_key);
    RUN(parse_rejects_invalid_character_in_key);
    RUN(parse_rejects_space_in_key);
    RUN(parse_rejects_duplicate_key_and_names_both_lines);
    RUN(parse_rejects_key_that_is_too_long);
    RUN(parse_rejects_value_that_is_too_long);
    RUN(parse_rejects_block_that_is_never_closed);
    RUN(parse_rejects_nested_mapping);

    RUN(crlf_line_endings_parse_like_lf);
    RUN(mixed_line_endings_parse_the_same_as_pure_lf);
    RUN(blank_lines_inside_block_are_skipped);
    RUN(block_at_end_of_text_leaves_no_body);
    RUN(block_at_end_of_text_without_trailing_newline_leaves_no_body);

    return test_summary();
}
