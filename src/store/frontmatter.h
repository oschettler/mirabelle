/* Front Matter: die typisierten Felder eines Datensatzes.
 *
 * Gemtext kennt keine Metadaten und keine Kommentare, deshalb steht am Anfang
 * einer Datei ein durch "---" abgetrennter Block. Der Rest der Datei ist
 * unverändert Gemtext und kann ohne Umwandlung über SPARTAN ausgeliefert
 * werden - man überspringt einfach den Block.
 *
 * Unterstützt wird ein bewusst winziger YAML-Ausschnitt:
 *
 *     schlüssel: skalar
 *     schlüssel: [a, b, c]
 *
 * Kein verschachteltes Mapping, keine Anker, keine mehrzeiligen Blöcke, keine
 * Kommentare. Alles andere ist ein Fehler mit Zeilennummer. Ein fest
 * umrissener Ausschnitt lässt sich vollständig prüfen; "irgendwie YAML" nicht.
 *
 * Ein Schlüssel beginnt in Spalte eins. Führender Leerraum bedeutet in YAML
 * Einrückung und damit Verschachtelung; eine so eingerückte Zeile wird deshalb
 * abgewiesen, mit einer Meldung, die das ausdrücklich sagt. Getrimmt wird der
 * Leerraum UM den Doppelpunkt und am Ende des Wertes.
 */
#ifndef PDA_STORE_FRONTMATTER_H
#define PDA_STORE_FRONTMATTER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct frontmatter frontmatter;

/* Zerlegt den Anfang von text. Beginnt der Text nicht mit einer "---"-Zeile,
 * gibt es kein Front Matter: das ist kein Fehler, das Ergebnis ist dann leer
 * und *body_offset ist 0.
 *
 * *body_offset erhält den Versatz, an dem der Gemtext-Körper beginnt.
 *
 * Bei einem Fehler NULL und eine Meldung "datei:zeile: text" in err. name wird
 * nur für die Meldung gebraucht und darf NULL sein. */
frontmatter *frontmatter_parse(const char *text, size_t len, const char *name,
                               size_t *body_offset, char *err, size_t err_size);

void frontmatter_free(frontmatter *fm);

int  frontmatter_count(const frontmatter *fm);
bool frontmatter_has(const frontmatter *fm, const char *key);

/* Der Schlüssel an Stelle i, in der Reihenfolge des Einlesens; NULL außerhalb.
 *
 * Zusammen mit frontmatter_count() ist das der Weg, alle Felder abzugehen,
 * ohne ihre Namen zu kennen - eine Volltextsuche braucht das, und der
 * generische Browser später auch. Ein count ohne diesen Zugriff wäre eine
 * Zahl über Dinge, an die niemand herankommt. */
const char *frontmatter_key_at(const frontmatter *fm, int i);

/* Der Wert als Zeichenkette, oder NULL. Bei einer Liste der erste Eintrag. */
const char *frontmatter_get(const frontmatter *fm, const char *key);

/* Listenzugriff. Ein Skalar zählt als Liste mit einem Eintrag - so muss der
 * Aufrufer nicht zwei Fälle unterscheiden, wenn ihm beides recht ist. */
int         frontmatter_list_count(const frontmatter *fm, const char *key);
const char *frontmatter_list_at(const frontmatter *fm, const char *key, int i);

/* Schreibt den Block wieder als Text, einschließlich der beiden "---"-Zeilen.
 * Die Reihenfolge der Schlüssel bleibt die des Einlesens - nur so ist ein
 * Rundlauf byteweise stabil, und nur dann sieht ein Nutzer in seiner Datei
 * nicht nach jedem Speichern eine andere Sortierung. false, wenn out zu klein
 * ist; needed erhält dann die nötige Größe. */
bool frontmatter_write(const frontmatter *fm, char *out, size_t out_size,
                       size_t *needed);

#endif /* PDA_STORE_FRONTMATTER_H */
