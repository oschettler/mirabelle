/* Sortieren und Suchen, siehe core/collate.h.
 *
 * Die Tabellen kommen aus data/, nicht aus dem Test: was hier geprüft wird,
 * ist die Ordnung, die der Nutzer sieht, nicht eine für den Test erfundene.
 * Ein Fehler in data/lang/de.sort soll hier auffallen und nicht erst im
 * Adressbuch.
 */
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/collate.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

static collate *load(const char *rel)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/%s", PDA_DATA_DIR, rel);

    collate *c = collate_load(path, err, sizeof err);
    if (!c) printf("  nicht ladbar: %s\n", err);
    return c;
}

static collate *sort_table(void)   { return load("lang/de.sort"); }
static collate *search_table(void) { return load("collate/search.fold"); }

/* --- Die Ordnung, die ein Mensch erwartet ------------------------------------ */

TEST(umlauts_sort_like_their_base_letter)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    /* Das Beispiel aus DESIGN.md: Müller steht zwischen Mulde und Multi. */
    CHECK(collate_compare(c, "Mulde", "Müller") < 0);
    CHECK(collate_compare(c, "Müller", "Multi") < 0);

    collate_free(c);
}

TEST(sharp_s_counts_as_two_letters)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    /* Straße zählt wie Strasse und steht damit vor Strauch. */
    CHECK_EQ(collate_compare(c, "Straße", "Strasse"), 0 - (strcmp("Straße", "Strasse") < 0 ? 1 : -1));
    CHECK(collate_compare(c, "Straße", "Strauch") < 0);
    CHECK(collate_compare(c, "Strasse", "Strauch") < 0);

    collate_free(c);
}

TEST(case_does_not_decide_the_order)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    /* Ohne Kleinschreibung stünde jedes große Z vor jedem kleinen a - das ist
     * die ASCII-Reihenfolge, nicht die eines Registers. */
    CHECK(collate_compare(c, "Zebra", "apfel") > 0);
    CHECK(collate_compare(c, "Apfel", "apfel") != 0);   /* aber unterscheidbar */

    collate_free(c);
}

TEST(equal_after_folding_still_has_a_fixed_order)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    /* Muller und Müller falten gleich. Gäbe es keinen zweiten Schritt, wäre
     * ihre Reihenfolge davon abhängig, in welcher sie ankamen - eine Liste
     * sähe nach jedem Sortieren anders aus. */
    int ab = collate_compare(c, "Muller", "Müller");
    int ba = collate_compare(c, "Müller", "Muller");

    CHECK(ab != 0);
    CHECK_EQ(ab, -ba);

    collate_free(c);
}

TEST(a_string_equals_itself)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    CHECK_EQ(collate_compare(c, "Müller", "Müller"), 0);
    CHECK_EQ(collate_compare(c, "", ""), 0);
    CHECK(collate_compare(c, "", "a") < 0);

    collate_free(c);
}

TEST(foreign_names_sort_under_their_base_letter)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    /* Ohne Tabelleneintrag stünde é hinter z, weil sein erstes Byte 0xC3
     * größer ist als jeder ASCII-Buchstabe. Im Adressbuch wäre das falsch. */
    CHECK(collate_compare(c, "Café", "Cafz") < 0);
    CHECK(collate_compare(c, "Ångström", "Berg") < 0);
    /* ø zählt wie o; dann entscheidet die vierte Stelle, e vor g. */
    CHECK(collate_compare(c, "Søren", "Sorge") < 0);
    CHECK(collate_compare(c, "Søren", "Sorte") < 0);
    CHECK(collate_compare(c, "Søren", "Sonne") > 0);

    collate_free(c);
}

/* --- Sortieren einer ganzen Liste --------------------------------------------- */

static const collate *g_sort;

static int cmp_thunk(const void *a, const void *b)
{
    return collate_compare(g_sort, *(const char *const *)a, *(const char *const *)b);
}

TEST(a_whole_list_comes_out_in_register_order)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);
    g_sort = c;

    const char *names[] = { "Multi", "Öhler", "Mulde", "Zander",
                            "Müller", "Ähre", "Apfel", "Straße" };
    const int   n       = 8;
    qsort(names, (size_t)n, sizeof names[0], cmp_thunk);

    const char *want[] = { "Ähre", "Apfel", "Mulde", "Müller",
                           "Multi", "Öhler", "Straße", "Zander" };
    for (int i = 0; i < n; i++) CHECK_STR(names[i], want[i]);

    collate_free(c);
    g_sort = NULL;
}

/* --- Suchen -------------------------------------------------------------------- */

TEST(search_finds_in_both_directions)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    /* Der eigentliche Punkt: es wird auf beiden Seiten gefaltet. Faltete man
     * nur die Eingabe, fände Muller zwar Müller, aber nicht umgekehrt. */
    CHECK(collate_contains(c, "Müller", "Muller"));
    CHECK(collate_contains(c, "Muller", "Müller"));
    CHECK(collate_contains(c, "Straße", "strasse"));
    CHECK(collate_contains(c, "Strasse", "STRASSE"));

    collate_free(c);
}

TEST(search_finds_in_the_middle_and_at_the_ends)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    CHECK(collate_contains(c, "Herr Müller aus Köln", "müller"));
    CHECK(collate_contains(c, "Herr Müller aus Köln", "Herr"));
    CHECK(collate_contains(c, "Herr Müller aus Köln", "Koln"));
    CHECK(!collate_contains(c, "Herr Müller aus Köln", "Mueller"));

    collate_free(c);
}

TEST(an_empty_needle_is_everywhere)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    CHECK(collate_contains(c, "irgendwas", ""));
    CHECK(collate_contains(c, "", ""));
    CHECK(!collate_contains(c, "", "a"));

    collate_free(c);
}

TEST(a_needle_longer_than_the_haystack_is_not_found)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    CHECK(!collate_contains(c, "Mül", "Müller"));

    /* Auch dann nicht, wenn der Anfang passt und die Faltung die Längen
     * verschiebt: ß wird zu zwei Zeichen. */
    CHECK(!collate_contains(c, "Stra", "Straße"));

    collate_free(c);
}

TEST(prefix_search_anchors_at_the_start)
{
    collate *c = search_table();
    REQUIRE(c != NULL);

    CHECK(collate_starts_with(c, "Müller", "mul"));
    CHECK(collate_starts_with(c, "Müller", ""));
    CHECK(!collate_starts_with(c, "Herr Müller", "mul"));   /* steht nicht vorn */
    CHECK(collate_contains(c, "Herr Müller", "mul"));       /* aber irgendwo */

    collate_free(c);
}

/* --- Falten als Text ----------------------------------------------------------- */

TEST(folding_writes_the_folded_text)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    char   out[64];
    size_t n = collate_fold(c, "Müller Straße", out, sizeof out);

    CHECK_STR(out, "muller strasse");
    CHECK_EQ(n, strlen("muller strasse"));

    collate_free(c);
}

TEST(folding_into_a_buffer_that_is_too_small_fails_cleanly)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    char out[8];
    memset(out, 'x', sizeof out);

    /* „Straße" wird zu sieben Zeichen und braucht mit Nullbyte acht - der
     * Puffer hat genau acht, also passt es gerade. Ein Zeichen mehr nicht. */
    CHECK_EQ(collate_fold(c, "Straße", out, sizeof out), 7u);
    CHECK_STR(out, "strasse");

    CHECK_EQ(collate_fold(c, "Straßen", out, sizeof out), (size_t)-1);

    /* Und deutlich zu lang. Das ist der Fall, der beim Schreiben über den
     * Puffer hinausliefe; dass er es nicht tut, sieht erst der Sanitizer -
     * am Rückgabewert allein wäre kein Unterschied zu erkennen. */
    CHECK_EQ(collate_fold(c, "Straßenbahnhaltestelle", out, sizeof out), (size_t)-1);

    collate_free(c);
}

TEST(unknown_characters_pass_through)
{
    collate *c = sort_table();
    REQUIRE(c != NULL);

    char out[64];
    CHECK(collate_fold(c, "Wörter (Λ) 42!", out, sizeof out) != (size_t)-1);
    CHECK_STR(out, "worter (Λ) 42!");

    collate_free(c);
}

/* --- Die Tabellen selbst -------------------------------------------------------- */

TEST(the_two_tables_are_not_the_same)
{
    /* Sie sehen sich ähnlich, sind aber für Verschiedenes da. Fiele jemand auf
     * den Gedanken, sie zusammenzulegen, müsste er sich für eine Sprache
     * entscheiden - dieser Test hält den Unterschied fest. */
    collate *s = sort_table();
    collate *f = search_table();
    REQUIRE(s != NULL);
    REQUIRE(f != NULL);

    char a[64], b[64];
    CHECK(collate_fold(s, "ãíû", a, sizeof a) != (size_t)-1);
    CHECK(collate_fold(f, "ãíû", b, sizeof b) != (size_t)-1);

    /* Die Suchtabelle kennt mehr Zeichen als die deutsche Sortiertabelle: für
     * das Sortieren deutscher Namen braucht es sie nicht, für eine großzügige
     * Suche schon. */
    CHECK_STR(b, "aiu");
    CHECK(strcmp(a, b) != 0);

    collate_free(s);
    collate_free(f);
}

/* --- Fehlerhafte Tabellen -------------------------------------------------------- */

static const char *write_temp(const char *name, const char *content)
{
    static char path[512];
    snprintf(path, sizeof path, "/tmp/pda_collate_%s", name);

    FILE *fp = fopen(path, "wb");
    if (!fp) return NULL;
    fputs(content, fp);
    fclose(fp);
    return path;
}

TEST(the_table_beats_the_built_in_lowercasing)
{
    /* Kleinbuchstaben aus Großbuchstaben zu machen ist die einzige Regel im
     * Code. Sie gilt nur, solange die Tabelle schweigt - sonst wäre sie eine
     * Sprachannahme im Programm, und genau die soll es hier nicht geben.
     *
     * Türkisch ist der Fall, an dem das auffällt: dort ist das kleine I zu
     * einem großen I ein ı, nicht ein i. */
    char        err[256] = "";
    const char *path     = write_temp("tuerkisch", "I ı\n");
    REQUIRE(path != NULL);

    collate *c = collate_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    char out[32];
    CHECK(collate_fold(c, "IST", out, sizeof out) != (size_t)-1);
    CHECK_STR(out, "ıst");     /* nicht "ist" */

    collate_free(c);
}

TEST(a_broken_table_says_which_line)
{
    char        err[256] = "";
    const char *path     = write_temp("dopplung", "ä a\n# Kommentar\nä o\n");
    REQUIRE(path != NULL);

    collate *c = collate_load(path, err, sizeof err);
    CHECK(c == NULL);
    CHECK(strstr(err, ":3:") != NULL);     /* die Zeile, die den Fehler macht */
    CHECK(strstr(err, "Zeile 1") != NULL); /* und die, mit der sie kollidiert */

    collate_free(c);
}

TEST(a_line_without_a_replacement_is_refused)
{
    char        err[256] = "";
    const char *path     = write_temp("leer", "ä a\nö\n");
    REQUIRE(path != NULL);

    CHECK(collate_load(path, err, sizeof err) == NULL);
    CHECK(strstr(err, ":2:") != NULL);
}

TEST(a_line_with_only_whitespace_after_the_character_is_refused)
{
    /* Sieht aus wie die Zeile darüber, ist aber ein anderer Weg durch den
     * Parser: hier folgt auf das Zeichen sehr wohl ein Leerzeichen, nur steht
     * danach nichts mehr. Wer nur den einen Fall prüft, lässt den anderen
     * offen - und der ergäbe eine Ersetzung durch nichts. */
    char        err[256] = "";
    const char *path     = write_temp("nur_leerraum", "ä a\nö   \n");
    REQUIRE(path != NULL);

    CHECK(collate_load(path, err, sizeof err) == NULL);
    CHECK(strstr(err, ":2:") != NULL);
}

TEST(a_replacement_that_is_too_long_is_refused)
{
    /* Nicht abschneiden, sondern ablehnen. Eine stillschweigend gekürzte
     * Ersetzung wäre ein Fehler, der erst Monate später als seltsame
     * Sortierreihenfolge auffiele. */
    char        err[256] = "";
    const char *path     = write_temp("zu_lang", "ä aaaaaaaaaaaaaaaaaaaaaaaa\n");
    REQUIRE(path != NULL);

    CHECK(collate_load(path, err, sizeof err) == NULL);
    CHECK(strstr(err, ":1:") != NULL);
}

TEST(more_than_one_character_on_the_left_is_refused)
{
    char        err[256] = "";
    const char *path     = write_temp("zweizeichen", "ae a\n");
    REQUIRE(path != NULL);

    CHECK(collate_load(path, err, sizeof err) == NULL);
    CHECK(strstr(err, ":1:") != NULL);
}

TEST(a_missing_file_says_so)
{
    char err[256] = "";
    CHECK(collate_load("/tmp/gibt-es-nicht-pda", err, sizeof err) == NULL);
    CHECK(strstr(err, "gibt-es-nicht-pda") != NULL);
}

TEST(an_empty_table_is_allowed_and_only_lowercases)
{
    char        err[256] = "";
    const char *path     = write_temp("leer_ganz", "# nur ein Kommentar\n\n");
    REQUIRE(path != NULL);

    collate *c = collate_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    char out[32];
    CHECK(collate_fold(c, "ABCä", out, sizeof out) != (size_t)-1);
    CHECK_STR(out, "abcä");    /* ohne Eintrag bleibt ä, wie es ist */

    collate_free(c);
}

int main(void)
{
    RUN(umlauts_sort_like_their_base_letter);
    RUN(sharp_s_counts_as_two_letters);
    RUN(case_does_not_decide_the_order);
    RUN(equal_after_folding_still_has_a_fixed_order);
    RUN(a_string_equals_itself);
    RUN(foreign_names_sort_under_their_base_letter);
    RUN(a_whole_list_comes_out_in_register_order);

    RUN(search_finds_in_both_directions);
    RUN(search_finds_in_the_middle_and_at_the_ends);
    RUN(an_empty_needle_is_everywhere);
    RUN(a_needle_longer_than_the_haystack_is_not_found);
    RUN(prefix_search_anchors_at_the_start);

    RUN(folding_writes_the_folded_text);
    RUN(folding_into_a_buffer_that_is_too_small_fails_cleanly);
    RUN(unknown_characters_pass_through);
    RUN(the_two_tables_are_not_the_same);

    RUN(the_table_beats_the_built_in_lowercasing);
    RUN(a_broken_table_says_which_line);
    RUN(a_line_without_a_replacement_is_refused);
    RUN(a_line_with_only_whitespace_after_the_character_is_refused);
    RUN(a_replacement_that_is_too_long_is_refused);
    RUN(more_than_one_character_on_the_left_is_refused);
    RUN(a_missing_file_says_so);
    RUN(an_empty_table_is_allowed_and_only_lowercases);

    return test_summary();
}
