/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test.h"

#include "store/record.h"
#include "store/frontmatter.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- Kennungen ------------------------------------------------------------- */

/* Feste Zeit: 2023-11-14T22:13:20 UTC (1700000000 Sekunden seit 1970),
 * geprüft mit "date -u -r 1700000000". Damit ist die erwartete Zeichenkette
 * wörtlich nachrechenbar, ohne von der Zeitzone der Maschine abzuhängen. */
#define FIXED_TIME 1700000000L

TEST(make_id_produces_the_documented_form_for_a_fixed_time)
{
    char id[RECORD_ID_LEN + 1];
    record_make_id(id, sizeof id, FIXED_TIME, 0);

    CHECK_EQ(strlen(id), RECORD_ID_LEN);
    CHECK_STR(id, "20231114T221320-0000");
}

TEST(make_id_with_nonzero_seq_appears_as_lowercase_hex)
{
    char id[RECORD_ID_LEN + 1];
    record_make_id(id, sizeof id, FIXED_TIME, 0xAB);

    CHECK_STR(id, "20231114T221320-00ab");
}

/* Die Zerlegung geht über gmtime - das Ergebnis darf sich also nicht
 * ändern, wenn der Prozess in einer anderen Zeitzone läuft. Eine besonders
 * exotische Zone (UTC+14, keine runde Stundenzahl gegenüber Mitteleuropa)
 * deckt am ehesten auf, falls doch einmal localtime hineinrutscht. */
TEST(make_id_does_not_depend_on_the_local_time_zone)
{
    char *old_tz = getenv("TZ");
    char  saved[256] = "";
    bool  had_tz = old_tz != NULL;
    if (had_tz) snprintf(saved, sizeof saved, "%s", old_tz);

    setenv("TZ", "Pacific/Kiritimati", 1);
    tzset();

    char id[RECORD_ID_LEN + 1];
    record_make_id(id, sizeof id, FIXED_TIME, 0);
    CHECK_STR(id, "20231114T221320-0000");

    if (had_tz) setenv("TZ", saved, 1);
    else        unsetenv("TZ");
    tzset();
}

TEST(different_seq_within_the_same_second_gives_different_ids)
{
    char id_a[RECORD_ID_LEN + 1], id_b[RECORD_ID_LEN + 1];
    record_make_id(id_a, sizeof id_a, FIXED_TIME, 1);
    record_make_id(id_b, sizeof id_b, FIXED_TIME, 2);

    CHECK(strcmp(id_a, id_b) != 0);
}

/* Der Grund für die Form: alphabetische Sortierung ist zugleich zeitliche
 * Sortierung. Mehrere aufsteigende Zeiten - über Minute, Stunde, Tag, Monat
 * und Jahr hinweg - müssen in derselben Reihenfolge durch strcmp kommen. */
TEST(ids_sort_alphabetically_in_time_order)
{
    static const long times[] = {
        FIXED_TIME,                    /* Basis */
        FIXED_TIME + 1,                /* eine Sekunde später */
        FIXED_TIME + 59,               /* nächste Minute */
        FIXED_TIME + 3600,             /* nächste Stunde */
        FIXED_TIME + 86400,            /* nächster Tag */
        FIXED_TIME + 40L * 86400,      /* nächster Monat */
        FIXED_TIME + 400L * 86400,     /* nächstes Jahr */
    };
    enum { N = sizeof times / sizeof times[0] };

    char ids[N][RECORD_ID_LEN + 1];
    for (int i = 0; i < N; i++)
        record_make_id(ids[i], sizeof ids[i], times[i], 0);

    for (int i = 0; i + 1 < N; i++)
        CHECK(strcmp(ids[i], ids[i + 1]) < 0);
}

TEST(id_valid_accepts_a_well_formed_id)
{
    CHECK(record_id_valid("20240115T103000-00ab"));
}

TEST(id_valid_rejects_too_short)
{
    CHECK(!record_id_valid("20240115T103000-00a"));
}

TEST(id_valid_rejects_too_long)
{
    CHECK(!record_id_valid("20240115T103000-00abc"));
}

TEST(id_valid_rejects_missing_t)
{
    CHECK(!record_id_valid("20240115X103000-00ab"));
}

TEST(id_valid_rejects_missing_dash)
{
    CHECK(!record_id_valid("20240115T103000X00ab"));
}

/* record_make_id erzeugt Hexadezimalziffern immer klein geschrieben (siehe
 * record.h) - eine Kennung mit Großbuchstaben im Hexteil ist deshalb keine
 * gültige Form und muss abgewiesen werden. */
TEST(id_valid_rejects_uppercase_in_hex_part)
{
    CHECK(!record_id_valid("20240115T103000-00AB"));
}

TEST(id_valid_rejects_month_00)
{
    CHECK(!record_id_valid("20240015T103000-00ab"));
}

TEST(id_valid_rejects_month_13)
{
    CHECK(!record_id_valid("20241315T103000-00ab"));
}

TEST(id_valid_rejects_day_00)
{
    CHECK(!record_id_valid("20240100T103000-00ab"));
}

TEST(id_valid_rejects_day_32)
{
    CHECK(!record_id_valid("20240132T103000-00ab"));
}

TEST(id_valid_rejects_letters_in_the_digit_part)
{
    CHECK(!record_id_valid("2024011aT103000-00ab"));
}

TEST(id_valid_rejects_empty_string)
{
    CHECK(!record_id_valid(""));
}

/* --- Zerlegen ---------------------------------------------------------------- */

TEST(parse_full_file_yields_fields_and_body)
{
    const char *text =
        "---\n"
        "title: Einkaufsliste\n"
        "tags: [haus, garten]\n"
        "---\n"
        "Milch\n"
        "Brot\n";
    size_t len = strlen(text);

    char err[256] = "";
    record *r = record_parse(text, len, "datei.gmi", err, sizeof err);
    REQUIRE(r != NULL);
    CHECK_EQ(err[0], '\0');

    frontmatter *fm = record_fields(r);
    CHECK_STR(frontmatter_get(fm, "title"), "Einkaufsliste");
    CHECK_EQ(frontmatter_list_count(fm, "tags"), 2);
    CHECK_STR(frontmatter_list_at(fm, "tags", 0), "haus");
    CHECK_STR(frontmatter_list_at(fm, "tags", 1), "garten");

    CHECK_STR(record_body(r), "Milch\nBrot\n");

    record_free(r);
}

TEST(parse_file_without_frontmatter_has_empty_fields_and_whole_text_as_body)
{
    const char *text = "Einfach nur Gemtext, ganz ohne Block.\n";
    size_t      len  = strlen(text);

    char err[256] = "";
    record *r = record_parse(text, len, "datei.gmi", err, sizeof err);
    REQUIRE(r != NULL);
    CHECK_EQ(err[0], '\0');

    CHECK_EQ(frontmatter_count(record_fields(r)), 0);
    CHECK_STR(record_body(r), text);

    record_free(r);
}

/* --- Rundlauf ------------------------------------------------------------------ */

/* Hilfsfunktion: parsen, schreiben, byteweise mit dem Original vergleichen.
 * Der Rundlauf ist der wichtigste Vertrag von record.c - wer eine Datei liest
 * und unverändert zurückschreibt, darf keine Änderung erzeugen. */
static void check_round_trip(const char *text)
{
    size_t len = strlen(text);

    char err[256] = "";
    record *r = record_parse(text, len, "rundlauf.gmi", err, sizeof err);
    REQUIRE(r != NULL);
    CHECK_EQ(err[0], '\0');

    size_t needed = 0;
    CHECK(!record_write(r, NULL, 0, &needed));
    CHECK(needed > 0);

    char *out = malloc(needed);
    REQUIRE(out != NULL);
    size_t written = 0;
    REQUIRE(record_write(r, out, needed, &written));
    CHECK_EQ(written, needed);
    CHECK_EQ(written - 1, len);
    CHECK_MEM(out, text, len);

    free(out);
    record_free(r);
}

TEST(round_trip_is_byte_exact_with_frontmatter)
{
    check_round_trip(
        "---\n"
        "title: Rundlauf\n"
        "rating: 5\n"
        "---\n"
        "Erste Zeile.\n"
        "Zweite Zeile.\n");
}

TEST(round_trip_is_byte_exact_without_frontmatter)
{
    check_round_trip("Nur Gemtext, kein Front Matter, mehrere\nZeilen.\n");
}

TEST(round_trip_is_byte_exact_with_body_ending_in_newline)
{
    check_round_trip(
        "---\n"
        "key: wert\n"
        "---\n"
        "Körper endet mit einem Zeilenumbruch.\n");
}

TEST(round_trip_is_byte_exact_with_body_not_ending_in_newline)
{
    check_round_trip(
        "---\n"
        "key: wert\n"
        "---\n"
        "Körper endet OHNE Zeilenumbruch");
}

TEST(write_fails_with_small_buffer_and_reports_needed_size)
{
    const char *text = "---\nkey: wert\n---\nKörper.\n";

    char err[256] = "";
    record *r = record_parse(text, strlen(text), "klein.gmi", err, sizeof err);
    REQUIRE(r != NULL);

    char   tiny[4];
    size_t needed = 0;
    CHECK(!record_write(r, tiny, sizeof tiny, &needed));
    CHECK(needed > sizeof tiny);

    char *buf = malloc(needed);
    REQUIRE(buf != NULL);
    size_t written = 0;
    CHECK(record_write(r, buf, needed, &written));
    CHECK_EQ(written, needed);

    free(buf);
    record_free(r);
}

/* --- Körper direkt setzen ------------------------------------------------------- */

TEST(set_body_and_body_agree)
{
    record *r = record_create();
    REQUIRE(r != NULL);

    CHECK_STR(record_body(r), "");

    CHECK(record_set_body(r, "Neuer Inhalt.\n"));
    CHECK_STR(record_body(r), "Neuer Inhalt.\n");

    CHECK(record_set_body(r, "Und noch einmal etwas anderes."));
    CHECK_STR(record_body(r), "Und noch einmal etwas anderes.");

    record_free(r);
}

/* Ein Datensatz ganz ohne Felder bekommt beim Schreiben keinen "---"-Block -
 * sonst bekäme eine Datei, die nie Front Matter hatte, beim bloßen
 * Zurückschreiben plötzlich eins. */
TEST(record_without_fields_gets_no_dashes_block_when_written)
{
    record *r = record_create();
    REQUIRE(r != NULL);
    CHECK_EQ(frontmatter_count(record_fields(r)), 0);
    CHECK(record_set_body(r, "Nur Text, kein Block.\n"));

    size_t needed = 0;
    record_write(r, NULL, 0, &needed);
    char *out = malloc(needed);
    REQUIRE(out != NULL);
    size_t written = 0;
    REQUIRE(record_write(r, out, needed, &written));

    CHECK_STR(out, "Nur Text, kein Block.\n");
    CHECK(strstr(out, "---") == NULL);

    free(out);
    record_free(r);
}

/* --- Mehrbytezeichen ------------------------------------------------------------- */

TEST(umlauts_in_body_survive_unmodified)
{
    const char *text =
        "---\n"
        "title: Grüße\n"
        "---\n"
        "Straße „Eins“, Größe: ä ö ü ß.\n";

    char err[256] = "";
    record *r = record_parse(text, strlen(text), "utf8.gmi", err, sizeof err);
    REQUIRE(r != NULL);
    CHECK_EQ(err[0], '\0');

    CHECK_STR(record_body(r), "Straße „Eins“, Größe: ä ö ü ß.\n");

    size_t needed = 0;
    record_write(r, NULL, 0, &needed);
    char *out = malloc(needed);
    REQUIRE(out != NULL);
    size_t written = 0;
    REQUIRE(record_write(r, out, needed, &written));
    CHECK_EQ(written - 1, strlen(text));
    CHECK_MEM(out, text, strlen(text));

    free(out);
    record_free(r);
}

int main(void)
{
    RUN(make_id_produces_the_documented_form_for_a_fixed_time);
    RUN(make_id_with_nonzero_seq_appears_as_lowercase_hex);
    RUN(make_id_does_not_depend_on_the_local_time_zone);
    RUN(different_seq_within_the_same_second_gives_different_ids);
    RUN(ids_sort_alphabetically_in_time_order);

    RUN(id_valid_accepts_a_well_formed_id);
    RUN(id_valid_rejects_too_short);
    RUN(id_valid_rejects_too_long);
    RUN(id_valid_rejects_missing_t);
    RUN(id_valid_rejects_missing_dash);
    RUN(id_valid_rejects_uppercase_in_hex_part);
    RUN(id_valid_rejects_month_00);
    RUN(id_valid_rejects_month_13);
    RUN(id_valid_rejects_day_00);
    RUN(id_valid_rejects_day_32);
    RUN(id_valid_rejects_letters_in_the_digit_part);
    RUN(id_valid_rejects_empty_string);

    RUN(parse_full_file_yields_fields_and_body);
    RUN(parse_file_without_frontmatter_has_empty_fields_and_whole_text_as_body);

    RUN(round_trip_is_byte_exact_with_frontmatter);
    RUN(round_trip_is_byte_exact_without_frontmatter);
    RUN(round_trip_is_byte_exact_with_body_ending_in_newline);
    RUN(round_trip_is_byte_exact_with_body_not_ending_in_newline);
    RUN(write_fails_with_small_buffer_and_reports_needed_size);

    RUN(set_body_and_body_agree);
    RUN(record_without_fields_gets_no_dashes_block_when_written);

    RUN(umlauts_in_body_survive_unmodified);

    return test_summary();
}
