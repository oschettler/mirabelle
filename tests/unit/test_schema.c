/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Die Schlussprüfung eines Schemas, siehe app/schema.h.
 *
 * Gelesen werden Schemata aus Lua-Dateien (D-15); dass das Lesen stimmt, steht
 * in test_lua.c. Hier geht es um die andere Hälfte: ob das Gelesene eine
 * Anwendung beschreibt, die es geben kann. Ein Schema, dessen `columns` ein
 * Feld nennt, das es nicht gibt, ist einwandfrei geschrieben und trotzdem
 * falsch.
 *
 * Die Schemata entstehen deshalb hier in C und nicht aus Dateien. So prüft
 * jeder Test genau eine Regel, und keiner davon geht kaputt, wenn sich die
 * Schreibweise ändert.
 */
#include "test.h"

#include <stdio.h>
#include <string.h>

#include "app/schema.h"

/* --- Gerüst --------------------------------------------------------------------- */

static schema_field field(const char *name, field_kind kind, const char *label)
{
    schema_field f;
    memset(&f, 0, sizeof f);

    snprintf(f.name,  sizeof f.name,  "%s", name);
    snprintf(f.label, sizeof f.label, "%s", label);
    f.kind = kind;
    return f;
}

static void add_field(schema *s, schema_field f)
{
    s->fields[s->field_count++] = f;
}

static void add_column(schema *s, const char *name)
{
    snprintf(s->columns[s->column_count++], SCHEMA_NAME_MAX, "%s", name);
}

static void add_form(schema *s, const char *name)
{
    snprintf(s->form[s->form_count++], SCHEMA_NAME_MAX, "%s", name);
}

/* Ein Schema, das durchgeht - Grundlage für die Fehlerfälle darunter. Jeder
 * Test verstellt daran genau eine Sache. */
static void good(schema *s)
{
    memset(s, 0, sizeof *s);

    snprintf(s->type,   sizeof s->type,   "task");
    snprintf(s->folder, sizeof s->folder, "Aufgaben");
    snprintf(s->label,  sizeof s->label,  "app.tasks");
    snprintf(s->sort,   sizeof s->sort,   "due");

    add_field(s, field("title", FIELD_TEXT, "field.title"));
    add_field(s, field("due",   FIELD_DATE, "field.due"));
    add_field(s, field("done",  FIELD_BOOL, "field.done"));

    add_column(s, "done");
    add_column(s, "title");
    add_form(s, "title");
    add_form(s, "due");
}

static bool ok(const schema *s, char *err, size_t n)
{
    return schema_check(s, "prüfling", err, n);
}

/* --- Was durchgeht ---------------------------------------------------------------- */

TEST(a_complete_schema_passes)
{
    schema s;
    char   err[256] = "";

    good(&s);
    if (!ok(&s, err, sizeof err)) printf("  %s\n", err);
    CHECK(ok(&s, err, sizeof err));
}

TEST(the_shipped_applications_pass)
{
    /* Sie kommen aus Lua-Dateien und sind dort schon geprüft worden - hier
     * steht nur, dass diese Prüfung dieselbe ist. Deshalb reicht ein Schema,
     * das eine der vier nachbaut. */
    schema s;
    char   err[256] = "";

    memset(&s, 0, sizeof s);
    snprintf(s.type,   sizeof s.type,   "note");
    snprintf(s.folder, sizeof s.folder, "Notizen");
    snprintf(s.label,  sizeof s.label,  "app.notes");
    snprintf(s.sort,   sizeof s.sort,   "title");

    add_field(&s, field("title", FIELD_TEXT,    "field.title"));
    add_field(&s, field("body",  FIELD_GEMTEXT, "field.notes"));
    add_column(&s, "title");
    add_form(&s, "title");
    add_form(&s, "body");

    CHECK(ok(&s, err, sizeof err));
}

/* --- Der Kopf ---------------------------------------------------------------------- */

TEST(the_head_must_be_complete)
{
    schema s;
    char   err[256] = "";

    good(&s);
    s.type[0] = '\0';
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "type") != NULL);

    good(&s);
    s.folder[0] = '\0';
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "folder") != NULL);

    good(&s);
    s.label[0] = '\0';
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "label") != NULL);
}

TEST(a_schema_without_fields_is_refused)
{
    /* Eine Anwendung ohne Felder hätte nichts zu zeigen und nichts zu
     * speichern. Sie fiele erst auf, wenn jemand sie öffnet. */
    schema s;
    char   err[256] = "";

    good(&s);
    s.field_count  = 0;
    s.column_count = 0;
    s.form_count   = 0;
    s.sort[0]      = '\0';
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "Feld") != NULL);
}

/* --- Namen, die es nicht gibt ------------------------------------------------------ */

TEST(a_typo_in_columns_is_caught)
{
    /* Der Fehler, um den es geht: „titel" statt „title". Ohne diese Prüfung
     * hätte der Browser eine Spalte, die immer leer ist, und niemand wüsste
     * warum. */
    schema s;
    char   err[256] = "";

    good(&s);
    snprintf(s.columns[0], SCHEMA_NAME_MAX, "titel");
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "titel") != NULL);
    CHECK(strstr(err, "columns") != NULL);
}

TEST(a_typo_in_form_or_sort_is_caught_too)
{
    schema s;
    char   err[256] = "";

    good(&s);
    snprintf(s.form[0], SCHEMA_NAME_MAX, "titel");
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "form") != NULL);

    good(&s);
    snprintf(s.sort, sizeof s.sort, "titel");
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "sort") != NULL);
}

TEST(no_sort_field_at_all_is_allowed)
{
    /* Ohne sort bleibt die Reihenfolge, in der die Datensätze kommen. Das ist
     * eine Entscheidung und kein Fehler. */
    schema s;
    char   err[256] = "";

    good(&s);
    s.sort[0] = '\0';
    CHECK(ok(&s, err, sizeof err));
}

/* --- Felder ------------------------------------------------------------------------ */

TEST(a_field_without_a_label_is_refused)
{
    /* Die Beschriftung ist ein Katalogschlüssel. Fehlt sie, stünde im Formular
     * eine Zeile ohne Namen. */
    schema s;
    char   err[256] = "";

    good(&s);
    s.fields[1].label[0] = '\0';
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "label") != NULL);
    CHECK(strstr(err, "due") != NULL);
}

TEST(a_choice_without_values_is_refused)
{
    schema s;
    char   err[256] = "";

    good(&s);
    s.fields[1].kind = FIELD_CHOICE;
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "values") != NULL);
}

TEST(values_on_something_that_is_not_a_choice_are_refused)
{
    /* Sie täten nichts, stünden aber da - und wer sie schreibt, erwartet, dass
     * sie wirken. Lieber ablehnen als stillschweigend übergehen. */
    schema s;
    char   err[256] = "";

    good(&s);
    snprintf(s.fields[0].values[0], SCHEMA_NAME_MAX, "a");
    s.fields[0].value_count = 1;
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "choice") != NULL);
}

TEST(two_gemtext_fields_are_refused)
{
    /* Ein Gemtext-Feld ist nicht ein Feld im Front Matter, sondern der Körper
     * des Datensatzes. Zwei davon gäbe es nicht zu verteilen. */
    schema s;
    char   err[256] = "";

    good(&s);
    s.fields[0].kind = FIELD_GEMTEXT;
    CHECK(ok(&s, err, sizeof err));

    s.fields[1].kind = FIELD_GEMTEXT;
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "gemtext") != NULL);
}

/* --- Der Kalender ------------------------------------------------------------------ */

TEST(a_list_view_needs_nothing_else)
{
    schema s;
    char   err[256] = "";

    good(&s);
    s.view = VIEW_LIST;
    CHECK(ok(&s, err, sizeof err));
}

TEST(a_calendar_needs_a_date_field_that_exists)
{
    /* Ein Kalender über einem Textfeld wäre ein leeres Raster, und der Fehler
     * fiele erst auf, wenn jemand ihn öffnet. */
    schema s;
    char   err[256] = "";

    good(&s);
    s.view = VIEW_MONTH;
    snprintf(s.view_field, sizeof s.view_field, "due");
    CHECK(ok(&s, err, sizeof err));

    snprintf(s.view_field, sizeof s.view_field, "title");
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "date") != NULL);

    snprintf(s.view_field, sizeof s.view_field, "gibtsnicht");
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "gibtsnicht") != NULL);
}

TEST(the_calendar_field_is_not_the_sort_field)
{
    /* Im mitgelieferten Terminschema heißen sie zufällig gleich. Hier nicht,
     * und die Prüfung darf die beiden nicht verwechseln. */
    schema s;
    char   err[256] = "";

    good(&s);
    s.view = VIEW_MONTH;
    snprintf(s.view_field, sizeof s.view_field, "due");
    snprintf(s.sort,       sizeof s.sort,       "title");
    CHECK(ok(&s, err, sizeof err));

    snprintf(s.sort, sizeof s.sort, "gibtsnicht");
    CHECK(!ok(&s, err, sizeof err));
    CHECK(strstr(err, "sort") != NULL);
}

/* --- Die Meldung ------------------------------------------------------------------- */

TEST(the_message_names_the_file)
{
    /* Wer vier Schemadateien hat, muss wissen, welche gemeint ist. */
    schema s;
    char   err[256] = "";

    good(&s);
    s.type[0] = '\0';
    CHECK(!schema_check(&s, "data/schema/task.lua", err, sizeof err));
    CHECK(strstr(err, "data/schema/task.lua") != NULL);
}

TEST(a_check_without_an_error_buffer_does_not_crash)
{
    /* Nicht jeder Aufrufer will die Meldung. Ein NULL darf nicht anders
     * entscheiden als ein Puffer. */
    schema s;

    good(&s);
    CHECK(schema_check(&s, "x", NULL, 0));

    s.type[0] = '\0';
    CHECK(!schema_check(&s, "x", NULL, 0));
}

int main(void)
{
    RUN(a_complete_schema_passes);
    RUN(the_shipped_applications_pass);

    RUN(the_head_must_be_complete);
    RUN(a_schema_without_fields_is_refused);

    RUN(a_typo_in_columns_is_caught);
    RUN(a_typo_in_form_or_sort_is_caught_too);
    RUN(no_sort_field_at_all_is_allowed);

    RUN(a_field_without_a_label_is_refused);
    RUN(a_choice_without_values_is_refused);
    RUN(values_on_something_that_is_not_a_choice_are_refused);
    RUN(two_gemtext_fields_are_refused);

    RUN(a_list_view_needs_nothing_else);
    RUN(a_calendar_needs_a_date_field_that_exists);
    RUN(the_calendar_field_is_not_the_sort_field);

    RUN(the_message_names_the_file);
    RUN(a_check_without_an_error_buffer_does_not_crash);

    return test_summary();
}
