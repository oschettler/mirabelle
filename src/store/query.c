/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe query.h für den Vertrag.
 *
 * Zwei Dinge macht diese Datei und sonst nichts: eine Abfrage aufbauen und
 * einen Datensatz daran messen. Kein Dateizugriff, keine Datenbank, kein
 * Zeichensatz - deshalb lässt sie sich vollständig prüfen, ohne dass etwas
 * anderes läuft.
 */
#include "store/query.h"

#include <string.h>

/* Kopiert nach dst, aber nur, wenn es ganz hineinpasst. Abschneiden wäre hier
 * das Schlimmste, was passieren kann: die Abfrage liefe weiter und suchte
 * etwas anderes, als der Aufrufer wollte. */
static bool copy_exact(char *dst, size_t cap, const char *src)
{
    if (!src) src = "";
    size_t n = strlen(src);
    if (n >= cap) return false;
    memcpy(dst, src, n + 1);
    return true;
}

/* --- Aufbauen ---------------------------------------------------------------- */

void query_init(query *q, const char *collection)
{
    memset(q, 0, sizeof *q);
    copy_exact(q->collection, sizeof q->collection, collection);
}

bool query_where(query *q, const char *field, query_op op, const char *value)
{
    if (q->filter_count >= QUERY_FILTERS_MAX) return false;
    if (!field || !*field) return false;

    query_filter f;
    memset(&f, 0, sizeof f);
    f.op = op;

    if (!copy_exact(f.field, sizeof f.field, field)) return false;

    /* Bei PRESENT und ABSENT gibt es keinen Wert zu vergleichen. Ihn trotzdem
     * zu übernehmen wäre irreführend: er stünde in der Struktur und täte
     * nichts. */
    if (op != QF_PRESENT && op != QF_ABSENT) {
        if (!value) return false;
        if (!copy_exact(f.value, sizeof f.value, value)) return false;
    }

    q->filters[q->filter_count++] = f;
    return true;
}

bool query_text(query *q, const char *words)
{
    return copy_exact(q->text, sizeof q->text, words);
}

bool query_order(query *q, const char *field, bool descending)
{
    q->descending = descending;
    return copy_exact(q->order_field, sizeof q->order_field, field);
}

void query_limit(query *q, int limit, int offset)
{
    q->limit  = limit  > 0 ? limit  : 0;
    q->offset = offset > 0 ? offset : 0;
}

/* --- Vergleichen -------------------------------------------------------------
 *
 * Ohne Faltungstabelle wird schlicht verglichen. Das ist kein Notbehelf,
 * sondern eine brauchbare Betriebsart: ein Test, der die Faltung nicht prüfen
 * will, soll sie nicht mitschleppen müssen.
 */

static bool text_contains(const collate *c, const char *hay, const char *needle)
{
    if (c) return collate_contains(c, hay, needle);
    return strstr(hay, needle) != NULL;
}

static bool text_starts_with(const collate *c, const char *hay, const char *prefix)
{
    if (c) return collate_starts_with(c, hay, prefix);
    return strncmp(hay, prefix, strlen(prefix)) == 0;
}

/* --- Eine Bedingung prüfen ----------------------------------------------------
 *
 * Ein Feld kann eine Liste sein (frontmatter.h). Eine Bedingung gilt als
 * erfüllt, wenn EIN Eintrag sie erfüllt - „Etikett enthält Arbeit" soll
 * zutreffen, wenn eines der Etiketten das tut, nicht nur das erste.
 */

static bool value_matches(const query_filter *f, const char *v, const collate *search)
{
    switch (f->op) {
    case QF_EQUALS:  return strcmp(v, f->value) == 0;
    case QF_CONTAINS:return text_contains(search, v, f->value);
    case QF_PREFIX:  return text_starts_with(search, v, f->value);
    case QF_LESS:    return strcmp(v, f->value) < 0;
    case QF_GREATER: return strcmp(v, f->value) > 0;
    case QF_PRESENT: return *v != '\0';
    case QF_ABSENT:  return false;   /* wird oben entschieden, siehe unten */
    }
    return false;
}

static bool filter_matches(const query_filter *f, frontmatter *fm,
                           const collate *search)
{
    int n = frontmatter_list_count(fm, f->field);

    /* ABSENT ist die einzige Bedingung, die über das Fehlen entscheidet, und
     * damit die einzige, die nicht über die Einträge laufen kann. */
    if (f->op == QF_ABSENT) {
        const char *v = frontmatter_get(fm, f->field);
        return !v || !*v;
    }

    for (int i = 0; i < n; i++) {
        const char *v = frontmatter_list_at(fm, f->field, i);
        if (v && value_matches(f, v, search)) return true;
    }
    return false;
}

/* --- Volltext -----------------------------------------------------------------
 *
 * Alle Wörter müssen vorkommen, jedes irgendwo: in einem Feldwert oder im
 * Körper. Das ist die Erwartung an ein Suchfeld - wer zwei Wörter eintippt,
 * will die Schnittmenge, nicht die Vereinigung.
 */

static bool record_has_word(record *rec, const char *word, size_t len,
                            const collate *search)
{
    char needle[QUERY_VALUE_MAX];
    if (len >= sizeof needle) len = sizeof needle - 1;
    memcpy(needle, word, len);
    needle[len] = '\0';

    if (text_contains(search, record_body(rec), needle)) return true;

    /* Dann alle Felder, einschließlich der Listeneinträge. Ein Etikett ist so
     * gut durchsuchbar wie eine Überschrift. */
    frontmatter *fm = record_fields(rec);
    int          nf = frontmatter_count(fm);

    for (int i = 0; i < nf; i++) {
        const char *key = frontmatter_key_at(fm, i);
        if (!key) continue;

        int nv = frontmatter_list_count(fm, key);
        for (int j = 0; j < nv; j++) {
            const char *v = frontmatter_list_at(fm, key, j);
            if (v && text_contains(search, v, needle)) return true;
        }
    }
    return false;
}

static bool text_matches(const query *q, record *rec, const collate *search)
{
    const char *p = q->text;

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        const char *start = p;
        while (*p && *p != ' ') p++;

        if (!record_has_word(rec, start, (size_t)(p - start), search))
            return false;
    }
    return true;
}

/* --- Messen -------------------------------------------------------------------- */

bool query_matches(const query *q, record *rec, const collate *search)
{
    frontmatter *fm = record_fields(rec);

    for (int i = 0; i < q->filter_count; i++)
        if (!filter_matches(&q->filters[i], fm, search)) return false;

    if (q->text[0] && !text_matches(q, rec, search)) return false;
    return true;
}

int query_compare(const query *q, record *a, record *b, const collate *sort)
{
    const char *field = q->order_field[0] ? q->order_field : "id";

    const char *va = frontmatter_get(record_fields(a), field);
    const char *vb = frontmatter_get(record_fields(b), field);

    bool ea = !va || !*va;
    bool eb = !vb || !*vb;

    /* Leere Felder ans Ende, und zwar in beiden Richtungen. Ein Datensatz ohne
     * Fälligkeitsdatum gehört nicht an den Anfang der Liste, nur weil jemand
     * absteigend sortiert. Deshalb steht diese Entscheidung VOR der
     * Umkehrung. */
    if (ea || eb) {
        if (ea && eb) return 0;
        return ea ? 1 : -1;
    }

    int r = sort ? collate_compare(sort, va, vb) : strcmp(va, vb);
    if (r > 1)  r = 1;
    if (r < -1) r = -1;

    return q->descending ? -r : r;
}
