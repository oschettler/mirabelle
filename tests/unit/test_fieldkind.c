/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Die Feldtyp-Registratur, siehe app/fieldkind.h.
 *
 * Zwei Dinge stehen hier auf dem Prüfstand: dass jeder Feldtyp vollständig
 * eingetragen ist, und dass Speicher- und Anzeigeform sauber auseinandergehen
 * und wieder zusammenfinden.
 */
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/fieldkind.h"
#include "app/schema.h"
#include "core/i18n.h"
#include "ui/theme.h"
#include "ui/widget.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* Statisch, weil Widgets nur einen Zeiger auf das Thema halten (widget.h). */
static theme g_theme;

static const theme *test_theme(void)
{
    static bool loaded = false;
    if (loaded) return &g_theme;

    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/themes/desktop.theme", PDA_DATA_DIR);
    if (!theme_load(&g_theme, path, err, sizeof err)) theme_defaults(&g_theme);
    loaded = true;
    return &g_theme;
}

static catalog *load_cat(const char *lang)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/lang/%s.strings", PDA_DATA_DIR, lang);

    catalog *c = i18n_load(path, err, sizeof err);
    if (!c) printf("  Katalog nicht ladbar: %s\n", err);
    return c;
}

static schema_field field_of(field_kind kind)
{
    schema_field f;
    memset(&f, 0, sizeof f);
    f.kind = kind;
    snprintf(f.name, sizeof f.name, "x");
    snprintf(f.label, sizeof f.label, "field.title");
    return f;
}

/* --- Vollständigkeit ------------------------------------------------------------ */

TEST(every_kind_has_a_complete_entry)
{
    /* Der Test, der einen vergessenen Eintrag findet: wer schema.h um einen
     * Feldtyp erweitert und die Registratur nicht, fällt hier durch statt
     * irgendwann in einem Nullzeiger. */
    const field_kind kinds[] = { FIELD_TEXT, FIELD_GEMTEXT, FIELD_DATE,
                                 FIELD_BOOL, FIELD_CHOICE };

    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
        const field_kind_ops *ops = fieldkind(kinds[i]);
        REQUIRE(ops != NULL);

        if (!ops->name) printf("  Feldtyp %d ohne Namen\n", (int)kinds[i]);
        CHECK(ops->name != NULL);
        CHECK(ops->format != NULL);
        CHECK(ops->parse != NULL);
        CHECK(ops->make_widget != NULL);
        CHECK(ops->read != NULL);
        CHECK(ops->write != NULL);

        /* Und der Name muss der sein, der auch in der Schemadatei steht. */
        CHECK_STR(ops->name, schema_kind_name(kinds[i]));
    }
}

TEST(an_unknown_kind_still_gives_an_entry)
{
    /* fieldkind() sagt zu: nie NULL. Ein Wert, den es in field_kind nicht
     * gibt, kann aus einer beschädigten Struktur kommen - dann soll die
     * Registratur auf Text zurückfallen und nicht neben ihr Feld greifen. */
    const field_kind_ops *ops = fieldkind((field_kind)99);
    REQUIRE(ops != NULL);
    CHECK_STR(ops->name, "text");

    ops = fieldkind((field_kind)-1);
    REQUIRE(ops != NULL);
    CHECK_STR(ops->name, "text");
}

/* --- Text ------------------------------------------------------------------------ */

TEST(text_stores_what_it_shows)
{
    schema_field f   = field_of(FIELD_TEXT);
    catalog     *cat = load_cat("de");
    REQUIRE(cat != NULL);

    char out[64];
    fieldkind_of(&f)->format(&f, cat, "Müller", out, sizeof out);
    CHECK_STR(out, "Müller");

    CHECK(fieldkind_of(&f)->parse(&f, cat, "Müller", out, sizeof out));
    CHECK_STR(out, "Müller");

    i18n_free(cat);
}

TEST(text_that_does_not_fit_is_refused)
{
    schema_field f   = field_of(FIELD_TEXT);
    catalog     *cat = load_cat("de");
    REQUIRE(cat != NULL);

    char out[8];
    CHECK(!fieldkind_of(&f)->parse(&f, cat, "viel zu lang für acht Byte",
                                   out, sizeof out));
    i18n_free(cat);
}

/* --- Datum -------------------------------------------------------------------------
 *
 * Der Feldtyp, an dem sich zeigt, wozu die Trennung gut ist: gespeichert wird
 * JJJJ-MM-TT, damit sich Daten als Zeichenketten sortieren; angezeigt wird, was
 * der Katalog sagt.
 */

TEST(a_date_is_stored_iso_and_shown_the_local_way)
{
    schema_field f  = field_of(FIELD_DATE);
    catalog     *de = load_cat("de");
    REQUIRE(de != NULL);

    char out[64];
    fieldkind_of(&f)->format(&f, de, "2026-03-05", out, sizeof out);
    CHECK_STR(out, "05.03.2026");     /* date.format ist %d.%m.%Y */

    CHECK(fieldkind_of(&f)->parse(&f, de, "05.03.2026", out, sizeof out));
    CHECK_STR(out, "2026-03-05");

    i18n_free(de);
}

TEST(the_date_format_comes_from_the_catalog)
{
    /* Ein anderes Format ist eine andere Katalogzeile, keine Änderung am Code.
     * Geprüft mit einem eigenen Katalog, damit der Test nicht davon abhängt,
     * welches Format data/lang zufällig gerade führt. */
    const char *path = "/tmp/pda_fieldkind_iso.strings";
    FILE       *fp   = fopen(path, "wb");
    REQUIRE(fp != NULL);
    fputs("date.format = %Y/%m/%d\nbool.yes = Ja\nbool.no = Nein\n", fp);
    fclose(fp);

    char     err[256] = "";
    catalog *cat = i18n_load(path, err, sizeof err);
    REQUIRE(cat != NULL);

    schema_field f = field_of(FIELD_DATE);
    char         out[64];

    fieldkind_of(&f)->format(&f, cat, "2026-03-05", out, sizeof out);
    CHECK_STR(out, "2026/03/05");

    CHECK(fieldkind_of(&f)->parse(&f, cat, "2026/03/05", out, sizeof out));
    CHECK_STR(out, "2026-03-05");

    i18n_free(cat);
}

TEST(a_date_survives_the_round_trip)
{
    schema_field f  = field_of(FIELD_DATE);
    catalog     *de = load_cat("de");
    REQUIRE(de != NULL);

    const char *dates[] = { "2026-01-01", "2026-12-31", "1999-06-15", "2026-03-05" };
    for (size_t i = 0; i < sizeof dates / sizeof dates[0]; i++) {
        char shown[64], back[64];
        fieldkind_of(&f)->format(&f, de, dates[i], shown, sizeof shown);
        CHECK(fieldkind_of(&f)->parse(&f, de, shown, back, sizeof back));
        CHECK_STR(back, dates[i]);
    }

    i18n_free(de);
}

TEST(an_empty_date_is_allowed)
{
    schema_field f  = field_of(FIELD_DATE);
    catalog     *de = load_cat("de");
    REQUIRE(de != NULL);

    char out[64];
    fieldkind_of(&f)->format(&f, de, "", out, sizeof out);
    CHECK_STR(out, "");

    CHECK(fieldkind_of(&f)->parse(&f, de, "", out, sizeof out));
    CHECK_STR(out, "");

    CHECK(fieldkind_of(&f)->parse(&f, de, "   ", out, sizeof out));
    CHECK_STR(out, "");

    i18n_free(de);
}

TEST(nonsense_dates_are_refused_not_guessed)
{
    schema_field f  = field_of(FIELD_DATE);
    catalog     *de = load_cat("de");
    REQUIRE(de != NULL);

    char out[64];
    const char *bad[] = {
        "5.3.2026",      /* ohne führende Null passt es nicht auf %d.%m.%Y */
        "05-03-2026",    /* falsches Trennzeichen */
        "32.03.2026",    /* den Tag gibt es nicht */
        "05.13.2026",    /* den Monat auch nicht */
        "05.03.2026x",   /* hinten steht noch etwas */
        "morgen",

        /* Der heimtückische Fall: alle Trennzeichen stimmen, und der
         * Buchstabe steckt mitten in der Jahreszahl. Wer die Ziffern nicht
         * prüft, rechnet ihn stillschweigend mit und speichert ein Jahr, das
         * nie jemand eingegeben hat. */
        "05.03.20a6",
        "0a.03.2026",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (fieldkind_of(&f)->parse(&f, de, bad[i], out, sizeof out))
            printf("  „%s“ wurde angenommen als „%s“\n", bad[i], out);
        CHECK(!fieldkind_of(&f)->parse(&f, de, bad[i], out, sizeof out));
    }

    i18n_free(de);
}

TEST(a_date_that_a_human_broke_is_shown_not_swallowed)
{
    /* Jemand hat die Datei von Hand bearbeitet. Der Wert ist kein Datum mehr,
     * aber er soll sichtbar bleiben - sonst sieht der Nutzer ein leeres Feld
     * und speichert seinen Text weg, ohne es zu merken. */
    schema_field f  = field_of(FIELD_DATE);
    catalog     *de = load_cat("de");
    REQUIRE(de != NULL);

    char out[64];
    fieldkind_of(&f)->format(&f, de, "irgendwann", out, sizeof out);
    CHECK_STR(out, "irgendwann");

    i18n_free(de);
}

/* --- Wahrheitswert ------------------------------------------------------------------ */

TEST(a_bool_is_stored_in_english_and_shown_translated)
{
    schema_field f  = field_of(FIELD_BOOL);
    catalog     *de = load_cat("de");
    catalog     *en = load_cat("en");
    REQUIRE(de && en);

    char out[64];

    /* Gespeichert wird englisch - wie jeder Feldinhalt, den nicht ein Mensch
     * getippt hat (D-1). */
    fieldkind_of(&f)->format(&f, de, "yes", out, sizeof out);
    CHECK_STR(out, "Ja");
    fieldkind_of(&f)->format(&f, en, "yes", out, sizeof out);
    CHECK_STR(out, "Yes");

    fieldkind_of(&f)->format(&f, de, "no", out, sizeof out);
    CHECK_STR(out, "Nein");

    /* Alles, was nicht „yes" ist, ist nein - auch ein leeres Feld. */
    fieldkind_of(&f)->format(&f, de, "", out, sizeof out);
    CHECK_STR(out, "Nein");

    CHECK(fieldkind_of(&f)->parse(&f, de, "Ja", out, sizeof out));
    CHECK_STR(out, "yes");
    CHECK(fieldkind_of(&f)->parse(&f, de, "Nein", out, sizeof out));
    CHECK_STR(out, "no");

    i18n_free(de);
    i18n_free(en);
}

TEST(a_bool_goes_through_a_checkbox)
{
    schema_field f   = field_of(FIELD_BOOL);
    catalog     *cat = load_cat("de");
    REQUIRE(cat != NULL);

    widget *w = fieldkind_of(&f)->make_widget(&f, test_theme(), cat);
    REQUIRE(w != NULL);

    char out[16];
    fieldkind_of(&f)->write(&f, cat, w, "yes");
    CHECK(checkbox_value(w));
    CHECK(fieldkind_of(&f)->read(&f, cat, w, out, sizeof out));
    CHECK_STR(out, "yes");

    fieldkind_of(&f)->write(&f, cat, w, "no");
    CHECK(!checkbox_value(w));
    CHECK(fieldkind_of(&f)->read(&f, cat, w, out, sizeof out));
    CHECK_STR(out, "no");

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Auswahl -------------------------------------------------------------------------- */

static schema_field a_choice(void)
{
    schema_field f = field_of(FIELD_CHOICE);
    snprintf(f.values[0], SCHEMA_NAME_MAX, "privat");
    snprintf(f.values[1], SCHEMA_NAME_MAX, "arbeit");
    snprintf(f.values[2], SCHEMA_NAME_MAX, "unbekanntes");
    f.value_count = 3;
    return f;
}

TEST(a_choice_shows_the_catalog_text_and_falls_back_to_the_value)
{
    schema_field f   = a_choice();
    catalog     *cat = load_cat("de");
    REQUIRE(cat != NULL);

    char out[64];
    fieldkind_of(&f)->format(&f, cat, "privat", out, sizeof out);
    CHECK_STR(out, "Privat");

    /* Kein Katalogeintrag: dann steht der Wert selbst da. Ein Schema soll sich
     * schreiben lassen, ohne dass jemand vorher Katalogzeilen anlegt. */
    fieldkind_of(&f)->format(&f, cat, "unbekanntes", out, sizeof out);
    CHECK_STR(out, "unbekanntes");

    i18n_free(cat);
}

TEST(a_choice_takes_both_forms_and_refuses_the_rest)
{
    schema_field f   = a_choice();
    catalog     *cat = load_cat("de");
    REQUIRE(cat != NULL);

    char out[64];
    CHECK(fieldkind_of(&f)->parse(&f, cat, "privat", out, sizeof out));
    CHECK_STR(out, "privat");

    CHECK(fieldkind_of(&f)->parse(&f, cat, "Privat", out, sizeof out));
    CHECK_STR(out, "privat");

    /* Nichts gewählt ist erlaubt. */
    CHECK(fieldkind_of(&f)->parse(&f, cat, "", out, sizeof out));
    CHECK_STR(out, "");

    /* Etwas, das im Schema nicht vorgesehen ist, nicht. */
    CHECK(!fieldkind_of(&f)->parse(&f, cat, "irgendwas", out, sizeof out));

    i18n_free(cat);
}

TEST(a_choice_goes_through_a_list)
{
    schema_field f   = a_choice();
    catalog     *cat = load_cat("de");
    REQUIRE(cat != NULL);

    widget *w = fieldkind_of(&f)->make_widget(&f, test_theme(), cat);
    REQUIRE(w != NULL);
    CHECK_EQ(list_count(w), 3);

    /* Frisch gebaut ist nichts gewählt. Der erste Wert wäre eine erfundene
     * Voreinstellung und stünde nach dem Speichern im Datensatz. */
    CHECK_EQ(list_selected(w), -1);

    char out[64];
    CHECK(fieldkind_of(&f)->read(&f, cat, w, out, sizeof out));
    CHECK_STR(out, "");

    /* Und ein Wert, den es nicht gibt, hebt die Auswahl auf, statt die alte
     * stehen zu lassen - sonst zeigte das Formular den Wert des zuvor
     * geöffneten Datensatzes. */
    fieldkind_of(&f)->write(&f, cat, w, "privat");
    CHECK_EQ(list_selected(w), 0);
    fieldkind_of(&f)->write(&f, cat, w, "gibtesnicht");
    CHECK_EQ(list_selected(w), -1);

    fieldkind_of(&f)->write(&f, cat, w, "arbeit");
    CHECK_EQ(list_selected(w), 1);
    CHECK(fieldkind_of(&f)->read(&f, cat, w, out, sizeof out));
    CHECK_STR(out, "arbeit");

    widget_destroy(w);
    i18n_free(cat);
}

TEST(two_choice_widgets_do_not_share_their_values)
{
    /* Der Fehler, der hier beinahe drin gewesen wäre: ein gemeinsamer
     * Umschreibpuffer für die Werte. Zwei Auswahlfelder in einem Formular
     * hätten einander überschrieben, und das zweite hätte die Werte des
     * ersten gezeigt. */
    catalog *cat = load_cat("de");
    REQUIRE(cat != NULL);

    schema_field a = a_choice();

    schema_field b = field_of(FIELD_CHOICE);
    snprintf(b.values[0], SCHEMA_NAME_MAX, "eins");
    snprintf(b.values[1], SCHEMA_NAME_MAX, "zwei");
    b.value_count = 2;

    widget *wa = fieldkind_of(&a)->make_widget(&a, test_theme(), cat);
    widget *wb = fieldkind_of(&b)->make_widget(&b, test_theme(), cat);
    REQUIRE(wa && wb);

    CHECK_EQ(list_count(wa), 3);
    CHECK_EQ(list_count(wb), 2);

    char out[64];
    fieldkind_of(&a)->write(&a, cat, wa, "arbeit");
    fieldkind_of(&b)->write(&b, cat, wb, "zwei");

    CHECK(fieldkind_of(&a)->read(&a, cat, wa, out, sizeof out));
    CHECK_STR(out, "arbeit");
    CHECK(fieldkind_of(&b)->read(&b, cat, wb, out, sizeof out));
    CHECK_STR(out, "zwei");

    widget_destroy(wa);
    widget_destroy(wb);
    i18n_free(cat);
}

/* --- Durch das Bedienelement und zurück ------------------------------------------------ */

TEST(a_value_survives_the_way_through_a_widget)
{
    catalog *cat = load_cat("de");
    REQUIRE(cat != NULL);

    struct { field_kind kind; const char *stored; } cases[] = {
        { FIELD_TEXT,    "Müller anrufen" },
        { FIELD_GEMTEXT, "Zwei Zeilen\nmit Umbruch" },
        { FIELD_DATE,    "2026-03-05" },
        { FIELD_BOOL,    "yes" },
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        schema_field          f   = field_of(cases[i].kind);
        const field_kind_ops *ops = fieldkind_of(&f);

        widget *w = ops->make_widget(&f, test_theme(), cat);
        REQUIRE(w != NULL);

        ops->write(&f, cat, w, cases[i].stored);

        char back[256];
        CHECK(ops->read(&f, cat, w, back, sizeof back));
        if (strcmp(back, cases[i].stored) != 0)
            printf("  %s: „%s“ wurde „%s“\n", ops->name, cases[i].stored, back);
        CHECK_STR(back, cases[i].stored);

        widget_destroy(w);
    }

    i18n_free(cat);
}

int main(void)
{
    RUN(every_kind_has_a_complete_entry);

    RUN(an_unknown_kind_still_gives_an_entry);
    RUN(text_stores_what_it_shows);
    RUN(text_that_does_not_fit_is_refused);

    RUN(a_date_is_stored_iso_and_shown_the_local_way);
    RUN(the_date_format_comes_from_the_catalog);
    RUN(a_date_survives_the_round_trip);
    RUN(an_empty_date_is_allowed);
    RUN(nonsense_dates_are_refused_not_guessed);
    RUN(a_date_that_a_human_broke_is_shown_not_swallowed);

    RUN(a_bool_is_stored_in_english_and_shown_translated);
    RUN(a_bool_goes_through_a_checkbox);

    RUN(a_choice_shows_the_catalog_text_and_falls_back_to_the_value);
    RUN(a_choice_takes_both_forms_and_refuses_the_rest);
    RUN(a_choice_goes_through_a_list);
    RUN(two_choice_widgets_do_not_share_their_values);

    RUN(a_value_survives_the_way_through_a_widget);

    return test_summary();
}
