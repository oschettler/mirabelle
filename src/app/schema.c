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

/* --- Zerteilen ----------------------------------------------------------------- */

/* Schneidet das nächste Wort aus *p heraus und rückt *p dahinter. Liefert
 * false, wenn keins mehr kommt. */
static bool next_word(char **p, char **word)
{
    char *s = *p;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) { *p = s; return false; }

    *word = s;
    while (*s && *s != ' ' && *s != '\t') s++;
    if (*s) *s++ = '\0';

    *p = s;
    return true;
}

static bool copy_word(char *dst, size_t cap, const char *src,
                      char *err, size_t err_size, const char *path, int line,
                      const char *what)
{
    if (strlen(src) >= cap)
        return fail(err, err_size, path, line, "%s ist zu lang (höchstens %zu Zeichen)",
                    what, cap - 1);
    snprintf(dst, cap, "%s", src);
    return true;
}

/* --- Feldtypen ------------------------------------------------------------------ */

static const struct { const char *name; field_kind kind; } KINDS[] = {
    { "text",    FIELD_TEXT    },
    { "gemtext", FIELD_GEMTEXT },
    { "date",    FIELD_DATE    },
    { "bool",    FIELD_BOOL    },
    { "choice",  FIELD_CHOICE  },
};

static bool parse_kind(const char *name, field_kind *out)
{
    for (size_t i = 0; i < sizeof KINDS / sizeof KINDS[0]; i++)
        if (strcmp(KINDS[i].name, name) == 0) { *out = KINDS[i].kind; return true; }
    return false;
}

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
static bool read_name_list(char (*dst)[SCHEMA_NAME_MAX], int cap, int *count,
                           char *rest, char *err, size_t err_size,
                           const char *path, int line, const char *what)
{
    *count = 0;

    char *word;
    while (next_word(&rest, &word)) {
        if (*count >= cap)
            return fail(err, err_size, path, line, "%s: höchstens %d Einträge",
                        what, cap);
        if (!copy_word(dst[*count], SCHEMA_NAME_MAX, word,
                       err, err_size, path, line, what)) return false;
        (*count)++;
    }

    if (*count == 0)
        return fail(err, err_size, path, line, "%s: leer", what);
    return true;
}

/* --- Laden ----------------------------------------------------------------------- */

/* Die Schlüssel auf oberster Ebene. */
static bool top_level(schema *s, const char *key, char *rest,
                      char *err, size_t err_size, const char *path, int line)
{
    char *word;

    if (strcmp(key, "type") == 0 || strcmp(key, "folder") == 0 ||
        strcmp(key, "label") == 0) {
        if (!next_word(&rest, &word))
            return fail(err, err_size, path, line, "%s: der Wert fehlt", key);

        char *dst = strcmp(key, "type")   == 0 ? s->type
                  : strcmp(key, "folder") == 0 ? s->folder
                                               : s->label;
        return copy_word(dst, SCHEMA_NAME_MAX, word, err, err_size, path, line, key);
    }

    if (strcmp(key, "sort") == 0) {
        if (!next_word(&rest, &word))
            return fail(err, err_size, path, line, "sort: der Wert fehlt");
        if (!copy_word(s->sort, SCHEMA_NAME_MAX, word,
                       err, err_size, path, line, "sort")) return false;

        /* Ein zweites Wort darf die Richtung angeben. */
        char *dir;
        if (next_word(&rest, &dir)) {
            if (strcmp(dir, "desc") == 0)      s->sort_desc = true;
            else if (strcmp(dir, "asc") == 0)  s->sort_desc = false;
            else return fail(err, err_size, path, line,
                             "sort: „%s“ ist keine Richtung (asc oder desc)", dir);
        }
        return true;
    }

    if (strcmp(key, "columns") == 0)
        return read_name_list(s->columns, SCHEMA_COLUMNS_MAX, &s->column_count,
                              rest, err, err_size, path, line, "columns");

    if (strcmp(key, "form") == 0)
        return read_name_list(s->form, SCHEMA_FIELDS_MAX, &s->form_count,
                              rest, err, err_size, path, line, "form");

    return fail(err, err_size, path, line, "unbekannter Schlüssel „%s“", key);
}

/* Die Schlüssel innerhalb eines Feldes. */
static bool field_level(schema_field *f, const char *key, char *rest,
                        char *err, size_t err_size, const char *path, int line)
{
    char *word;

    if (strcmp(key, "kind") == 0) {
        if (!next_word(&rest, &word))
            return fail(err, err_size, path, line, "kind: der Wert fehlt");
        if (!parse_kind(word, &f->kind))
            return fail(err, err_size, path, line, "unbekannter Feldtyp „%s“", word);
        return true;
    }

    if (strcmp(key, "label") == 0) {
        if (!next_word(&rest, &word))
            return fail(err, err_size, path, line, "label: der Wert fehlt");
        return copy_word(f->label, SCHEMA_NAME_MAX, word,
                         err, err_size, path, line, "label");
    }

    if (strcmp(key, "required") == 0) {
        if (!next_word(&rest, &word))
            return fail(err, err_size, path, line, "required: der Wert fehlt");
        if (strcmp(word, "yes") == 0)      f->required = true;
        else if (strcmp(word, "no") == 0)  f->required = false;
        else return fail(err, err_size, path, line,
                         "required: „%s“ ist weder yes noch no", word);
        return true;
    }

    if (strcmp(key, "values") == 0)
        return read_name_list(f->values, SCHEMA_VALUES_MAX, &f->value_count,
                              rest, err, err_size, path, line, "values");

    return fail(err, err_size, path, line, "unbekannter Schlüssel „%s“ in einem Feld", key);
}

/* Alles, was sich erst beurteilen lässt, wenn die Datei ganz gelesen ist. */
static bool check_whole(const schema *s, char *err, size_t err_size, const char *path)
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

    if (s->sort[0] && !schema_field_by_name(s, s->sort))
        return fail(err, err_size, path, 0,
                    "sort nennt „%s“, aber es gibt kein solches Feld", s->sort);

    return true;
}

bool schema_load(schema *s, const char *path, char *err, size_t err_size)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return fail(err, err_size, path, 0, "nicht lesbar");

    schema tmp;
    memset(&tmp, 0, sizeof tmp);

    char          line[512];
    int           lineno  = 0;
    bool          ok      = true;
    schema_field *current = NULL;   /* das zuletzt begonnene Feld */

    while (ok && fgets(line, sizeof line, fp)) {
        lineno++;

        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        /* Der Einzug entscheidet, wohin die Zeile gehört - deshalb wird er
         * gemessen, bevor er weggeschnitten wird. */
        bool  indented = (line[0] == ' ' || line[0] == '\t');
        char *p        = line;
        while (*p == ' ' || *p == '\t') p++;

        char *end = p + strlen(p);
        while (end > p && (end[-1] == ' ' || end[-1] == '\t' ||
                           end[-1] == '\r' || end[-1] == '\n')) end--;
        *end = '\0';
        if (!*p) continue;

        char *key;
        next_word(&p, &key);

        if (indented) {
            if (!current) {
                ok = fail(err, err_size, path, lineno,
                          "eingerückt, aber kein Feld begonnen");
                break;
            }
            ok = field_level(current, key, p, err, err_size, path, lineno);
            continue;
        }

        if (strcmp(key, "field") == 0) {
            char *name;
            if (!next_word(&p, &name)) {
                ok = fail(err, err_size, path, lineno, "field: der Name fehlt");
                break;
            }
            if (tmp.field_count >= SCHEMA_FIELDS_MAX) {
                ok = fail(err, err_size, path, lineno,
                          "höchstens %d Felder", SCHEMA_FIELDS_MAX);
                break;
            }
            if (schema_field_by_name(&tmp, name)) {
                ok = fail(err, err_size, path, lineno,
                          "das Feld „%s“ gibt es schon", name);
                break;
            }

            current = &tmp.fields[tmp.field_count];
            memset(current, 0, sizeof *current);
            if (!copy_word(current->name, SCHEMA_NAME_MAX, name,
                           err, err_size, path, lineno, "field")) {
                ok = false;
                break;
            }
            tmp.field_count++;
            continue;
        }

        /* Etwas auf oberster Ebene, das kein Feld beginnt - dann ist das
         * zuletzt begonnene Feld zu Ende. */
        current = NULL;
        ok      = top_level(&tmp, key, p, err, err_size, path, lineno);
    }

    fclose(fp);
    if (!ok) return false;
    if (!check_whole(&tmp, err, err_size, path)) return false;

    *s = tmp;
    return true;
}
