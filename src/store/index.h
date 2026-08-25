/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Der Index: eine SQLite-Datenbank, die man jederzeit wegwerfen kann.
 *
 * Alles, was hier drinsteht, steht auch in den Dateien des Vaults (D-3). Der
 * Index macht das Suchen schnell und sonst nichts. `index.db` zu löschen darf
 * kein Byte Nutzdaten kosten - ein Test erzwingt genau das, indem er die Datei
 * wegwirft, neu aufbaut und dieselben Antworten verlangt.
 *
 * Deshalb steht hier auch keine Abfragesprache für Anwendungen. Sie geben eine
 * `query` (query.h) herein, und diese Datei ist die einzige Stelle im ganzen
 * Programm, an der daraus SQL wird. Gibt es keinen Index - auf dem Gerät wird
 * das der Normalfall sein -, beantwortet query_matches() dieselbe Abfrage
 * direkt über die Datensätze.
 *
 * Eine Ausnahme von der Schichtenregel steht hier bewusst: SQLite greift
 * selbst auf Dateien zu und geht nicht über plat.h. Das ist der Preis dafür,
 * eine fertige Datenbank zu benutzen, statt eine zu schreiben, und er ist
 * bezahlbar, weil der Index verzichtbar ist. Alles, was NICHT verzichtbar ist -
 * der Vault - geht weiterhin über plat.h.
 *
 * Gefaltet wird beim Schreiben, nicht beim Suchen: zu jedem Feldwert legt der
 * Index seine Sortier- und seine Suchfassung mit ab (collate.h). Erst dadurch
 * kann SQL nach DIN 5007 sortieren, ohne die Regel zu kennen.
 */
#ifndef PDA_STORE_INDEX_H
#define PDA_STORE_INDEX_H

#include <stdbool.h>
#include <stddef.h>

#include "core/collate.h"
#include "store/query.h"
#include "store/record.h"
#include "store/vault.h"

typedef struct index_db index_db;

/* Öffnet oder legt die Datenbank unter path an und richtet das Schema ein.
 * ":memory:" ist erlaubt und für Tests gedacht.
 *
 * sort und search sind die Faltungstabellen. Sie müssen den Index überleben;
 * er kopiert sie nicht. NULL ist erlaubt - dann wird ungefaltet abgelegt, und
 * Sortierung wie Suche verlieren die Umlautbehandlung.
 *
 * Bei einem Fehler NULL und eine Meldung in err. */
index_db *index_open(const char *path, const collate *sort, const collate *search,
                     char *err, size_t err_size);
void      index_close(index_db *ix);

/* Trägt rec ein oder ersetzt einen vorhandenen Eintrag gleicher Kennung.
 * Der Datensatz selbst wird nicht gespeichert - nur, was zum Finden nötig
 * ist. */
bool index_put(index_db *ix, const char *collection, const char *id,
               record *rec, char *err, size_t err_size);

/* Entfernt den Eintrag. true auch dann, wenn er gar nicht da war - wie
 * vault_delete, aus demselben Grund. */
bool index_remove(index_db *ix, const char *collection, const char *id,
                  char *err, size_t err_size);

/* Wirft alles weg. Danach ist der Index leer, aber benutzbar. */
bool index_clear(index_db *ix, char *err, size_t err_size);

/* Beantwortet die Abfrage. Die Kennungen landen in ids_out (Platz für cap
 * Einträge à RECORD_ID_LEN + 1 Bytes), *count_out erhält die Anzahl.
 *
 * Wie bei vault_list werden überzählige Treffer stillschweigend abgeschnitten;
 * wer alle will, gibt genug Platz oder setzt query_limit(). */
bool index_query(index_db *ix, const query *q,
                 char (*ids_out)[RECORD_ID_LEN + 1], int cap, int *count_out,
                 char *err, size_t err_size);

/* Baut den Index für die genannten Sammlungen aus dem Vault neu auf.
 *
 * Das ist die Funktion, die D-3 einlöst: nach einem Aufruf ist der Index
 * wieder genau das, was aus den Dateien folgt - egal, was vorher darin stand
 * oder ob es ihn überhaupt gab. */
bool index_rebuild(index_db *ix, vault *v,
                   const char *const *collections, int count,
                   char *err, size_t err_size);

#endif /* PDA_STORE_INDEX_H */
