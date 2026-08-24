/* Der SQLite-Index, siehe store/index.h.
 *
 * Der wichtigste Test dieser Datei ist der letzte: Index wegwerfen, neu
 * aufbauen, dieselben Antworten. Er ist die Prüfung auf D-3 - ein abgeleiteter
 * Index darf nichts kosten, wenn man ihn löscht. Alles davor prüft, dass er
 * überhaupt dieselben Antworten gibt wie der Weg ohne ihn.
 */
#include "test.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/collate.h"
#include "store/index.h"
#include "store/query.h"
#include "store/record.h"
#include "store/vault.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* --- Gerüst -------------------------------------------------------------------
 *
 * Wie in test_vault.c: das Anlegen und Wegräumen der Testverzeichnisse ist
 * Gerüst und darf die Standardbibliothek direkt benutzen. */

static void temp_root(char *buf, size_t n)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    size_t len = strlen(dir);
    while (len > 1 && dir[len - 1] == '/') len--;
    snprintf(buf, n, "%.*s/pda_index_test", (int)len, dir);
}

static void rmrf(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return;

    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                char child[700];
                snprintf(child, sizeof child, "%s/%s", path, e->d_name);
                rmrf(child);
            }
            closedir(d);
        }
        rmdir(path);
    } else {
        unlink(path);
    }
}

static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static collate *g_sort;
static collate *g_search;

static bool load_tables(void)
{
    char path[512], err[256] = "";

    snprintf(path, sizeof path, "%s/lang/de.sort", PDA_DATA_DIR);
    g_sort = collate_load(path, err, sizeof err);

    snprintf(path, sizeof path, "%s/collate/search.fold", PDA_DATA_DIR);
    g_search = collate_load(path, err, sizeof err);

    if (!g_sort || !g_search) printf("  Tabellen nicht ladbar: %s\n", err);
    return g_sort && g_search;
}

static void free_tables(void)
{
    collate_free(g_sort);
    collate_free(g_search);
    g_sort = g_search = NULL;
}

static record *rec_of(const char *text)
{
    char err[256] = "";
    return record_parse(text, strlen(text), "test", err, sizeof err);
}

/* Vier Datensätze, wie sie im Adressbuch stünden. Die Namen sind mit Bedacht
 * gewählt: sie fallen beim Sortieren auseinander, sobald die Faltung fehlt. */
static const char *const PEOPLE[] = {
    "---\nid: 20260101T090000-0001\nname: Zander\ncity: Köln\ntags: [Arbeit]\n---\n"
    "Kollege aus dem Vertrieb.\n",

    "---\nid: 20260102T090000-0001\nname: Öhler\ncity: Aachen\n---\n"
    "Nachbarin.\n",

    "---\nid: 20260103T090000-0001\nname: Müller\ncity: Köln\ntags: [Arbeit, Telefon]\n---\n"
    "Wegen der Lieferung.\n",

    "---\nid: 20260104T090000-0001\nname: Mulde\ncity: Bonn\n---\n"
    "Bekannt aus dem Verein.\n",
};

static const char *const IDS[] = {
    "20260101T090000-0001", "20260102T090000-0001",
    "20260103T090000-0001", "20260104T090000-0001",
};

static index_db *filled_index(void)
{
    char err[256] = "";
    index_db *ix = index_open(":memory:", g_sort, g_search, err, sizeof err);
    if (!ix) { printf("  Index nicht offen: %s\n", err); return NULL; }

    for (int i = 0; i < 4; i++) {
        record *r = rec_of(PEOPLE[i]);
        if (!r) { printf("  Datensatz %d nicht lesbar\n", i); index_close(ix); return NULL; }
        bool ok = index_put(ix, "Kontakte", IDS[i], r, err, sizeof err);
        record_free(r);
        if (!ok) { printf("  Eintragen: %s\n", err); index_close(ix); return NULL; }
    }
    return ix;
}

/* Führt q aus und liefert die Anzahl; die Kennungen landen in got. */
static int run(index_db *ix, const query *q, char (*got)[RECORD_ID_LEN + 1], int cap)
{
    char err[256] = "";
    int  n = -1;
    if (!index_query(ix, q, got, cap, &n, err, sizeof err)) {
        printf("  Abfrage: %s\n", err);
        return -1;
    }
    return n;
}

/* --- Grundlagen ---------------------------------------------------------------- */

TEST(an_empty_index_answers_with_nothing)
{
    REQUIRE(load_tables());

    char      err[256] = "";
    index_db *ix       = index_open(":memory:", g_sort, g_search, err, sizeof err);
    REQUIRE(ix != NULL);

    query q;
    query_init(&q, "Kontakte");

    char got[8][RECORD_ID_LEN + 1];
    CHECK_EQ(run(ix, &q, got, 8), 0);

    index_close(ix);
    free_tables();
}

TEST(everything_that_went_in_comes_out)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    query_init(&q, "Kontakte");

    char got[8][RECORD_ID_LEN + 1];
    CHECK_EQ(run(ix, &q, got, 8), 4);

    index_close(ix);
    free_tables();
}

TEST(putting_the_same_id_twice_replaces_it)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    char    err[256] = "";
    record *r = rec_of("---\nid: 20260101T090000-0001\nname: Zimmermann\n---\nneu\n");
    REQUIRE(r != NULL);
    CHECK(index_put(ix, "Kontakte", IDS[0], r, err, sizeof err));
    record_free(r);

    query q;
    query_init(&q, "Kontakte");

    char got[8][RECORD_ID_LEN + 1];
    CHECK_EQ(run(ix, &q, got, 8), 4);      /* nicht fünf */

    /* Und der alte Name findet nichts mehr. */
    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_EQUALS, "Zander"));
    CHECK_EQ(run(ix, &q, got, 8), 0);

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_EQUALS, "Zimmermann"));
    CHECK_EQ(run(ix, &q, got, 8), 1);

    index_close(ix);
    free_tables();
}

TEST(removing_takes_it_out_of_all_three_tables)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    char err[256] = "";
    CHECK(index_remove(ix, "Kontakte", IDS[0], err, sizeof err));

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK_EQ(run(ix, &q, got, 8), 3);

    /* Auch über ein Feld nicht mehr zu finden ... */
    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_EQUALS, "Zander"));
    CHECK_EQ(run(ix, &q, got, 8), 0);

    /* ... und auch nicht über den Volltext. Die Volltexttabelle ist eine
     * eigene; wer nur die Feldtabelle räumt, lässt hier eine Leiche zurück. */
    query_init(&q, "Kontakte");
    CHECK(query_text(&q, "Vertrieb"));
    CHECK_EQ(run(ix, &q, got, 8), 0);

    /* Zweimal löschen ist kein Fehler. */
    CHECK(index_remove(ix, "Kontakte", IDS[0], err, sizeof err));

    index_close(ix);
    free_tables();
}

/* --- Bedingungen ----------------------------------------------------------------- */

TEST(conditions_narrow_the_result)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "city", QF_EQUALS, "Köln"));
    CHECK_EQ(run(ix, &q, got, 8), 2);

    /* Zwei Bedingungen sind ein Und. */
    CHECK(query_where(&q, "name", QF_EQUALS, "Müller"));
    CHECK_EQ(run(ix, &q, got, 8), 1);
    CHECK_STR(got[0], IDS[2]);

    index_close(ix);
    free_tables();
}

TEST(contains_finds_across_the_umlaut)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    /* Der Kern der Sache: „muller" findet „Müller", weil beim Eintragen schon
     * gefaltet abgelegt wurde. */
    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_CONTAINS, "muller"));
    CHECK_EQ(run(ix, &q, got, 8), 1);
    CHECK_STR(got[0], IDS[2]);

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "city", QF_CONTAINS, "KOLN"));
    CHECK_EQ(run(ix, &q, got, 8), 2);

    index_close(ix);
    free_tables();
}

TEST(contains_finds_in_the_middle_prefix_does_not)
{
    /* Der Unterschied zwischen den beiden zeigt sich nur, wenn der Suchtext
     * NICHT am Anfang steht. Bei „muller" in „Müller" stünde er das, und
     * beide Bedingungen gäben dieselbe Antwort. */
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "city", QF_CONTAINS, "onn"));    /* in „Bonn" */
    CHECK_EQ(run(ix, &q, got, 8), 1);
    CHECK_STR(got[0], IDS[3]);

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "city", QF_PREFIX, "onn"));
    CHECK_EQ(run(ix, &q, got, 8), 0);

    index_close(ix);
    free_tables();
}

TEST(an_empty_field_counts_as_absent)
{
    /* Ein Feld, das dasteht und leer ist, ist für die Suche keins. Sonst
     * fände „hat ein Fälligkeitsdatum" jede Aufgabe, in deren Datei die Zeile
     * steht - auch die ohne Datum. */
    REQUIRE(load_tables());

    char      err[256] = "";
    index_db *ix       = index_open(":memory:", g_sort, g_search, err, sizeof err);
    REQUIRE(ix != NULL);

    record *leer = rec_of("---\nid: 20260201T090000-0001\nname: Leer\ndue:\n---\nnichts\n");
    record *voll = rec_of("---\nid: 20260202T090000-0001\nname: Voll\ndue: 2026-03-15\n---\netwas\n");
    REQUIRE(leer && voll);

    CHECK(index_put(ix, "Aufgaben", "20260201T090000-0001", leer, err, sizeof err));
    CHECK(index_put(ix, "Aufgaben", "20260202T090000-0001", voll, err, sizeof err));
    record_free(leer);
    record_free(voll);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Aufgaben");
    CHECK(query_where(&q, "due", QF_PRESENT, NULL));
    CHECK_EQ(run(ix, &q, got, 8), 1);
    CHECK_STR(got[0], "20260202T090000-0001");

    query_init(&q, "Aufgaben");
    CHECK(query_where(&q, "due", QF_ABSENT, NULL));
    CHECK_EQ(run(ix, &q, got, 8), 1);
    CHECK_STR(got[0], "20260201T090000-0001");

    index_close(ix);
    free_tables();
}

TEST(comparisons_work_on_iso_dates)
{
    REQUIRE(load_tables());

    char      err[256] = "";
    index_db *ix       = index_open(":memory:", g_sort, g_search, err, sizeof err);
    REQUIRE(ix != NULL);

    static const char *const tasks[] = {
        "---\nid: 20260201T090000-0001\ndue: 2026-01-10\n---\nfrueh\n",
        "---\nid: 20260202T090000-0001\ndue: 2026-03-15\n---\nmitte\n",
        "---\nid: 20260203T090000-0001\ndue: 2026-12-24\n---\nspaet\n",
    };
    static const char *const tids[] = {
        "20260201T090000-0001", "20260202T090000-0001", "20260203T090000-0001",
    };

    for (int i = 0; i < 3; i++) {
        record *r = rec_of(tasks[i]);
        REQUIRE(r != NULL);
        CHECK(index_put(ix, "Aufgaben", tids[i], r, err, sizeof err));
        record_free(r);
    }

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Aufgaben");
    CHECK(query_where(&q, "due", QF_LESS, "2026-04-01"));
    CHECK(query_order(&q, "due", false));
    CHECK_EQ(run(ix, &q, got, 8), 2);
    CHECK_STR(got[0], tids[0]);
    CHECK_STR(got[1], tids[1]);

    query_init(&q, "Aufgaben");
    CHECK(query_where(&q, "due", QF_GREATER, "2026-04-01"));
    CHECK_EQ(run(ix, &q, got, 8), 1);
    CHECK_STR(got[0], tids[2]);

    /* Der Jahreswechsel: eine Vergleichsart, die nur auf Tag und Monat
     * schaut, käme hier auf etwas anderes. */
    query_init(&q, "Aufgaben");
    CHECK(query_where(&q, "due", QF_GREATER, "2025-12-31"));
    CHECK_EQ(run(ix, &q, got, 8), 3);

    index_close(ix);
    free_tables();
}

TEST(a_percent_sign_is_a_character_not_a_wildcard)
{
    /* Hier fiele eine LIKE-Umsetzung ohne Maskierung auf: „%" würde alles
     * finden statt nichts. */
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_CONTAINS, "%"));
    CHECK_EQ(run(ix, &q, got, 8), 0);

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_CONTAINS, "_"));
    CHECK_EQ(run(ix, &q, got, 8), 0);

    index_close(ix);
    free_tables();
}

TEST(an_apostrophe_is_a_character_not_a_syntax_error)
{
    REQUIRE(load_tables());

    char      err[256] = "";
    index_db *ix       = index_open(":memory:", g_sort, g_search, err, sizeof err);
    REQUIRE(ix != NULL);

    record *r = rec_of("---\nid: 20260105T090000-0001\nname: O'Brien\n---\nIrisch.\n");
    REQUIRE(r != NULL);
    CHECK(index_put(ix, "Kontakte", "20260105T090000-0001", r, err, sizeof err));
    record_free(r);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_EQUALS, "O'Brien"));
    CHECK_EQ(run(ix, &q, got, 8), 1);

    index_close(ix);
    free_tables();
}

TEST(prefix_present_and_absent_work_like_without_the_index)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_PREFIX, "mul"));
    CHECK_EQ(run(ix, &q, got, 8), 2);      /* Mulde und Müller */

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "tags", QF_PRESENT, NULL));
    CHECK_EQ(run(ix, &q, got, 8), 2);      /* Zander und Müller */

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "tags", QF_ABSENT, NULL));
    CHECK_EQ(run(ix, &q, got, 8), 2);      /* Öhler und Mulde */

    index_close(ix);
    free_tables();
}

TEST(a_list_field_matches_if_one_entry_does)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    /* „Telefon" ist der zweite Eintrag. Legte der Index nur den ersten ab,
     * fände er hier nichts. */
    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "tags", QF_EQUALS, "Telefon"));
    CHECK_EQ(run(ix, &q, got, 8), 1);
    CHECK_STR(got[0], IDS[2]);

    index_close(ix);
    free_tables();
}

/* --- Volltext --------------------------------------------------------------------- */

TEST(full_text_finds_body_and_fields)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_text(&q, "Lieferung"));
    CHECK_EQ(run(ix, &q, got, 8), 1);

    query_init(&q, "Kontakte");
    CHECK(query_text(&q, "Aachen"));       /* nur in einem Feld */
    CHECK_EQ(run(ix, &q, got, 8), 1);

    /* FTS5 faltet mit `remove_diacritics 2` selbst - dieselbe Regel wie
     * data/collate/search.fold. */
    query_init(&q, "Kontakte");
    CHECK(query_text(&q, "Koln"));
    CHECK_EQ(run(ix, &q, got, 8), 2);

    index_close(ix);
    free_tables();
}

TEST(full_text_wants_all_words)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_text(&q, "Koln Lieferung"));
    CHECK_EQ(run(ix, &q, got, 8), 1);

    query_init(&q, "Kontakte");
    CHECK(query_text(&q, "Koln Verein"));
    CHECK_EQ(run(ix, &q, got, 8), 0);

    index_close(ix);
    free_tables();
}

TEST(a_quote_in_the_search_text_does_not_break_the_query)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_text(&q, "\"Lieferung"));

    /* Es darf keinen Fehler geben; ob etwas gefunden wird, ist zweitrangig. */
    CHECK(run(ix, &q, got, 8) >= 0);

    index_close(ix);
    free_tables();
}

/* --- Sortieren ---------------------------------------------------------------------- */

TEST(sorting_follows_din_5007)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    query_init(&q, "Kontakte");
    CHECK(query_order(&q, "name", false));

    char got[8][RECORD_ID_LEN + 1];
    CHECK_EQ(run(ix, &q, got, 8), 4);

    /* Mulde, Müller, Öhler, Zander. Ohne die abgelegte Sortierfassung stünden
     * Müller und Öhler hinter Zander, weil ihr erstes Byte größer ist. */
    CHECK_STR(got[0], IDS[3]);
    CHECK_STR(got[1], IDS[2]);
    CHECK_STR(got[2], IDS[1]);
    CHECK_STR(got[3], IDS[0]);

    index_close(ix);
    free_tables();
}

TEST(descending_turns_the_order_around)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    query_init(&q, "Kontakte");
    CHECK(query_order(&q, "name", true));

    char got[8][RECORD_ID_LEN + 1];
    CHECK_EQ(run(ix, &q, got, 8), 4);
    CHECK_STR(got[0], IDS[0]);
    CHECK_STR(got[3], IDS[3]);

    index_close(ix);
    free_tables();
}

TEST(records_without_the_sort_field_go_last_in_both_directions)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    /* Nach „tags" sortieren: zwei Datensätze haben keine. */
    query_init(&q, "Kontakte");
    CHECK(query_order(&q, "tags", false));
    CHECK_EQ(run(ix, &q, got, 8), 4);
    CHECK(strcmp(got[0], IDS[1]) != 0 && strcmp(got[0], IDS[3]) != 0);

    query_init(&q, "Kontakte");
    CHECK(query_order(&q, "tags", true));
    CHECK_EQ(run(ix, &q, got, 8), 4);
    CHECK(strcmp(got[0], IDS[1]) != 0 && strcmp(got[0], IDS[3]) != 0);

    index_close(ix);
    free_tables();
}

TEST(limit_and_offset_cut_a_window_out)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    query_init(&q, "Kontakte");
    CHECK(query_order(&q, "name", false));
    query_limit(&q, 2, 1);

    char got[8][RECORD_ID_LEN + 1];
    CHECK_EQ(run(ix, &q, got, 8), 2);
    CHECK_STR(got[0], IDS[2]);   /* Müller */
    CHECK_STR(got[1], IDS[1]);   /* Öhler */

    index_close(ix);
    free_tables();
}

TEST(more_hits_than_room_are_cut_off_not_an_error)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query q;
    query_init(&q, "Kontakte");
    CHECK(query_order(&q, "name", false));

    char got[2][RECORD_ID_LEN + 1];
    CHECK_EQ(run(ix, &q, got, 2), 2);
    CHECK_STR(got[0], IDS[3]);

    index_close(ix);
    free_tables();
}

TEST(the_collection_separates_the_answers)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    char    err[256] = "";
    record *r = rec_of("---\nid: 20260106T090000-0001\nname: Müller\n---\nAufgabe.\n");
    REQUIRE(r != NULL);
    CHECK(index_put(ix, "Aufgaben", "20260106T090000-0001", r, err, sizeof err));
    record_free(r);

    query q;
    char  got[8][RECORD_ID_LEN + 1];

    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "name", QF_EQUALS, "Müller"));
    CHECK_EQ(run(ix, &q, got, 8), 1);
    CHECK_STR(got[0], IDS[2]);

    /* Ohne Sammlung: beide. */
    query_init(&q, NULL);
    CHECK(query_where(&q, "name", QF_EQUALS, "Müller"));
    CHECK_EQ(run(ix, &q, got, 8), 2);

    index_close(ix);
    free_tables();
}

/* --- Der abgeleitete Index (D-3) ------------------------------------------------------
 *
 * Der Test, um den es geht. Ein Vault wird gefüllt, der Index daraus gebaut,
 * eine Abfrage gestellt. Dann wird die Indexdatei gelöscht und alles
 * wiederholt. Kommt etwas anderes heraus, war der Index keine Ableitung,
 * sondern hielt Daten, die sonst nirgends stehen.
 */
TEST(throwing_the_index_away_costs_nothing)
{
    REQUIRE(load_tables());

    char root[600], dbpath[700], err[256] = "";
    temp_root(root, sizeof root);
    rmrf(root);
    snprintf(dbpath, sizeof dbpath, "%s.db", root);
    rmrf(dbpath);

    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    for (int i = 0; i < 4; i++) {
        record *r = rec_of(PEOPLE[i]);
        REQUIRE(r != NULL);
        char id[RECORD_ID_LEN + 1];
        CHECK(vault_save(v, "Kontakte", r, id, sizeof id, err, sizeof err));
        record_free(r);
    }

    const char *colls[] = { "Kontakte" };

    query q;
    query_init(&q, "Kontakte");
    CHECK(query_where(&q, "city", QF_CONTAINS, "koln"));
    CHECK(query_order(&q, "name", false));

    char before[8][RECORD_ID_LEN + 1];
    int  n_before;
    {
        index_db *ix = index_open(dbpath, g_sort, g_search, err, sizeof err);
        REQUIRE(ix != NULL);
        CHECK(index_rebuild(ix, v, colls, 1, err, sizeof err));
        n_before = run(ix, &q, before, 8);
        index_close(ix);
    }
    CHECK_EQ(n_before, 2);
    CHECK(file_exists(dbpath));

    /* Und jetzt weg damit. */
    rmrf(dbpath);
    CHECK(!file_exists(dbpath));

    char after[8][RECORD_ID_LEN + 1];
    int  n_after;
    {
        index_db *ix = index_open(dbpath, g_sort, g_search, err, sizeof err);
        REQUIRE(ix != NULL);
        CHECK(index_rebuild(ix, v, colls, 1, err, sizeof err));
        n_after = run(ix, &q, after, 8);
        index_close(ix);
    }

    CHECK_EQ(n_after, n_before);
    for (int i = 0; i < n_after; i++) CHECK_STR(after[i], before[i]);

    vault_close(v);
    rmrf(root);
    rmrf(dbpath);
    free_tables();
}

TEST(rebuilding_forgets_what_is_no_longer_there)
{
    REQUIRE(load_tables());

    char root[600], err[256] = "";
    temp_root(root, sizeof root);
    rmrf(root);

    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    char ids[4][RECORD_ID_LEN + 1];
    for (int i = 0; i < 4; i++) {
        record *r = rec_of(PEOPLE[i]);
        REQUIRE(r != NULL);
        CHECK(vault_save(v, "Kontakte", r, ids[i], sizeof ids[i], err, sizeof err));
        record_free(r);
    }

    const char *colls[] = { "Kontakte" };
    index_db   *ix      = index_open(":memory:", g_sort, g_search, err, sizeof err);
    REQUIRE(ix != NULL);
    CHECK(index_rebuild(ix, v, colls, 1, err, sizeof err));

    query q;
    query_init(&q, "Kontakte");
    char got[8][RECORD_ID_LEN + 1];
    CHECK_EQ(run(ix, &q, got, 8), 4);

    /* Einen aus dem Vault löschen und neu aufbauen. */
    CHECK(vault_delete(v, "Kontakte", ids[0], err, sizeof err));
    CHECK(index_rebuild(ix, v, colls, 1, err, sizeof err));
    CHECK_EQ(run(ix, &q, got, 8), 3);

    index_close(ix);
    vault_close(v);
    rmrf(root);
    free_tables();
}

/* --- Beide Wege müssen dasselbe sagen -------------------------------------------------
 *
 * Der Index ist der schnelle Weg, query_matches() der, der immer geht. Geben
 * sie verschiedene Antworten, ist einer von beiden falsch - und welcher, merkt
 * man erst, wenn ein Nutzer sich wundert.
 */
TEST(the_index_and_the_direct_scan_agree)
{
    REQUIRE(load_tables());
    index_db *ix = filled_index();
    REQUIRE(ix != NULL);

    query qs[6];
    for (int i = 0; i < 6; i++) query_init(&qs[i], "Kontakte");

    CHECK(query_where(&qs[0], "city", QF_EQUALS, "Köln"));
    CHECK(query_where(&qs[1], "name", QF_CONTAINS, "muller"));
    CHECK(query_where(&qs[2], "name", QF_PREFIX, "mul"));
    CHECK(query_where(&qs[3], "tags", QF_PRESENT, NULL));
    CHECK(query_where(&qs[4], "tags", QF_ABSENT, NULL));
    CHECK(query_text(&qs[5], "Koln"));

    for (int k = 0; k < 6; k++) {
        char got[8][RECORD_ID_LEN + 1];
        int  n = run(ix, &qs[k], got, 8);
        CHECK(n >= 0);

        int direct = 0;
        for (int i = 0; i < 4; i++) {
            record *r = rec_of(PEOPLE[i]);
            REQUIRE(r != NULL);
            if (query_matches(&qs[k], r, g_search)) direct++;
            record_free(r);
        }

        if (n != direct)
            printf("  Abfrage %d: Index %d, direkt %d\n", k, n, direct);
        CHECK_EQ(n, direct);
    }

    index_close(ix);
    free_tables();
}

int main(void)
{
    RUN(an_empty_index_answers_with_nothing);
    RUN(everything_that_went_in_comes_out);
    RUN(putting_the_same_id_twice_replaces_it);
    RUN(removing_takes_it_out_of_all_three_tables);

    RUN(conditions_narrow_the_result);
    RUN(contains_finds_across_the_umlaut);
    RUN(contains_finds_in_the_middle_prefix_does_not);
    RUN(an_empty_field_counts_as_absent);
    RUN(comparisons_work_on_iso_dates);
    RUN(a_percent_sign_is_a_character_not_a_wildcard);
    RUN(an_apostrophe_is_a_character_not_a_syntax_error);
    RUN(prefix_present_and_absent_work_like_without_the_index);
    RUN(a_list_field_matches_if_one_entry_does);

    RUN(full_text_finds_body_and_fields);
    RUN(full_text_wants_all_words);
    RUN(a_quote_in_the_search_text_does_not_break_the_query);

    RUN(sorting_follows_din_5007);
    RUN(descending_turns_the_order_around);
    RUN(records_without_the_sort_field_go_last_in_both_directions);
    RUN(limit_and_offset_cut_a_window_out);
    RUN(more_hits_than_room_are_cut_off_not_an_error);
    RUN(the_collection_separates_the_answers);

    RUN(throwing_the_index_away_costs_nothing);
    RUN(rebuilding_forgets_what_is_no_longer_there);
    RUN(the_index_and_the_direct_scan_agree);

    return test_summary();
}
