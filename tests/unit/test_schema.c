/* Schemata, siehe app/schema.h.
 *
 * Die echten Schemata aus data/schema/ werden mitgeprüft. Ein Tippfehler dort
 * soll hier auffallen und nicht als leere Spalte im Browser.
 */
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/schema.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

static bool load_real(schema *s, const char *name)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/schema/%s.schema", PDA_DATA_DIR, name);

    if (!schema_load(s, path, err, sizeof err)) {
        printf("  %s\n", err);
        return false;
    }
    return true;
}

static const char *write_temp(const char *name, const char *content)
{
    static char path[512];
    snprintf(path, sizeof path, "/tmp/pda_schema_%s.schema", name);

    FILE *fp = fopen(path, "wb");
    if (!fp) return NULL;
    fputs(content, fp);
    fclose(fp);
    return path;
}

/* Ein Schema, das durchgeht - Grundlage für die Fehlerfälle darunter. */
static const char *GOOD =
    "type    task\n"
    "folder  Aufgaben\n"
    "label   app.tasks\n"
    "sort    due\n"
    "columns done title\n"
    "form    title due\n"
    "\n"
    "field title\n"
    "    kind      text\n"
    "    label     field.title\n"
    "    required  yes\n"
    "\n"
    "field due\n"
    "    kind   date\n"
    "    label  field.due\n"
    "\n"
    "field done\n"
    "    kind   bool\n"
    "    label  field.done\n";

static bool load_text(schema *s, const char *name, const char *text,
                      char *err, size_t err_size)
{
    const char *path = write_temp(name, text);
    if (!path) return false;
    return schema_load(s, path, err, err_size);
}

/* --- Die echten Schemata -------------------------------------------------------- */

TEST(the_task_schema_reads)
{
    schema s;
    REQUIRE(load_real(&s, "task"));

    CHECK_STR(s.type, "task");
    CHECK_STR(s.folder, "Aufgaben");
    CHECK_STR(s.label, "app.tasks");
    CHECK_EQ(s.field_count, 6);
    CHECK_EQ(s.column_count, 3);
    CHECK_STR(s.sort, "due");
    CHECK(!s.sort_desc);

    const schema_field *title = schema_field_by_name(&s, "title");
    REQUIRE(title != NULL);
    CHECK_EQ(title->kind, FIELD_TEXT);
    CHECK(title->required);

    const schema_field *prio = schema_field_by_name(&s, "priority");
    REQUIRE(prio != NULL);
    CHECK_EQ(prio->kind, FIELD_CHOICE);
    CHECK_EQ(prio->value_count, 5);
    CHECK_STR(prio->values[0], "1");
    CHECK_STR(prio->values[4], "5");

    CHECK(schema_field_by_name(&s, "gibtesnicht") == NULL);
}

TEST(all_three_applications_are_just_files)
{
    /* Der Punkt von D-7: drei Anwendungen, drei Dateien, kein Programmcode
     * dazwischen. Wenn alle drei laden und verschiedene Felder haben, ist der
     * Beweis geführt - der Browser sieht danach nur noch diese Struktur. */
    schema task, contact, note;
    REQUIRE(load_real(&task, "task"));
    REQUIRE(load_real(&contact, "contact"));
    REQUIRE(load_real(&note, "note"));

    CHECK_STR(task.folder, "Aufgaben");
    CHECK_STR(contact.folder, "Kontakte");
    CHECK_STR(note.folder, "Notizen");

    CHECK(schema_field_by_name(&task, "due") != NULL);
    CHECK(schema_field_by_name(&contact, "due") == NULL);
    CHECK(schema_field_by_name(&contact, "phone") != NULL);

    /* Jede Sammlung hat einen Gemtext-Körper - das ist die eine Gemeinsamkeit,
     * auf die sich der Browser verlassen darf. */
    const schema_field *b;
    b = schema_field_by_name(&task, "body");    CHECK(b && b->kind == FIELD_GEMTEXT);
    b = schema_field_by_name(&contact, "body"); CHECK(b && b->kind == FIELD_GEMTEXT);
    b = schema_field_by_name(&note, "body");    CHECK(b && b->kind == FIELD_GEMTEXT);
}

/* --- Das Format ------------------------------------------------------------------ */

TEST(indentation_decides_where_a_line_belongs)
{
    schema s;
    char   err[256] = "";
    REQUIRE(load_text(&s, "gut", GOOD, err, sizeof err));

    /* „label" steht zweimal in der Datei: einmal oben für die Anwendung,
     * einmal eingerückt für ein Feld. Nur der Einzug unterscheidet sie. */
    CHECK_STR(s.label, "app.tasks");
    CHECK_STR(schema_field_by_name(&s, "title")->label, "field.title");
}

TEST(a_top_level_line_ends_the_open_field)
{
    /* Nach „form" darf keine eingerückte Zeile mehr zu „title" gehören. */
    schema s;
    char   err[256] = "";
    const char *text =
        "type x\nfolder X\nlabel l\ncolumns a\nform a\n"
        "field a\n    kind text\n    label la\n"
        "sort a\n"
        "    kind date\n";

    CHECK(!load_text(&s, "nach_oben", text, err, sizeof err));
    CHECK(strstr(err, "kein Feld begonnen") != NULL);
}

TEST(comments_and_blank_lines_are_ignored)
{
    schema s;
    char   err[256] = "";
    const char *text =
        "# oben ein Kommentar\n"
        "type x   # und einer dahinter\n"
        "\n   \n"
        "folder X\nlabel l\ncolumns a\nform a\n"
        "field a\n"
        "    # auch hier\n"
        "    kind text\n"
        "    label la\n";

    REQUIRE(load_text(&s, "kommentare", text, err, sizeof err));
    CHECK_STR(s.type, "x");
    CHECK_EQ(s.field_count, 1);
}

TEST(the_sort_direction_is_optional)
{
    schema s;
    char   err[256] = "";

    const char *head =
        "type x\nfolder X\nlabel l\ncolumns due\nform due\n"
        "field due\n    kind date\n    label ld\n";

    char text[512];

    snprintf(text, sizeof text, "sort due\n%s", head);
    REQUIRE(load_text(&s, "sort_ohne", text, err, sizeof err));
    CHECK(!s.sort_desc);

    snprintf(text, sizeof text, "sort due asc\n%s", head);
    REQUIRE(load_text(&s, "sort_asc", text, err, sizeof err));
    CHECK(!s.sort_desc);

    snprintf(text, sizeof text, "sort due desc\n%s", head);
    REQUIRE(load_text(&s, "sort_desc", text, err, sizeof err));
    CHECK(s.sort_desc);

    /* Alles andere ist keine Richtung, sondern ein Tippfehler. */
    snprintf(text, sizeof text, "sort due rueckwaerts\n%s", head);
    CHECK(!load_text(&s, "sort_falsch", text, err, sizeof err));
    CHECK(strstr(err, "Richtung") != NULL);
}

/* --- Was abgelehnt wird ------------------------------------------------------------ */

TEST(a_typo_in_columns_is_caught_at_load_time)
{
    /* Der Fehler, um den es geht: „titel" statt „title". Ohne diese Prüfung
     * hätte der Browser eine Spalte, die immer leer ist, und niemand wüsste
     * warum. */
    schema s;
    char   err[256] = "";
    const char *text =
        "type x\nfolder X\nlabel l\ncolumns titel\nform title\n"
        "field title\n    kind text\n    label lt\n";

    CHECK(!load_text(&s, "spalte_tippfehler", text, err, sizeof err));
    CHECK(strstr(err, "titel") != NULL);
    CHECK(strstr(err, "columns") != NULL);
}

TEST(a_typo_in_form_or_sort_is_caught_too)
{
    schema s;
    char   err[256] = "";

    const char *bad_form =
        "type x\nfolder X\nlabel l\ncolumns title\nform titel\n"
        "field title\n    kind text\n    label lt\n";
    CHECK(!load_text(&s, "form_tippfehler", bad_form, err, sizeof err));
    CHECK(strstr(err, "form") != NULL);

    const char *bad_sort =
        "type x\nfolder X\nlabel l\ncolumns title\nform title\nsort titel\n"
        "field title\n    kind text\n    label lt\n";
    CHECK(!load_text(&s, "sort_tippfehler", bad_sort, err, sizeof err));
    CHECK(strstr(err, "sort") != NULL);
}

TEST(an_unknown_key_or_kind_is_refused)
{
    schema s;
    char   err[256] = "";

    const char *bad_key =
        "type x\nfolder X\nlabel l\ncolumns title\nform title\nfarbe rot\n"
        "field title\n    kind text\n    label lt\n";
    CHECK(!load_text(&s, "unbekannt", bad_key, err, sizeof err));
    CHECK(strstr(err, "farbe") != NULL);
    CHECK(strstr(err, ":6:") != NULL);

    const char *bad_kind =
        "type x\nfolder X\nlabel l\ncolumns title\nform title\n"
        "field title\n    kind farbe\n    label lt\n";
    CHECK(!load_text(&s, "unbekannter_typ", bad_kind, err, sizeof err));
    CHECK(strstr(err, "farbe") != NULL);
    CHECK(strstr(err, ":7:") != NULL);
}

TEST(a_choice_without_values_is_refused)
{
    schema s;
    char   err[256] = "";
    const char *text =
        "type x\nfolder X\nlabel l\ncolumns p\nform p\n"
        "field p\n    kind choice\n    label lp\n";

    CHECK(!load_text(&s, "auswahl_leer", text, err, sizeof err));
    CHECK(strstr(err, "values") != NULL);
}

TEST(values_on_something_that_is_not_a_choice_are_refused)
{
    /* Sie täten nichts, stünden aber da - und wer sie schreibt, erwartet, dass
     * sie wirken. Lieber ablehnen als stillschweigend übergehen. */
    schema s;
    char   err[256] = "";
    const char *text =
        "type x\nfolder X\nlabel l\ncolumns t\nform t\n"
        "field t\n    kind text\n    label lt\n    values a b\n";

    CHECK(!load_text(&s, "werte_ohne_auswahl", text, err, sizeof err));
    CHECK(strstr(err, "choice") != NULL);
}

TEST(a_field_without_a_label_is_refused)
{
    schema s;
    char   err[256] = "";
    const char *text =
        "type x\nfolder X\nlabel l\ncolumns t\nform t\n"
        "field t\n    kind text\n";

    CHECK(!load_text(&s, "ohne_label", text, err, sizeof err));
    CHECK(strstr(err, "label") != NULL);
}

TEST(the_head_must_be_complete)
{
    schema s;
    char   err[256] = "";

    const char *no_type =
        "folder X\nlabel l\ncolumns t\nform t\nfield t\n    kind text\n    label lt\n";
    CHECK(!load_text(&s, "ohne_type", no_type, err, sizeof err));
    CHECK(strstr(err, "type") != NULL);

    const char *no_folder =
        "type x\nlabel l\ncolumns t\nform t\nfield t\n    kind text\n    label lt\n";
    CHECK(!load_text(&s, "ohne_folder", no_folder, err, sizeof err));
    CHECK(strstr(err, "folder") != NULL);

    const char *no_label =
        "type x\nfolder X\ncolumns t\nform t\nfield t\n    kind text\n    label lt\n";
    CHECK(!load_text(&s, "ohne_label_oben", no_label, err, sizeof err));
    CHECK(strstr(err, "label") != NULL);

    const char *no_fields = "type x\nfolder X\nlabel l\n";
    CHECK(!load_text(&s, "ohne_felder", no_fields, err, sizeof err));
    CHECK(strstr(err, "Feld") != NULL);
}

TEST(a_duplicate_field_is_refused)
{
    schema s;
    char   err[256] = "";
    const char *text =
        "type x\nfolder X\nlabel l\ncolumns t\nform t\n"
        "field t\n    kind text\n    label lt\n"
        "field t\n    kind date\n    label lt2\n";

    CHECK(!load_text(&s, "doppelt", text, err, sizeof err));
    CHECK(strstr(err, "gibt es schon") != NULL);
}

TEST(an_empty_list_is_refused)
{
    schema s;
    char   err[256] = "";
    const char *text =
        "type x\nfolder X\nlabel l\ncolumns\nform t\n"
        "field t\n    kind text\n    label lt\n";

    CHECK(!load_text(&s, "leere_liste", text, err, sizeof err));
    CHECK(strstr(err, "leer") != NULL);
}

TEST(a_name_that_is_too_long_is_refused_not_truncated)
{
    schema s;
    char   err[256] = "";

    char big[SCHEMA_NAME_MAX + 20];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';

    char text[512];
    snprintf(text, sizeof text,
             "type %s\nfolder X\nlabel l\ncolumns t\nform t\n"
             "field t\n    kind text\n    label lt\n", big);

    CHECK(!load_text(&s, "zu_lang", text, err, sizeof err));
    CHECK(strstr(err, "zu lang") != NULL);
}

TEST(required_no_means_no)
{
    schema s;
    char   err[256] = "";
    const char *text =
        "type x\nfolder X\nlabel l\ncolumns a b\nform a b\n"
        "field a\n    kind text\n    label la\n    required yes\n"
        "field b\n    kind text\n    label lb\n    required no\n";

    REQUIRE(load_text(&s, "pflicht", text, err, sizeof err));
    CHECK(schema_field_by_name(&s, "a")->required);
    CHECK(!schema_field_by_name(&s, "b")->required);

    /* Und alles andere ist keine Antwort auf die Frage. */
    const char *bad =
        "type x\nfolder X\nlabel l\ncolumns a\nform a\n"
        "field a\n    kind text\n    label la\n    required vielleicht\n";
    CHECK(!load_text(&s, "pflicht_falsch", bad, err, sizeof err));
    CHECK(strstr(err, "yes") != NULL);
}

TEST(too_many_fields_are_refused_not_written_past_the_end)
{
    /* Ohne die Obergrenze schriebe der Parser über das Ende der Struktur
     * hinaus. Am Rückgabewert wäre das nicht zu sehen - erst der Sanitizer
     * macht daraus einen Fehlschlag. */
    char text[8192];
    int  n = snprintf(text, sizeof text, "type x\nfolder X\nlabel l\n"
                                         "columns f0\nform f0\n");
    for (int i = 0; i < SCHEMA_FIELDS_MAX + 3; i++)
        n += snprintf(text + n, sizeof text - (size_t)n,
                      "field f%d\n    kind text\n    label l%d\n", i, i);

    schema s;
    char   err[256] = "";
    CHECK(!load_text(&s, "zu_viele_felder", text, err, sizeof err));
    CHECK(strstr(err, "höchstens") != NULL);
}

TEST(lists_that_are_too_long_are_refused)
{
    schema s;
    char   err[256] = "";
    char   text[4096];
    int    n;

    /* Zu viele Spalten. */
    n = snprintf(text, sizeof text, "type x\nfolder X\nlabel l\ncolumns");
    for (int i = 0; i < SCHEMA_COLUMNS_MAX + 2; i++)
        n += snprintf(text + n, sizeof text - (size_t)n, " f%d", i);
    n += snprintf(text + n, sizeof text - (size_t)n, "\nform f0\n");
    for (int i = 0; i < SCHEMA_COLUMNS_MAX + 2; i++)
        n += snprintf(text + n, sizeof text - (size_t)n,
                      "field f%d\n    kind text\n    label l%d\n", i, i);

    CHECK(!load_text(&s, "zu_viele_spalten", text, err, sizeof err));
    CHECK(strstr(err, "columns") != NULL);

    /* Zu viele Auswahlwerte. */
    n = snprintf(text, sizeof text,
                 "type x\nfolder X\nlabel l\ncolumns p\nform p\n"
                 "field p\n    kind choice\n    label lp\n    values");
    for (int i = 0; i < SCHEMA_VALUES_MAX + 2; i++)
        n += snprintf(text + n, sizeof text - (size_t)n, " v%d", i);
    snprintf(text + n, sizeof text - (size_t)n, "\n");

    CHECK(!load_text(&s, "zu_viele_werte", text, err, sizeof err));
    CHECK(strstr(err, "values") != NULL);
}

TEST(the_view_registry_takes_list_and_month)
{
    schema s;
    char   err[256] = "";

    const char *head =
        "type e\nfolder E\nlabel l\ncolumns t\nform t\n"
        "field t\n    kind text\n    label lt\n"
        "field d\n    kind date\n    label ld\n";

    char text[1024];

    /* Ohne Angabe eine Liste. */
    REQUIRE(load_text(&s, "ansicht_ohne", head, err, sizeof err));
    CHECK_EQ(s.view, VIEW_LIST);

    snprintf(text, sizeof text, "view list\n%s", head);
    REQUIRE(load_text(&s, "ansicht_liste", text, err, sizeof err));
    CHECK_EQ(s.view, VIEW_LIST);

    snprintf(text, sizeof text, "view month d\n%s", head);
    REQUIRE(load_text(&s, "ansicht_monat", text, err, sizeof err));
    CHECK_EQ(s.view, VIEW_MONTH);
    CHECK_STR(s.view_field, "d");
}

TEST(a_calendar_without_a_date_field_is_refused)
{
    /* Ein Kalender über einem Textfeld wäre ein leeres Raster, und der Fehler
     * fiele erst auf, wenn jemand ihn öffnet. */
    schema s;
    char   err[256] = "";

    const char *head =
        "type e\nfolder E\nlabel l\ncolumns t\nform t\n"
        "field t\n    kind text\n    label lt\n";

    char text[1024];

    snprintf(text, sizeof text, "view month t\n%s", head);
    CHECK(!load_text(&s, "monat_ohne_datum", text, err, sizeof err));
    CHECK(strstr(err, "date") != NULL);

    snprintf(text, sizeof text, "view month gibtsnicht\n%s", head);
    CHECK(!load_text(&s, "monat_falsches_feld", text, err, sizeof err));
    CHECK(strstr(err, "gibtsnicht") != NULL);

    snprintf(text, sizeof text, "view month\n%s", head);
    CHECK(!load_text(&s, "monat_ohne_feld", text, err, sizeof err));
    CHECK(strstr(err, "Datumsfeld") != NULL);

    snprintf(text, sizeof text, "view raster t\n%s", head);
    CHECK(!load_text(&s, "ansicht_unbekannt", text, err, sizeof err));
    CHECK(strstr(err, "raster") != NULL);
}

TEST(a_missing_file_says_so)
{
    schema s;
    char   err[256] = "";
    CHECK(!schema_load(&s, "/tmp/gibt-es-nicht-pda.schema", err, sizeof err));
    CHECK(strstr(err, "gibt-es-nicht-pda") != NULL);
}

TEST(a_broken_schema_leaves_the_target_untouched)
{
    /* Ein halbfertiges Schema darf nie herauskommen. Wer eins hat und ein
     * kaputtes nachlädt, behält seins. */
    schema s;
    char   err[256] = "";
    REQUIRE(load_text(&s, "gut2", GOOD, err, sizeof err));
    CHECK_STR(s.type, "task");

    const char *bad = "type y\nfolder Y\nlabel l\ncolumns nix\nform nix\n"
                      "field t\n    kind text\n    label lt\n";
    CHECK(!load_text(&s, "kaputt", bad, err, sizeof err));

    CHECK_STR(s.type, "task");
    CHECK_EQ(s.field_count, 3);
}

int main(void)
{
    RUN(the_task_schema_reads);
    RUN(all_three_applications_are_just_files);

    RUN(indentation_decides_where_a_line_belongs);
    RUN(a_top_level_line_ends_the_open_field);
    RUN(comments_and_blank_lines_are_ignored);
    RUN(the_sort_direction_is_optional);

    RUN(a_typo_in_columns_is_caught_at_load_time);
    RUN(a_typo_in_form_or_sort_is_caught_too);
    RUN(an_unknown_key_or_kind_is_refused);
    RUN(a_choice_without_values_is_refused);
    RUN(values_on_something_that_is_not_a_choice_are_refused);
    RUN(a_field_without_a_label_is_refused);
    RUN(the_head_must_be_complete);
    RUN(a_duplicate_field_is_refused);
    RUN(an_empty_list_is_refused);
    RUN(a_name_that_is_too_long_is_refused_not_truncated);
    RUN(required_no_means_no);
    RUN(too_many_fields_are_refused_not_written_past_the_end);
    RUN(lists_that_are_too_long_are_refused);
    RUN(the_view_registry_takes_list_and_month);
    RUN(a_calendar_without_a_date_field_is_refused);
    RUN(a_missing_file_says_so);
    RUN(a_broken_schema_leaves_the_target_untouched);

    return test_summary();
}
