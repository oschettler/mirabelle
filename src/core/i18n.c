/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe i18n.h für den Vertrag und data/lang/de.strings für das Format und die
 * echten Texte.
 *
 * Der Parser folgt keymap.c: jeder Fehler bekommt eine Meldung
 * "datei:zeile: meldung", und bei einem Fehler wird kein halbfertiger
 * Katalog zurückgegeben.
 */
#include "i18n.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define I18N_KEY_MAX  96    /* längster Schlüssel */
#define I18N_TEXT_MAX 512   /* längster Text, in Byte */

typedef struct {
    char key[I18N_KEY_MAX + 1];
    char text[I18N_TEXT_MAX + 1];
    int  line;               /* für Fehlermeldungen bei doppeltem Schlüssel */
} catalog_entry;

struct catalog {
    catalog_entry *entries;
    size_t         count;
    size_t         cap;
};

/* --- Meldungen --------------------------------------------------------------- */

/* Wie fail() in keymap.c: baut "datei:zeile: meldung" und liefert immer true,
 * damit Aufrufer direkt "return fail(...);" schreiben können. */
static bool fail(char *err, size_t err_size, const char *path, int line,
                  const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    if (err && err_size > 0)
        snprintf(err, err_size, "%s:%d: %s", path, line, msg);

    return true;
}

/* --- Wachsende Liste ----------------------------------------------------------- */

static bool entries_push(catalog *c, catalog_entry e)
{
    if (c->count == c->cap) {
        size_t         newcap = c->cap ? c->cap * 2 : 16;
        catalog_entry *p      = realloc(c->entries, newcap * sizeof *p);
        if (!p) return false;
        c->entries = p;
        c->cap     = newcap;
    }
    c->entries[c->count++] = e;
    return true;
}

/* --- Zeile lesen ----------------------------------------------------------------
 *
 * Wie read_line() in keymap.c: eine Zeile ohne Zeilenende in einen wachsenden
 * Puffer, byteweise - so wird nie mitten in einem Mehrbytezeichen abgeschnitten,
 * weil jede Zeile vollständig bis zum Zeilenende gelesen wird. false am echten
 * Dateiende. */
static bool read_line(FILE *f, char **buf, size_t *cap, int *line_no)
{
    size_t len     = 0;
    bool   got_any = false;
    int    c;

    for (;;) {
        c = fgetc(f);
        if (c == EOF) {
            if (!got_any) return false;
            break;
        }
        got_any = true;
        if (c == '\n') break;

        if (len + 2 > *cap) {
            size_t newcap = (*cap == 0) ? 128 : (*cap * 2);
            char  *p      = realloc(*buf, newcap);
            if (!p) return false;
            *buf = p;
            *cap = newcap;
        }
        (*buf)[len++] = (char)c;
    }

    (*buf)[len] = '\0';
    if (len > 0 && (*buf)[len - 1] == '\r') (*buf)[len - 1] = '\0';
    (*line_no)++;
    return true;
}

/* Entfernt ASCII-Leerraum (Leerzeichen, Tabulator) an beiden Enden von
 * [*start, *end). */
static void trim(const char **start, const char **end)
{
    while (*start < *end && (**start == ' ' || **start == '\t')) (*start)++;
    while (*end > *start && ((*end)[-1] == ' ' || (*end)[-1] == '\t')) (*end)--;
}

static bool is_key_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
}

/* Nach dem Laden sortiert i18n_load die Einträge nach Schlüssel, damit
 * find_entry binär suchen kann, wie font_find es für Zeichensätze tut. */
static int compare_entries(const void *a, const void *b)
{
    return strcmp(((const catalog_entry *)a)->key, ((const catalog_entry *)b)->key);
}

static void sort_entries(catalog *c)
{
    qsort(c->entries, c->count, sizeof *c->entries, compare_entries);
}

/* --- Laden ------------------------------------------------------------------- */

catalog *i18n_load(const char *path, char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    FILE *f = fopen(path, "rb");
    if (!f) {
        fail(err, err_size, path, 0, "Datei kann nicht geöffnet werden: %s",
             strerror(errno));
        return NULL;
    }

    catalog *c = calloc(1, sizeof *c);
    if (!c) {
        fclose(f);
        fail(err, err_size, path, 0, "Speicher reicht nicht");
        return NULL;
    }

    char  *line     = NULL;
    size_t line_cap = 0;
    int    line_no  = 0;

    while (read_line(f, &line, &line_cap, &line_no)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        const char *lstart = line;
        const char *lend   = line + strlen(line);
        trim(&lstart, &lend);
        if (lstart == lend) continue;   /* Leerzeile oder reiner Kommentar */

        char *eq = strchr(line, '=');
        if (!eq) {
            fail(err, err_size, path, line_no, "Gleichheitszeichen fehlt");
            free(line);
            fclose(f);
            i18n_free(c);
            return NULL;
        }

        const char *kstart = line, *kend = eq;
        trim(&kstart, &kend);
        const char *tstart = eq + 1, *tend = line + strlen(line);
        trim(&tstart, &tend);

        size_t klen = (size_t)(kend - kstart);
        size_t tlen = (size_t)(tend - tstart);

        if (klen == 0) {
            fail(err, err_size, path, line_no, "Schlüssel ist leer");
            free(line);
            fclose(f);
            i18n_free(c);
            return NULL;
        }
        if (klen > I18N_KEY_MAX) {
            fail(err, err_size, path, line_no,
                 "Schlüssel zu lang (höchstens %d Zeichen)", I18N_KEY_MAX);
            free(line);
            fclose(f);
            i18n_free(c);
            return NULL;
        }
        for (size_t i = 0; i < klen; i++) {
            if (!is_key_char(kstart[i])) {
                fail(err, err_size, path, line_no,
                     "ungültiges Zeichen im Schlüssel '%.*s'", (int)klen, kstart);
                free(line);
                fclose(f);
                i18n_free(c);
                return NULL;
            }
        }
        if (tlen > I18N_TEXT_MAX) {
            fail(err, err_size, path, line_no,
                 "Text zu lang (höchstens %d Byte)", I18N_TEXT_MAX);
            free(line);
            fclose(f);
            i18n_free(c);
            return NULL;
        }

        catalog_entry e = {0};
        memcpy(e.key, kstart, klen);
        e.key[klen] = '\0';
        memcpy(e.text, tstart, tlen);
        e.text[tlen] = '\0';
        e.line = line_no;

        for (size_t i = 0; i < c->count; i++) {
            if (strcmp(c->entries[i].key, e.key) == 0) {
                fail(err, err_size, path, line_no,
                     "Schlüssel '%s' ist bereits in Zeile %d vergeben",
                     e.key, c->entries[i].line);
                free(line);
                fclose(f);
                i18n_free(c);
                return NULL;
            }
        }

        if (!entries_push(c, e)) {
            fail(err, err_size, path, line_no, "Speicher reicht nicht");
            free(line);
            fclose(f);
            i18n_free(c);
            return NULL;
        }
    }

    bool had_error = ferror(f) != 0;
    fclose(f);
    free(line);

    if (had_error) {
        fail(err, err_size, path, 0, "Datei nicht lesbar: Lesefehler");
        i18n_free(c);
        return NULL;
    }

    sort_entries(c);
    return c;
}

void i18n_free(catalog *c)
{
    if (!c) return;
    free(c->entries);
    free(c);
}

int i18n_count(const catalog *c)
{
    return c ? (int)c->count : 0;
}

/* --- Nachschlagen -------------------------------------------------------------
 *
 * Die Einträge liegen nach dem Laden sortiert vor (siehe sort_entries oben),
 * gesucht wird binär wie in font_find. */

static const catalog_entry *find_entry(const catalog *c, const char *key)
{
    if (!c) return NULL;

    int lo = 0, hi = (int)c->count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcmp(c->entries[mid].key, key);
        if (cmp == 0) return &c->entries[mid];
        if (cmp < 0) lo = mid + 1;
        else         hi = mid - 1;
    }
    return NULL;
}

bool i18n_has(const catalog *c, const char *key)
{
    return find_entry(c, key) != NULL;
}

const char *T(const catalog *c, const char *key)
{
    const catalog_entry *e = find_entry(c, key);
    return e ? e->text : key;
}

/* --- Platzhalter ---------------------------------------------------------------
 *
 * Hängt len Byte aus s an out an, sofern noch Platz samt Nullbyte ist. */
static bool append(char *out, size_t out_size, size_t *pos, const char *s, size_t len)
{
    if (*pos + len + 1 > out_size) return false;
    memcpy(out + *pos, s, len);
    *pos += len;
    return true;
}

/* Setzt {0}, {1}, ... in text aus args ein. Ein Platzhalter ohne passendes
 * Argument, ein { ohne schließende Klammer oder mit etwas anderem als Ziffern
 * darin bleibt unverändert stehen. */
static bool format_text(const char *text, char *out, size_t out_size,
                         const char *const *args, int argc)
{
    if (out_size == 0) return false;

    size_t pos = 0;
    for (const char *p = text; *p; ) {
        if (*p == '{') {
            const char *start = p + 1;
            const char *q     = start;
            while (*q >= '0' && *q <= '9') q++;

            if (q > start && *q == '}') {
                int idx = 0;   /* wächst nicht über 9 hinaus - mehr wird ohnehin nie gebraucht */
                for (const char *d = start; d < q; d++)
                    if (idx <= 9) idx = idx * 10 + (*d - '0');

                if (idx <= 9 && idx < argc) {
                    if (!append(out, out_size, &pos, args[idx], strlen(args[idx])))
                        goto too_small;
                } else {
                    if (!append(out, out_size, &pos, p, (size_t)(q + 1 - p)))
                        goto too_small;
                }
                p = q + 1;
                continue;
            }
            if (!append(out, out_size, &pos, p, 1)) goto too_small;
            p++;
            continue;
        }
        if (!append(out, out_size, &pos, p, 1)) goto too_small;
        p++;
    }

    out[pos] = '\0';
    return true;

too_small:
    out[0] = '\0';
    return false;
}

bool Tf(const catalog *c, const char *key, char *out, size_t out_size,
        const char *const *args, int argc)
{
    return format_text(T(c, key), out, out_size, args, argc);
}

bool Tn(const catalog *c, const char *key, int n, char *out, size_t out_size)
{
    char sub[I18N_KEY_MAX + 8];   /* Schlüssel + ".other" */
    snprintf(sub, sizeof sub, "%s.%s", key, n == 1 ? "one" : "other");

    const catalog_entry *e = find_entry(c, sub);
    if (e) {
        char        num[16];
        snprintf(num, sizeof num, "%d", n);
        const char *args[1] = { num };
        return format_text(e->text, out, out_size, args, 1);
    }

    /* Fehlt die passende Variante: wie T verfahren und den Schlüssel selbst
     * liefern, trotzdem true - die Zahl fehlt dann eben in der Anzeige. */
    size_t klen = strlen(key);
    if (out_size == 0 || klen + 1 > out_size) {
        if (out_size > 0) out[0] = '\0';
        return false;
    }
    memcpy(out, key, klen + 1);
    return true;
}
