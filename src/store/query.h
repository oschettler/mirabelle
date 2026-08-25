/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Eine Abfrage ist eine Datenstruktur, keine Zeichenkette.
 *
 * Anwendungen bauen nie SQL zusammen. Sie füllen eine `query` und geben sie
 * weiter; wer sie ausführt, entscheidet selbst, wie. Das hat drei Gründe, und
 * alle drei sind wichtiger als die paar Zeilen, die es kostet:
 *
 *   1. Es gibt keine Anführungszeichen zu setzen und damit auch keine
 *      Möglichkeit, sie zu vergessen. Ein Name mit einem Apostroph ist ein
 *      Name, kein Syntaxfehler.
 *   2. Der Index ist abgeleitet (D-3). Wer `index.db` löscht, darf nichts
 *      verlieren - also muss dieselbe Abfrage auch ohne Index laufen, direkt
 *      über die Datensätze. Genau das macht query_matches(). Stünde in der
 *      Anwendung SQL, ginge das nicht.
 *   3. Eine Abfrage lässt sich prüfen, anzeigen und speichern, ohne dass eine
 *      Datenbank in der Nähe ist. Die Tests hier brauchen keine.
 *
 * Aus derselben Struktur baut store/index.c später SQL - an genau einer
 * Stelle im ganzen Programm.
 *
 * Alle Größen sind fest. Eine Abfrage ist ein kleines, kurzlebiges Ding; sie
 * auf den Stapel legen zu können ist mehr wert als beliebig viele Bedingungen.
 */
#ifndef PDA_STORE_QUERY_H
#define PDA_STORE_QUERY_H

#include <stdbool.h>
#include <stddef.h>

#include "core/collate.h"
#include "store/record.h"

#define QUERY_FIELD_MAX   48
#define QUERY_VALUE_MAX  128
#define QUERY_FILTERS_MAX  8

typedef enum {
    QF_EQUALS,     /* Feld ist genau dieser Wert - ungefaltet, Zeichen für Zeichen */
    QF_CONTAINS,   /* Feld enthält den Wert, gefaltet: „muller" findet „Müller" */
    QF_PREFIX,     /* Feld beginnt mit dem Wert, gefaltet */
    QF_LESS,       /* Feld ist kleiner als der Wert */
    QF_GREATER,    /* Feld ist größer als der Wert */
    QF_PRESENT,    /* Feld ist überhaupt gesetzt; value bleibt leer */
    QF_ABSENT      /* Feld fehlt oder ist leer */
} query_op;

/* Warum QF_LESS und QF_GREATER für Daten reichen:
 *
 * Datumsfelder stehen als JJJJ-MM-TT im Front Matter, und in dieser Form ist
 * die alphabetische Reihenfolge zugleich die zeitliche. „Fällig vor dem
 * 1. März" ist damit ein Zeichenkettenvergleich, und es braucht weder einen
 * Datumstyp im Speicher noch eine Umrechnung beim Abfragen. Dieselbe
 * Eigenschaft trägt schon die Kennung eines Datensatzes (record.h).
 */

typedef struct {
    char     field[QUERY_FIELD_MAX];
    query_op op;
    char     value[QUERY_VALUE_MAX];
} query_filter;

typedef struct {
    char         collection[QUERY_FIELD_MAX];   /* leer heißt: alle */
    query_filter filters[QUERY_FILTERS_MAX];
    int          filter_count;

    /* Volltext. Alle Wörter müssen vorkommen, irgendwo im Datensatz - in
     * einem Feld oder im Körper. Leer heißt: kein Volltextteil. */
    char text[QUERY_VALUE_MAX];

    char order_field[QUERY_FIELD_MAX];   /* leer heißt: nach Kennung */
    bool descending;

    int limit;    /* 0 heißt: alle */
    int offset;
} query;

/* Setzt alles zurück und legt die Sammlung fest. collection darf NULL sein. */
void query_init(query *q, const char *collection);

/* Hängt eine Bedingung an. false, wenn kein Platz mehr ist oder Feld oder Wert
 * zu lang sind - abgeschnitten wird nichts, das ergäbe eine Abfrage, die
 * stillschweigend etwas anderes sucht.
 *
 * Bei QF_PRESENT und QF_ABSENT wird value nicht beachtet und darf NULL sein. */
bool query_where(query *q, const char *field, query_op op, const char *value);

/* Setzt den Volltextteil. NULL oder leer schaltet ihn ab. false, wenn zu lang. */
bool query_text(query *q, const char *words);

/* Sortierung. field NULL oder leer heißt: nach Kennung, also zeitlich. */
bool query_order(query *q, const char *field, bool descending);

void query_limit(query *q, int limit, int offset);

/* --- Ausführen ohne Index ----------------------------------------------------
 *
 * Der Weg, der immer funktioniert: über die Datensätze laufen und jeden
 * fragen. Langsamer als ein Index, aber er braucht keinen - und das ist die
 * Zusage aus D-3.
 */

/* true, wenn rec alle Bedingungen und den Volltextteil erfüllt.
 *
 * search ist die Faltungstabelle für die Suche (collate.h). Sie darf NULL
 * sein; dann wird ungefaltet verglichen, und „Muller" findet „Müller" nicht
 * mehr. Für Tests ist das gelegentlich das, was man will. */
bool query_matches(const query *q, record *rec, const collate *search);

/* Vergleicht zwei Datensätze nach der Sortierung der Abfrage, wie strcmp.
 *
 * Fehlt das Sortierfeld in einem Datensatz, kommt er ans Ende - unabhängig von
 * der Richtung. Ein leeres Fälligkeitsdatum gehört nicht an den Anfang der
 * Liste, nur weil man umgekehrt sortiert.
 *
 * sort ist die Sortiertabelle (collate.h) und darf NULL sein. */
int query_compare(const query *q, record *a, record *b, const collate *sort);

#endif /* PDA_STORE_QUERY_H */
