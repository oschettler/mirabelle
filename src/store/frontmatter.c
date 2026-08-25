/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe frontmatter.h für den Vertrag und das Format.
 *
 * Der Parser folgt i18n.c: jeder Fehler bekommt eine Meldung
 * "datei:zeile: meldung", und bei einem Fehler wird keine halbfertige
 * Struktur zurückgegeben.
 *
 * Anders als i18n.c braucht es hier keine binäre Suche - ein Front-Matter-
 * Block hat eine Handvoll Einträge, eine lineare Suche reicht.
 */
#include "frontmatter.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FM_KEY_MAX   64    /* längster Schlüssel, in Zeichen */
#define FM_VALUE_MAX 512   /* längster Wert (auch je Listeneintrag), in Byte */

typedef struct {
    char    key[FM_KEY_MAX + 1];
    char  **values;        /* dynamisch angelegte, einzeln freizugebende Zeichenketten */
    size_t  value_count;
    bool    is_list;        /* stand der Wert in "[...]"? entscheidet, wie geschrieben wird */
    int     line;            /* für die Meldung bei doppeltem Schlüssel */
} fm_entry;

struct frontmatter {
    fm_entry *entries;
    size_t    count;
    size_t    cap;
};

/* --- Meldungen ---------------------------------------------------------------- */

static void fail(char *err, size_t err_size, const char *name, int line,
                  const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    if (err && err_size > 0)
        snprintf(err, err_size, "%s:%d: %s", name, line, msg);
}

/* --- Wachsende Listen ----------------------------------------------------------- */

static bool entries_push(frontmatter *fm, fm_entry e)
{
    if (fm->count == fm->cap) {
        size_t    newcap = fm->cap ? fm->cap * 2 : 8;
        fm_entry *p       = realloc(fm->entries, newcap * sizeof *p);
        if (!p) return false;
        fm->entries = p;
        fm->cap     = newcap;
    }
    fm->entries[fm->count++] = e;
    return true;
}

/* Übernimmt value in jedem Fall - bei einem Fehlschlag wird es freigegeben,
 * der Aufrufer muss es dann nicht mehr anfassen. */
static bool list_append(fm_entry *e, char *value)
{
    char **p = realloc(e->values, (e->value_count + 1) * sizeof *p);
    if (!p) {
        free(value);
        return false;
    }
    e->values = p;
    e->values[e->value_count++] = value;
    return true;
}

static void free_entry_values(fm_entry *e)
{
    for (size_t i = 0; i < e->value_count; i++) free(e->values[i]);
    free(e->values);
    e->values      = NULL;
    e->value_count = 0;
}

static bool dup_range(const char *s, size_t n, char **out)
{
    char *p = malloc(n + 1);
    if (!p) return false;
    if (n) memcpy(p, s, n);
    p[n]  = '\0';
    *out = p;
    return true;
}

/* --- Kleine Textbausteine -------------------------------------------------------- */

/* Entfernt ASCII-Leerraum (Leerzeichen, Tabulator) an beiden Enden von
 * [*start, *end). Wie trim() in i18n.c. */
static void trim(const char **start, const char **end)
{
    while (*start < *end && (**start == ' ' || **start == '\t')) (*start)++;
    while (*end > *start && ((*end)[-1] == ' ' || (*end)[-1] == '\t')) (*end)--;
}

static bool is_key_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/* Liefert die nächste Zeile ab *pos, ohne Zeilenende (LF oder CRLF).
 * *pos zeigt danach auf den Anfang der nächsten Zeile, oder auf len am
 * Textende. false, wenn *pos bereits am Textende steht. */
static bool next_line(const char *text, size_t len, size_t *pos,
                       size_t *start, size_t *line_len)
{
    if (*pos >= len) return false;

    *start = *pos;
    size_t i = *pos;
    while (i < len && text[i] != '\n') i++;

    size_t end = i;
    if (end > *start && text[end - 1] == '\r') end--;

    *line_len = end - *start;
    *pos      = (i < len) ? i + 1 : len;
    return true;
}

static bool is_delimiter(const char *line, size_t line_len)
{
    return line_len == 3 && memcmp(line, "---", 3) == 0;
}

/* --- Liste im Wert --------------------------------------------------------------
 *
 * content ist der Text zwischen den eckigen Klammern, nicht nullterminiert. */
static bool parse_list(const char *content, size_t content_len, fm_entry *e,
                        const char *name, int line_no, char *err, size_t err_size)
{
    if (content_len == 0) return true;   /* "[]" - die leere Liste */

    const char *p   = content;
    const char *end = content + content_len;

    for (;;) {
        const char *comma    = memchr(p, ',', (size_t)(end - p));
        const char *item_end = comma ? comma : end;

        const char *istart = p, *iend = item_end;
        trim(&istart, &iend);
        size_t ilen = (size_t)(iend - istart);

        if (ilen > FM_VALUE_MAX) {
            fail(err, err_size, name, line_no,
                 "Wert zu lang (höchstens %d Byte)", FM_VALUE_MAX);
            free_entry_values(e);
            return false;
        }

        char *v;
        if (!dup_range(istart, ilen, &v)) {
            fail(err, err_size, name, line_no, "Speicher reicht nicht");
            free_entry_values(e);
            return false;
        }
        if (!list_append(e, v)) {
            fail(err, err_size, name, line_no, "Speicher reicht nicht");
            free_entry_values(e);
            return false;
        }

        if (!comma) return true;
        p = comma + 1;
    }
}

/* --- Parsen ---------------------------------------------------------------------- */

frontmatter *frontmatter_parse(const char *text, size_t len, const char *name,
                                size_t *body_offset, char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';
    if (!name) name = "";
    if (body_offset) *body_offset = 0;

    size_t pos = 0, start, line_len;

    if (!next_line(text, len, &pos, &start, &line_len) ||
        !is_delimiter(text + start, line_len)) {
        /* Kein "---" am Anfang: kein Front Matter, kein Fehler. */
        frontmatter *fm = calloc(1, sizeof *fm);
        if (!fm) fail(err, err_size, name, 0, "Speicher reicht nicht");
        return fm;
    }

    frontmatter *fm = calloc(1, sizeof *fm);
    if (!fm) {
        fail(err, err_size, name, 0, "Speicher reicht nicht");
        return NULL;
    }

    int  line_no = 1;
    bool closed  = false;

    while (next_line(text, len, &pos, &start, &line_len)) {
        line_no++;
        const char *lstart = text + start;

        if (is_delimiter(lstart, line_len)) {
            closed = true;
            if (body_offset) *body_offset = pos;
            break;
        }

        const char *tstart = lstart, *tend = lstart + line_len;
        trim(&tstart, &tend);
        if (tstart == tend) continue;   /* Leerzeile im Block */

        bool        starts_ws = line_len > 0 &&
                                 (lstart[0] == ' ' || lstart[0] == '\t');
        const char *colon     = memchr(lstart, ':', line_len);

        if (starts_ws && colon) {
            fail(err, err_size, name, line_no,
                 "verschachteltes Mapping - verschachtelte Strukturen werden "
                 "nicht unterstützt");
            frontmatter_free(fm);
            return NULL;
        }
        if (!colon) {
            fail(err, err_size, name, line_no, "Doppelpunkt fehlt");
            frontmatter_free(fm);
            return NULL;
        }

        const char *kstart = lstart, *kend = colon;
        trim(&kstart, &kend);
        const char *vstart = colon + 1, *vend = lstart + line_len;
        trim(&vstart, &vend);

        size_t klen = (size_t)(kend - kstart);
        if (klen == 0) {
            fail(err, err_size, name, line_no, "Schlüssel ist leer");
            frontmatter_free(fm);
            return NULL;
        }
        if (klen > FM_KEY_MAX) {
            fail(err, err_size, name, line_no,
                 "Schlüssel zu lang (höchstens %d Zeichen)", FM_KEY_MAX);
            frontmatter_free(fm);
            return NULL;
        }
        for (size_t i = 0; i < klen; i++) {
            if (!is_key_char(kstart[i])) {
                fail(err, err_size, name, line_no,
                     "ungültiges Zeichen im Schlüssel '%.*s'", (int)klen, kstart);
                frontmatter_free(fm);
                return NULL;
            }
        }

        char key[FM_KEY_MAX + 1];
        memcpy(key, kstart, klen);
        key[klen] = '\0';

        for (size_t i = 0; i < fm->count; i++) {
            if (strcmp(fm->entries[i].key, key) == 0) {
                fail(err, err_size, name, line_no,
                     "Schlüssel '%s' ist bereits in Zeile %d vergeben",
                     key, fm->entries[i].line);
                frontmatter_free(fm);
                return NULL;
            }
        }

        fm_entry e = {0};
        memcpy(e.key, key, klen + 1);
        e.line = line_no;

        size_t vlen = (size_t)(vend - vstart);
        if (vlen >= 2 && vstart[0] == '[' && vstart[vlen - 1] == ']') {
            e.is_list = true;
            if (!parse_list(vstart + 1, vlen - 2, &e, name, line_no, err, err_size)) {
                frontmatter_free(fm);
                return NULL;
            }
        } else {
            if (vlen > FM_VALUE_MAX) {
                fail(err, err_size, name, line_no,
                     "Wert zu lang (höchstens %d Byte)", FM_VALUE_MAX);
                frontmatter_free(fm);
                return NULL;
            }
            char *v;
            if (!dup_range(vstart, vlen, &v) || !list_append(&e, v)) {
                fail(err, err_size, name, line_no, "Speicher reicht nicht");
                free_entry_values(&e);
                frontmatter_free(fm);
                return NULL;
            }
        }

        if (!entries_push(fm, e)) {
            fail(err, err_size, name, line_no, "Speicher reicht nicht");
            free_entry_values(&e);
            frontmatter_free(fm);
            return NULL;
        }
    }

    if (!closed) {
        fail(err, err_size, name, 1, "Block wird nie geschlossen");
        frontmatter_free(fm);
        return NULL;
    }

    return fm;
}

void frontmatter_free(frontmatter *fm)
{
    if (!fm) return;
    for (size_t i = 0; i < fm->count; i++) free_entry_values(&fm->entries[i]);
    free(fm->entries);
    free(fm);
}

int frontmatter_count(const frontmatter *fm)
{
    return fm ? (int)fm->count : 0;
}

const char *frontmatter_key_at(const frontmatter *fm, int i)
{
    if (!fm || i < 0 || (size_t)i >= fm->count) return NULL;
    return fm->entries[i].key;
}

/* --- Nachschlagen ----------------------------------------------------------------- */

static const fm_entry *find_entry(const frontmatter *fm, const char *key)
{
    if (!fm || !key) return NULL;
    for (size_t i = 0; i < fm->count; i++)
        if (strcmp(fm->entries[i].key, key) == 0) return &fm->entries[i];
    return NULL;
}

bool frontmatter_has(const frontmatter *fm, const char *key)
{
    return find_entry(fm, key) != NULL;
}

const char *frontmatter_get(const frontmatter *fm, const char *key)
{
    const fm_entry *e = find_entry(fm, key);
    if (!e || e->value_count == 0) return NULL;
    return e->values[0];
}

int frontmatter_list_count(const frontmatter *fm, const char *key)
{
    const fm_entry *e = find_entry(fm, key);
    return e ? (int)e->value_count : 0;
}

const char *frontmatter_list_at(const frontmatter *fm, const char *key, int i)
{
    const fm_entry *e = find_entry(fm, key);
    if (!e || i < 0 || (size_t)i >= e->value_count) return NULL;
    return e->values[i];
}

/* --- Schreiben ---------------------------------------------------------------------
 *
 * Der Block wird zunächst in einen wachsenden Puffer gebaut - wie read_line in
 * i18n.c, nur beim Schreiben statt beim Lesen. So gibt es nur einen einzigen
 * Formatierungsweg, keinen für die Größenmessung und keinen zweiten fürs
 * tatsächliche Schreiben, die sonst auseinanderlaufen könnten. */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} sbuf;

static bool sbuf_append(sbuf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t newcap = b->cap ? b->cap * 2 : 128;
        while (newcap < b->len + n + 1) newcap *= 2;
        char *p = realloc(b->data, newcap);
        if (!p) return false;
        b->data = p;
        b->cap  = newcap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

static bool sbuf_append_str(sbuf *b, const char *s)
{
    return sbuf_append(b, s, strlen(s));
}

bool frontmatter_write(const frontmatter *fm, char *out, size_t out_size,
                        size_t *needed)
{
    sbuf b = {0};
    bool ok = sbuf_append_str(&b, "---\n");

    for (size_t i = 0; ok && fm && i < fm->count; i++) {
        const fm_entry *e = &fm->entries[i];
        ok = sbuf_append_str(&b, e->key) && sbuf_append_str(&b, ": ");
        if (!ok) break;

        if (e->is_list) {
            ok = sbuf_append_str(&b, "[");
            for (size_t j = 0; ok && j < e->value_count; j++) {
                if (j) ok = sbuf_append_str(&b, ", ");
                if (ok) ok = sbuf_append_str(&b, e->values[j]);
            }
            if (ok) ok = sbuf_append_str(&b, "]");
        } else {
            ok = sbuf_append_str(&b, e->value_count ? e->values[0] : "");
        }
        if (ok) ok = sbuf_append_str(&b, "\n");
    }

    if (ok) ok = sbuf_append_str(&b, "---\n");

    if (!ok) {
        free(b.data);
        if (needed) *needed = 0;   /* Speicher reicht nicht - Größe unbekannt */
        return false;
    }

    if (needed) *needed = b.len + 1;

    if (out_size < b.len + 1) {
        free(b.data);
        return false;
    }

    memcpy(out, b.data, b.len + 1);
    free(b.data);
    return true;
}
