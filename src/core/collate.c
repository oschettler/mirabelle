/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe collate.h für den Vertrag und data/lang/de.sort für eine Tabelle.
 *
 * Der Parser folgt keymap.c und i18n.c: jeder Fehler bekommt eine Meldung
 * "datei:zeile: meldung", und bei einem Fehler kommt keine halbfertige Tabelle
 * zurück.
 *
 * Verglichen wird ohne Puffer. Statt beide Texte erst zu falten und dann die
 * Ergebnisse zu vergleichen, laufen zwei Falter nebeneinander her und liefern
 * je ein Byte. Das spart nicht nur die Zuteilung - es nimmt auch die Frage
 * weg, wie groß der Puffer sein müsste. „ß" wird zu zwei Zeichen, und wer
 * einmal mit einer festen Obergrenze anfängt, schneidet irgendwann einen
 * Namen ab.
 */
#include "core/collate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/lines.h"
#include "core/utf8.h"

#define COLLATE_REPL_MAX 15   /* längster Ersatz in Byte, ohne Nullbyte */

typedef struct {
    uint32_t cp;                          /* das ersetzte Zeichen */
    char     repl[COLLATE_REPL_MAX + 1];  /* wodurch */
    int      line;                        /* für die Meldung bei Dopplung */
} collate_entry;

struct collate {
    collate_entry *entries;   /* nach cp sortiert, damit binär gesucht werden kann */
    size_t         count;
    size_t         cap;
};

/* --- Nachschlagen ------------------------------------------------------------
 *
 * Die Einträge liegen nach Codepunkt sortiert, also binäre Suche. Bei
 * dreißig Zeilen wäre das gleichgültig; die Funktion läuft aber je Zeichen und
 * je Vergleich, und ein Sortierlauf über tausend Namen ruft sie oft genug auf,
 * dass es sich lohnt.
 */
static const char *lookup(const collate *c, uint32_t cp)
{
    size_t lo = 0, hi = c->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (c->entries[mid].cp == cp) return c->entries[mid].repl;
        if (c->entries[mid].cp < cp)  lo = mid + 1;
        else                          hi = mid;
    }
    return NULL;
}

/* --- Der Falter ---------------------------------------------------------------
 *
 * Liefert die gefaltete Eingabe Byte für Byte: erst den Rest dessen, was das
 * letzte Zeichen ergeben hat, dann das nächste Zeichen.
 */

typedef struct {
    const collate *c;
    const char    *p;         /* Rest der Eingabe */

    /* Zwei getrennte Quellen für das nächste Byte, und das mit Absicht:
     * matches_here() kopiert einen Falter, um von einer Stelle aus
     * probeweise weiterzulaufen. Läge der Ersatztext manchmal im eigenen
     * Puffer und manchmal woanders, zeigte die Kopie noch auf den Puffer des
     * Originals - und läse ab dem nächsten Zeichen fremde Bytes.
     *
     * repl zeigt immer in die Tabelle, die unverändert liegen bleibt. raw
     * liegt im Falter selbst und wird beim Kopieren mitkopiert. Beide sind
     * damit für sich gültig. */
    const char *repl;         /* Rest eines Ersatzes, oder NULL */
    char        raw[5];       /* ein durchgereichtes Zeichen, nullterminiert */
    int         raw_pos;      /* nächstes Byte darin, -1 wenn keins ansteht */
} folder;

static void fold_start(folder *f, const collate *c, const char *utf8)
{
    f->c       = c;
    f->p       = utf8;
    f->repl    = NULL;
    f->raw[0]  = '\0';
    f->raw_pos = -1;
}

/* Nächstes Byte der gefalteten Fassung, oder -1 am Ende. */
static int fold_next(folder *f)
{
    for (;;) {
        if (f->raw_pos >= 0) {
            int b = (unsigned char)f->raw[f->raw_pos];
            if (b) { f->raw_pos++; return b; }
            f->raw_pos = -1;
        }

        if (f->repl) {
            if (*f->repl) return (unsigned char)*f->repl++;
            f->repl = NULL;
        }

        if (!*f->p) return -1;

        uint32_t cp = utf8_next(&f->p);

        const char *r = lookup(f->c, cp);
        if (r) {
            f->repl = r;
            continue;
        }

        /* Die einzige Regel, die nicht aus der Tabelle kommt - und sie greift
         * nur, weil die Tabelle nichts gesagt hat. */
        if (cp >= 'A' && cp <= 'Z') cp += 'a' - 'A';

        int n = utf8_encode(cp, f->raw);
        if (n <= 0) continue;   /* kann nicht vorkommen, siehe utf8.h */
        f->raw[n]  = '\0';
        f->raw_pos = 0;
    }
}

/* --- Laden ------------------------------------------------------------------- */

static bool add_entry(collate *c, uint32_t cp, const char *repl, int line,
                      char *err, size_t err_size, const linereader *r)
{
    if (c->count == c->cap) {
        size_t         newcap = c->cap ? c->cap * 2 : 32;
        collate_entry *e      = realloc(c->entries, newcap * sizeof *e);
        if (!e) return lines_fail(r, err, err_size, "kein Speicher");
        c->entries = e;
        c->cap     = newcap;
    }

    /* Einsortieren statt anhängen und später sortieren: die Tabellen sind
     * klein, und so ist die Dopplungsprüfung gleich miterledigt. */
    size_t i = 0;
    while (i < c->count && c->entries[i].cp < cp) i++;

    if (i < c->count && c->entries[i].cp == cp)
        return lines_fail(r, err, err_size,
                          "Zeichen schon in Zeile %d belegt", c->entries[i].line);

    memmove(&c->entries[i + 1], &c->entries[i],
            (c->count - i) * sizeof *c->entries);

    c->entries[i].cp   = cp;
    c->entries[i].line = line;
    snprintf(c->entries[i].repl, sizeof c->entries[i].repl, "%s", repl);
    c->count++;
    return true;
}

collate *collate_load(const char *path, char *err, size_t err_size)
{
    linereader r;
    if (!lines_open(&r, path, err, err_size)) return NULL;

    collate *c = calloc(1, sizeof *c);
    if (!c) {
        lines_close(&r);
        lines_fail_file(path, err, err_size, "kein Speicher");
        return NULL;
    }

    bool ok = true;
    while (ok && lines_next(&r)) {
        /* Links genau ein Zeichen, rechts sein Ersatz. Mehr Wörter sind ein
         * Tippfehler und kein Ersatz mit Leerzeichen darin - ein solcher wäre
         * in einer Sortiertabelle sinnlos. */
        if (r.count != 2) {
            ok = lines_fail(&r, err, err_size,
                            r.count < 2 ? "der Ersatz fehlt"
                                        : "genau zwei Angaben je Zeile");
            break;
        }

        const char *left = r.word[0];
        uint32_t    cp   = utf8_next(&left);

        if (*left) {
            ok = lines_fail(&r, err, err_size,
                            "links muss genau ein Zeichen stehen");
            break;
        }
        if (cp == 0 || cp == UTF8_REPLACEMENT) {
            ok = lines_fail(&r, err, err_size, "kein gültiges Zeichen");
            break;
        }
        if (strlen(r.word[1]) > COLLATE_REPL_MAX) {
            ok = lines_fail(&r, err, err_size,
                            "der Ersatz ist länger als %d Byte", COLLATE_REPL_MAX);
            break;
        }

        ok = add_entry(c, cp, r.word[1], r.line, err, err_size, &r);
    }

    lines_close(&r);
    if (!ok) {
        collate_free(c);
        return NULL;
    }
    return c;
}

void collate_free(collate *c)
{
    if (!c) return;
    free(c->entries);
    free(c);
}

/* --- Falten, vergleichen, suchen ---------------------------------------------- */

size_t collate_fold(const collate *c, const char *utf8, char *out, size_t out_size)
{
    folder f;
    fold_start(&f, c, utf8);

    size_t n = 0;
    for (int b = fold_next(&f); b >= 0; b = fold_next(&f)) {
        if (n + 1 >= out_size) return (size_t)-1;
        out[n++] = (char)b;
    }

    if (n >= out_size) return (size_t)-1;
    out[n] = '\0';
    return n;
}

int collate_compare(const collate *c, const char *a, const char *b)
{
    folder fa, fb;
    fold_start(&fa, c, a);
    fold_start(&fb, c, b);

    for (;;) {
        int ca = fold_next(&fa);
        int cb = fold_next(&fb);
        if (ca != cb) return ca < cb ? -1 : 1;

        /* An dieser Stelle sind beide gleich, also endet die Schleife, wenn
         * einer von beiden -1 liefert. Ein Nullbyte kann dort nicht stehen:
         * die Eingabe ist eine C-Zeichenkette, und die Ersetzungen sind es
         * auch. */
        if (ca < 0) break;
    }

    /* Gefaltet gleich - dann entscheidet der ungefaltete Text, damit die
     * Reihenfolge überhaupt festliegt. */
    int raw = strcmp(a, b);
    return raw < 0 ? -1 : (raw > 0 ? 1 : 0);
}

/* Prüft, ob der gefaltete needle an der Stelle beginnt, an der hay gerade
 * steht. hay wird dabei kopiert, damit der Aufrufer weiterlaufen kann. */
static bool matches_here(folder hay, const collate *c, const char *needle)
{
    folder nee;
    fold_start(&nee, c, needle);

    for (;;) {
        int cn = fold_next(&nee);
        if (cn < 0) return true;          /* Nadel zu Ende: gefunden */
        if (fold_next(&hay) != cn) return false;
    }
}

bool collate_starts_with(const collate *c, const char *text, const char *prefix)
{
    folder f;
    fold_start(&f, c, text);
    return matches_here(f, c, prefix);
}

bool collate_contains(const collate *c, const char *haystack, const char *needle)
{
    /* Reihum an jeder Stelle des Heuhaufens ansetzen. Das ist der einfachste
     * Suchalgorithmus, den es gibt, und für Namen und Notizzeilen genau
     * richtig - ein Boyer-Moore auf einem gefalteten Strom wäre mehr Code als
     * Nutzen. */
    folder f;
    fold_start(&f, c, haystack);

    for (;;) {
        if (matches_here(f, c, needle)) return true;
        if (fold_next(&f) < 0) return false;
    }
}
