/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Ein Leser für zeilenweise Datendateien.
 *
 * Drei Formate im Projekt haben dieselbe Gestalt: eine Zeile, mehrere durch
 * Leerraum getrennte Wörter, `#` leitet einen Kommentar ein, Leerzeilen zählen
 * nicht.
 *
 *     data/themes/desktop.theme    titlebar_h  20
 *     data/keys/default.keys       app.quit    Cmd+Q   global
 *     data/lang/de.sort            ä           a
 *
 * Vor diesem Leser hatte jedes davon seine eigene Schleife, seinen eigenen
 * Zeilenzähler und seine eigene Meldung. Dreimal dasselbe, dreimal die
 * Gelegenheit, es unterschiedlich zu machen — und genau das war passiert: das
 * eine erlaubte Kommentare mitten in der Zeile, das andere nicht.
 *
 * Was hier NICHT hineingehört: der Textkatalog (`schlüssel = text`, der Wert
 * darf Leerzeichen enthalten) und die Schemadateien (verschachtelt). Ein
 * Leser, der auch die noch könnte, wäre kein gemeinsamer Leser mehr, sondern
 * ein kleines YAML.
 */
#ifndef PDA_CORE_LINES_H
#define PDA_CORE_LINES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define LINES_MAX_WORDS 8
#define LINES_MAX_LEN   512

typedef struct {
    const char *path;      /* für Meldungen; gehört dem Aufrufer */
    FILE       *fp;
    int         line;      /* Nummer der zuletzt gelesenen Zeile, ab 1 */

    char  buf[LINES_MAX_LEN];
    char *word[LINES_MAX_WORDS];
    int   count;           /* Wörter in dieser Zeile, mindestens 1 */
} linereader;

/* Öffnet die Datei. Bei einem Fehler false und eine Meldung "datei: text". */
bool lines_open(linereader *r, const char *path, char *err, size_t err_size);
void lines_close(linereader *r);

/* Liest die nächste Zeile, die etwas enthält, und zerlegt sie in Wörter.
 * false am Dateiende.
 *
 * Mehr als LINES_MAX_WORDS Wörter werden abgeschnitten - der Aufrufer prüft
 * ohnehin, wie viele er erwartet, und meldet den Rest als Fehler. */
bool lines_next(linereader *r);

/* Baut "datei:zeile: meldung" nach err und liefert immer false, damit
 * Aufrufer `return lines_fail(...)` schreiben können. */
bool lines_fail(const linereader *r, char *err, size_t err_size,
                const char *fmt, ...);

/* Wie lines_fail, aber ohne Zeilennummer - für Fehler, die die ganze Datei
 * betreffen. */
bool lines_fail_file(const char *path, char *err, size_t err_size,
                     const char *fmt, ...);

#endif /* PDA_CORE_LINES_H */
