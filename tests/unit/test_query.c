/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Abfragen als Datenstruktur, siehe store/query.h.
 *
 * Hier läuft keine Datenbank. Das ist der Punkt: dieselbe Abfrage, die später
 * über den Index geht, muss auch ohne ihn zu beantworten sein - sonst wäre
 * der Index nicht abgeleitet, sondern nötig, und D-3 wäre gebrochen.
 */
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/collate.h"
#include "store/query.h"
#include "store/record.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

static collate *search_table(void)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/collate/search.fold", PDA_DATA_DIR);

    collate *c = collate_load(path, err, sizeof err);
    if (!c) printf("  Suchtabelle nicht ladbar: %s\n", err);
    return c;
}

static collate *sort_table(void)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/lang/de.sort", PDA_DATA_DIR);

    collate *c = collate_load(path, err, sizeof err);
    if (!c) printf("  Sortiertabelle nicht ladbar: %s\n", err);
    return c;
}

static record *rec_of(const char *text)
{
    char    err[256] = "";
    record *r = record_parse(text, strlen(text), "test", err, sizeof err);
    if (!r) printf("  Datensatz nicht lesbar: %s\n", err);
    return r;
}

/* Eine Aufgabe, wie sie im Vault liegt. */
static record *a_task(void)
{
    return rec_of("---\n"
                  "id: 20260301T101500-0001\n"
                  "title: Müller anrufen\n"
                  "due: 2026-03-15\n"
                  "status: offen\n"
                  "tags: [Arbeit, Telefon]\n"
                  "---\n"
                  "# Müller anrufen\n"
                  "Wegen der Lieferung aus Köln.\n");
}

/* --- Aufbauen ------------------------------------------------------------------ */

TEST(a_fresh_query_matches_everything)
{
    query q;
    query_init(&q, "Aufgaben");

    CHECK_STR(q.collection, "Aufgaben");
    CHECK_EQ(q.filter_count, 0);
    CHECK_EQ(q.limit, 0);

    record *r = a_task();
    REQUIRE(r != NULL);
    CHECK(query_matches(&q, r, NULL));
    record_free(r);
}

TEST(too_many_conditions_are_refused_not_dropped)
{
    query q;
    query_init(&q, NULL);

    for (int i = 0; i < QUERY_FILTERS_MAX; i++)
        CHECK(query_where(&q, "status", QF_EQUALS, "offen"));

    CHECK(!query_where(&q, "status", QF_EQUALS, "offen"));
    CHECK_EQ(q.filter_count, QUERY_FILTERS_MAX);
}

TEST(values_that_are_too_long_are_refused_not_truncated)
{
    /* Abgeschnitten wäre schlimmer als abgelehnt: die Abfrage liefe weiter
     * und suchte etwas anderes, als der Aufrufer wollte. */
    char big[QUERY_VALUE_MAX + 10];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';

    query q;
    query_init(&q, NULL);

    CHECK(!query_where(&q, "title", QF_CONTAINS, big));
    CHECK_EQ(q.filter_count, 0);

    CHECK(!query_text(&q, big));
    CHECK_EQ(q.text[0], '\0');

    CHECK(!query_where(&q, big, QF_PRESENT, NULL));
    CHECK(!query_order(&q, big, false));
}

TEST(a_condition_without_a_field_is_refused)
{
    query q;
    query_init(&q, NULL);

    CHECK(!query_where(&q, NULL, QF_EQUALS, "x"));
    CHECK(!query_where(&q, "", QF_EQUALS, "x"));
    CHECK(!query_where(&q, "title", QF_EQUALS, NULL));

    /* Bei PRESENT gibt es nichts zu vergleichen - dort ist NULL richtig. */
    CHECK(query_where(&q, "title", QF_PRESENT, NULL));
    CHECK_EQ(q.filters[0].value[0], '\0');
}

TEST(limit_and_offset_never_go_negative)
{
    query q;
    query_init(&q, NULL);

    query_limit(&q, -5, -3);
    CHECK_EQ(q.limit, 0);
    CHECK_EQ(q.offset, 0);
}

/* --- Bedingungen ---------------------------------------------------------------- */

TEST(equals_compares_character_by_character)
{
    query q;
    query_init(&q, NULL);
    CHECK(query_where(&q, "status", QF_EQUALS, "offen"));

    record *r = a_task();
    REQUIRE(r != NULL);
    CHECK(query_matches(&q, r, NULL));

    /* Gleichheit wird nicht gefaltet - „Offen" ist ein anderer Zustand als
     * „offen", und ein Zustandsfeld ist kein Fließtext. Geprüft MIT Tabelle:
     * ohne sie unterschieden sich Gleichheit und Enthaltensein hier gar
     * nicht, und der Test ließe offen, welche von beiden gemeint ist. */
    collate *c = search_table();
    REQUIRE(c != NULL);

    query_init(&q, NULL);
    CHECK(query_where(&q, "status", QF_EQUALS, "Offen"));
    CHECK(!query_matches(&q, r, c));

    /* Und Gleichheit ist auch kein Enthaltensein: ein Teilwort genügt nicht. */
    query_init(&q, NULL);
    CHECK(query_where(&q, "status", QF_EQUALS, "off"));
    CHECK(!query_matches(&q, r, c));

    query_init(&q, NULL);
    CHECK(query_where(&q, "status", QF_CONTAINS, "off"));
    CHECK(query_matches(&q, r, c));

    record_free(r);
    collate_free(c);
}

TEST(contains_folds_both_sides)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    query q;
    query_init(&q, NULL);
    CHECK(query_where(&q, "title", QF_CONTAINS, "muller"));

    record *r = a_task();
    REQUIRE(r != NULL);
    CHECK(query_matches(&q, r, c));

    /* Ohne Tabelle findet es nicht - genau der Unterschied, den die Faltung
     * ausmacht. */
    CHECK(!query_matches(&q, r, NULL));

    record_free(r);
    collate_free(c);
}

TEST(prefix_anchors_at_the_start_of_the_field)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    record *r = a_task();
    REQUIRE(r != NULL);

    query q;
    query_init(&q, NULL);
    CHECK(query_where(&q, "title", QF_PREFIX, "Mül"));
    CHECK(query_matches(&q, r, c));

    query_init(&q, NULL);
    CHECK(query_where(&q, "title", QF_PREFIX, "anrufen"));
    CHECK(!query_matches(&q, r, c));

    record_free(r);
    collate_free(c);
}

TEST(dates_compare_as_text_because_they_are_iso)
{
    record *r = a_task();     /* fällig am 2026-03-15 */
    REQUIRE(r != NULL);

    query q;
    query_init(&q, NULL);
    CHECK(query_where(&q, "due", QF_LESS, "2026-04-01"));
    CHECK(query_matches(&q, r, NULL));

    query_init(&q, NULL);
    CHECK(query_where(&q, "due", QF_LESS, "2026-03-01"));
    CHECK(!query_matches(&q, r, NULL));

    query_init(&q, NULL);
    CHECK(query_where(&q, "due", QF_GREATER, "2026-03-01"));
    CHECK(query_matches(&q, r, NULL));

    /* Der Jahreswechsel ist der Fall, an dem eine Vergleichsart auffliegt, die
     * nur auf Tag und Monat schaut. */
    query_init(&q, NULL);
    CHECK(query_where(&q, "due", QF_GREATER, "2025-12-31"));
    CHECK(query_matches(&q, r, NULL));

    record_free(r);
}

TEST(present_and_absent_ask_about_the_field_itself)
{
    record *r = a_task();
    REQUIRE(r != NULL);

    query q;
    query_init(&q, NULL);
    CHECK(query_where(&q, "due", QF_PRESENT, NULL));
    CHECK(query_matches(&q, r, NULL));

    query_init(&q, NULL);
    CHECK(query_where(&q, "reminder", QF_PRESENT, NULL));
    CHECK(!query_matches(&q, r, NULL));

    query_init(&q, NULL);
    CHECK(query_where(&q, "reminder", QF_ABSENT, NULL));
    CHECK(query_matches(&q, r, NULL));

    query_init(&q, NULL);
    CHECK(query_where(&q, "due", QF_ABSENT, NULL));
    CHECK(!query_matches(&q, r, NULL));

    record_free(r);
}

TEST(an_empty_field_counts_as_absent)
{
    record *r = rec_of("---\nid: 20260301T101500-0001\ndue:\n---\nnichts\n");
    REQUIRE(r != NULL);

    query q;
    query_init(&q, NULL);
    CHECK(query_where(&q, "due", QF_ABSENT, NULL));
    CHECK(query_matches(&q, r, NULL));

    query_init(&q, NULL);
    CHECK(query_where(&q, "due", QF_PRESENT, NULL));
    CHECK(!query_matches(&q, r, NULL));

    record_free(r);
}

TEST(a_list_field_matches_if_one_entry_does)
{
    record *r = a_task();     /* tags: [Arbeit, Telefon] */
    REQUIRE(r != NULL);

    query q;
    query_init(&q, NULL);
    CHECK(query_where(&q, "tags", QF_EQUALS, "Telefon"));

    /* Der zweite Eintrag. Wer nur den ersten prüfte, käme hier nicht durch -
     * und ein Etikett steht selten an erster Stelle. */
    CHECK(query_matches(&q, r, NULL));

    query_init(&q, NULL);
    CHECK(query_where(&q, "tags", QF_EQUALS, "Privat"));
    CHECK(!query_matches(&q, r, NULL));

    record_free(r);
}

TEST(all_conditions_must_hold)
{
    record *r = a_task();
    REQUIRE(r != NULL);

    query q;
    query_init(&q, NULL);
    CHECK(query_where(&q, "status", QF_EQUALS, "offen"));
    CHECK(query_where(&q, "due", QF_LESS, "2026-04-01"));
    CHECK(query_matches(&q, r, NULL));

    CHECK(query_where(&q, "status", QF_EQUALS, "erledigt"));
    CHECK(!query_matches(&q, r, NULL));

    record_free(r);
}

/* --- Volltext -------------------------------------------------------------------- */

TEST(full_text_searches_body_and_fields)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    record *r = a_task();
    REQUIRE(r != NULL);

    query q;

    query_init(&q, NULL);
    CHECK(query_text(&q, "Lieferung"));      /* steht im Körper */
    CHECK(query_matches(&q, r, c));

    query_init(&q, NULL);
    CHECK(query_text(&q, "Telefon"));        /* steht in einem Listenfeld */
    CHECK(query_matches(&q, r, c));

    query_init(&q, NULL);
    CHECK(query_text(&q, "2026-03-15"));     /* steht in einem Skalarfeld */
    CHECK(query_matches(&q, r, c));

    query_init(&q, NULL);
    CHECK(query_text(&q, "Hamburg"));
    CHECK(!query_matches(&q, r, c));

    record_free(r);
    collate_free(c);
}

TEST(full_text_wants_all_words)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    record *r = a_task();
    REQUIRE(r != NULL);

    query q;
    query_init(&q, NULL);
    CHECK(query_text(&q, "Koln Lieferung"));   /* beide da, in beliebiger Ordnung */
    CHECK(query_matches(&q, r, c));

    query_init(&q, NULL);
    CHECK(query_text(&q, "Koln Hamburg"));     /* eines fehlt */
    CHECK(!query_matches(&q, r, c));

    /* Mehrfache Leerzeichen und Ränder dürfen nichts ändern. */
    query_init(&q, NULL);
    CHECK(query_text(&q, "  Koln   Lieferung  "));
    CHECK(query_matches(&q, r, c));

    record_free(r);
    collate_free(c);
}

/* --- Sortieren --------------------------------------------------------------------- */

TEST(sorting_uses_the_collation)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    record *a = rec_of("---\nid: 1\nname: Mulde\n---\n");
    record *b = rec_of("---\nid: 2\nname: Müller\n---\n");
    record *d = rec_of("---\nid: 3\nname: Multi\n---\n");
    REQUIRE(a && b && d);

    query q;
    query_init(&q, NULL);
    CHECK(query_order(&q, "name", false));

    CHECK(query_compare(&q, a, b, c) < 0);
    CHECK(query_compare(&q, b, d, c) < 0);

    /* Ohne Tabelle stünde Müller hinter Multi, weil das erste Byte von ü
     * größer ist als jeder Buchstabe. */
    CHECK(query_compare(&q, b, d, NULL) > 0);

    record_free(a);
    record_free(b);
    record_free(d);
    collate_free(c);
}

TEST(descending_turns_the_order_around)
{
    record *a = rec_of("---\nid: 1\nname: Anton\n---\n");
    record *b = rec_of("---\nid: 2\nname: Berta\n---\n");
    REQUIRE(a && b);

    query q;
    query_init(&q, NULL);
    CHECK(query_order(&q, "name", false));
    CHECK(query_compare(&q, a, b, NULL) < 0);

    CHECK(query_order(&q, "name", true));
    CHECK(query_compare(&q, a, b, NULL) > 0);

    record_free(a);
    record_free(b);
}

TEST(records_without_the_sort_field_go_last_in_both_directions)
{
    record *has  = rec_of("---\nid: 1\ndue: 2026-03-15\n---\n");
    record *none = rec_of("---\nid: 2\n---\n");
    REQUIRE(has && none);

    query q;
    query_init(&q, NULL);
    CHECK(query_order(&q, "due", false));
    CHECK(query_compare(&q, has, none, NULL) < 0);

    /* Und auch umgekehrt sortiert bleibt der leere hinten. Eine Aufgabe ohne
     * Fälligkeit gehört nicht an den Anfang, nur weil jemand die Richtung
     * umdreht. */
    CHECK(query_order(&q, "due", true));
    CHECK(query_compare(&q, has, none, NULL) < 0);

    /* Zwei leere sind gleich. */
    record *none2 = rec_of("---\nid: 3\n---\n");
    REQUIRE(none2 != NULL);
    CHECK_EQ(query_compare(&q, none, none2, NULL), 0);

    record_free(has);
    record_free(none);
    record_free(none2);
}

TEST(without_a_sort_field_the_id_decides)
{
    /* Die Kennung ist zeitsortiert (record.h), also ist das zugleich die
     * zeitliche Reihenfolge - ohne dass irgendwo ein Datum verglichen wird. */
    record *older = rec_of("---\nid: 20260301T101500-0001\n---\n");
    record *newer = rec_of("---\nid: 20260415T090000-0001\n---\n");
    REQUIRE(older && newer);

    query q;
    query_init(&q, NULL);
    CHECK_STR(q.order_field, "");
    CHECK(query_compare(&q, older, newer, NULL) < 0);

    record_free(older);
    record_free(newer);
}

/* --- Eine ganze Ergebnisliste ------------------------------------------------------- */

static const query   *g_q;
static const collate *g_sort;

static int cmp_thunk(const void *a, const void *b)
{
    return query_compare(g_q, *(record *const *)a, *(record *const *)b, g_sort);
}

TEST(filtering_and_sorting_a_whole_set)
{
    collate *sc = sort_table();
    collate *fc = search_table();
    REQUIRE(sc && fc);

    record *all[5];
    all[0] = rec_of("---\nid: 1\nname: Zander\nstatus: offen\n---\n");
    all[1] = rec_of("---\nid: 2\nname: Öhler\nstatus: offen\n---\n");
    all[2] = rec_of("---\nid: 3\nname: Apfel\nstatus: erledigt\n---\n");
    all[3] = rec_of("---\nid: 4\nname: Müller\nstatus: offen\n---\n");
    all[4] = rec_of("---\nid: 5\nname: Mulde\nstatus: offen\n---\n");
    for (int i = 0; i < 5; i++) REQUIRE(all[i] != NULL);

    query q;
    query_init(&q, "Aufgaben");
    CHECK(query_where(&q, "status", QF_EQUALS, "offen"));
    CHECK(query_order(&q, "name", false));

    record *hits[5];
    int     n = 0;
    for (int i = 0; i < 5; i++)
        if (query_matches(&q, all[i], fc)) hits[n++] = all[i];

    CHECK_EQ(n, 4);

    g_q = &q; g_sort = sc;
    qsort(hits, (size_t)n, sizeof hits[0], cmp_thunk);
    g_q = NULL; g_sort = NULL;

    const char *want[] = { "Mulde", "Müller", "Öhler", "Zander" };
    for (int i = 0; i < n; i++)
        CHECK_STR(frontmatter_get(record_fields(hits[i]), "name"), want[i]);

    for (int i = 0; i < 5; i++) record_free(all[i]);
    collate_free(sc);
    collate_free(fc);
}

int main(void)
{
    RUN(a_fresh_query_matches_everything);
    RUN(too_many_conditions_are_refused_not_dropped);
    RUN(values_that_are_too_long_are_refused_not_truncated);
    RUN(a_condition_without_a_field_is_refused);
    RUN(limit_and_offset_never_go_negative);

    RUN(equals_compares_character_by_character);
    RUN(contains_folds_both_sides);
    RUN(prefix_anchors_at_the_start_of_the_field);
    RUN(dates_compare_as_text_because_they_are_iso);
    RUN(present_and_absent_ask_about_the_field_itself);
    RUN(an_empty_field_counts_as_absent);
    RUN(a_list_field_matches_if_one_entry_does);
    RUN(all_conditions_must_hold);

    RUN(full_text_searches_body_and_fields);
    RUN(full_text_wants_all_words);

    RUN(sorting_uses_the_collation);
    RUN(descending_turns_the_order_around);
    RUN(records_without_the_sort_field_go_last_in_both_directions);
    RUN(without_a_sort_field_the_id_decides);
    RUN(filtering_and_sorting_a_whole_set);

    return test_summary();
}
