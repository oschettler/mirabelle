/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe schema.h für den Vertrag und data/schema/ für echte Schemata.
 *
 * Der Parser folgt keymap.c, i18n.c und collate.c: jeder Fehler bekommt eine
 * Meldung "datei:zeile: meldung", und bei einem Fehler kommt kein halbfertiges
 * Ergebnis zurück.
 *
 * Neu gegenüber den anderen ist die eine Ebene Einzug. Sie ist der ganze
 * Unterschied zu einer flachen Schlüssel-Wert-Datei, und sie kostet eine
 * einzige Zustandsvariable: welches Feld gerade offen ist.
 */
#include "app/schema.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* --- Meldungen --------------------------------------------------------------- */

static bool fail(char *err, size_t err_size, const char *path, int line,
                 const char *fmt, ...)
{
    char    msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    if (err && err_size) {
        if (line > 0) snprintf(err, err_size, "%s:%d: %s", path, line, msg);
        else          snprintf(err, err_size, "%s: %s", path, msg);
    }
    return false;
}

/* --- Feldtypen ------------------------------------------------------------------
 *
 * Die eine Stelle, an der ein Feldtyp einen Namen hat. Der Leser in
 * lua/pdalua_schema.c geht über schema_kind_name(), statt die Namen ein
 * zweites Mal aufzuschreiben - sonst hieße derselbe Typ irgendwann an zwei
 * Stellen verschieden.
 */

static const struct { const char *name; field_kind kind; } KINDS[] = {
    { "text",    FIELD_TEXT    },
    { "gemtext", FIELD_GEMTEXT },
    { "date",    FIELD_DATE    },
    { "bool",    FIELD_BOOL    },
    { "choice",  FIELD_CHOICE  },
};

const char *schema_kind_name(field_kind kind)
{
    for (size_t i = 0; i < sizeof KINDS / sizeof KINDS[0]; i++)
        if (KINDS[i].kind == kind) return KINDS[i].name;
    return "?";
}

const schema_field *schema_field_by_name(const schema *s, const char *name)
{
    for (int i = 0; i < s->field_count; i++)
        if (strcmp(s->fields[i].name, name) == 0) return &s->fields[i];
    return NULL;
}

/* --- Eine Liste von Feldnamen --------------------------------------------------
 *
 * columns und form sind beides Wortlisten von Feldnamen. Ob sie auf etwas
 * zeigen, das es gibt, entscheidet sich erst, wenn alle Felder gelesen sind -
 * deshalb wird hier nur gesammelt und ganz am Ende geprüft.
 */
/* Alles, was sich erst beurteilen lässt, wenn das Schema vollständig ist.
 *
 * Getrennt vom Lesen, weil beides verschiedene Fragen beantwortet: der Leser
 * in lua/pdalua_schema.c prüft, ob die Tabelle die richtige Gestalt hat, und
 * diese Funktion, ob das Ergebnis eine Anwendung beschreibt, die es geben
 * kann. Ein Schema, dessen `columns` ein Feld nennt, das es nicht gibt, ist
 * einwandfrei geschrieben und trotzdem falsch. */
bool schema_check(const schema *s, const char *path, char *err, size_t err_size)
{
    if (!s->type[0])   return fail(err, err_size, path, 0, "type fehlt");
    if (!s->folder[0]) return fail(err, err_size, path, 0, "folder fehlt");
    if (!s->label[0])  return fail(err, err_size, path, 0, "label fehlt");
    if (s->field_count == 0) return fail(err, err_size, path, 0, "kein einziges Feld");

    for (int i = 0; i < s->field_count; i++) {
        const schema_field *f = &s->fields[i];
        if (!f->label[0])
            return fail(err, err_size, path, 0, "Feld „%s“: label fehlt", f->name);
        if (f->kind == FIELD_CHOICE && f->value_count == 0)
            return fail(err, err_size, path, 0,
                        "Feld „%s“ ist eine Auswahl ohne values", f->name);
        if (f->kind != FIELD_CHOICE && f->value_count > 0)
            return fail(err, err_size, path, 0,
                        "Feld „%s“ hat values, ist aber kein choice", f->name);
    }

    /* Ein Gemtext-Feld ist nicht ein Feld im Front Matter, sondern der Körper
     * des Datensatzes (browser.h). Zwei davon gäbe es nicht zu verteilen. */
    int gemtext = 0;
    for (int i = 0; i < s->field_count; i++)
        if (s->fields[i].kind == FIELD_GEMTEXT) gemtext++;
    if (gemtext > 1)
        return fail(err, err_size, path, 0,
                    "%d Felder vom Typ gemtext; ein Datensatz hat nur einen Körper",
                    gemtext);

    /* Jeder Name in columns, sort und form muss ein Feld sein, das es gibt.
     * Ein Tippfehler soll beim Laden auffallen und nicht als leere Spalte. */
    for (int i = 0; i < s->column_count; i++)
        if (!schema_field_by_name(s, s->columns[i]))
            return fail(err, err_size, path, 0,
                        "columns nennt „%s“, aber es gibt kein solches Feld",
                        s->columns[i]);

    for (int i = 0; i < s->form_count; i++)
        if (!schema_field_by_name(s, s->form[i]))
            return fail(err, err_size, path, 0,
                        "form nennt „%s“, aber es gibt kein solches Feld",
                        s->form[i]);

    if (s->view == VIEW_MONTH) {
        const schema_field *f = schema_field_by_name(s, s->view_field);
        if (!f)
            return fail(err, err_size, path, 0,
                        "view month nennt „%s“, aber es gibt kein solches Feld",
                        s->view_field);
        if (f->kind != FIELD_DATE)
            return fail(err, err_size, path, 0,
                        "view month braucht ein Feld vom Typ date; „%s“ ist %s",
                        s->view_field, schema_kind_name(f->kind));
    }

    if (s->sort[0] && !schema_field_by_name(s, s->sort))
        return fail(err, err_size, path, 0,
                    "sort nennt „%s“, aber es gibt kein solches Feld", s->sort);

    if (s->title_field[0]) {
        const schema_field *f = schema_field_by_name(s, s->title_field);
        if (!f)
            return fail(err, err_size, path, 0,
                        "title_field nennt „%s“, aber es gibt kein solches Feld",
                        s->title_field);
        if (f->kind != FIELD_TEXT)
            return fail(err, err_size, path, 0,
                        "title_field braucht ein Feld vom Typ text; „%s“ ist %s",
                        s->title_field, schema_kind_name(f->kind));
    }

    return true;
}
