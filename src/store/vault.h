/* Der Vault: ein Verzeichnis voller Datensätze, sortiert nach Sammlung.
 *
 *     ~/PDA/
 *       Aufgaben/20260822T151400-a3f9.gmi
 *       Termine/...
 *
 * Eine Sammlung ist nichts weiter als ein Unterverzeichnisname - keine
 * eigene Datenstruktur, kein Schema. Das hält den Vault klein und lässt die
 * Anwendung frei entscheiden, welche Sammlungen es gibt.
 *
 * Jede Datei trägt als Namen die Kennung des Datensatzes (siehe
 * RECORD_ID_LEN in record.h) mit der Endung ".gmi". Weil die Kennung
 * zeitsortiert ist, ist ihre alphabetische Sortierung zugleich die
 * zeitliche - genau dafür wurde ihr Aufbau so gewählt, und vault_list nutzt
 * das aus, statt selbst irgendein Datum zu vergleichen.
 *
 * Alle Dateizugriffe laufen über plat.h, nie direkt über die Standard-
 * bibliothek - sonst kennt die Speicherschicht das Betriebssystem, und die
 * Portierung auf das Gerät bricht.
 *
 * Geschrieben wird sicher: erst in eine Datei mit angehängtem ".tmp", dann
 * mit plat_rename darübergelegt. Bricht das Programm mittendrin ab, steht
 * die alte Fassung noch da statt einer halben neuen.
 */
#ifndef PDA_STORE_VAULT_H
#define PDA_STORE_VAULT_H

#include <stdbool.h>
#include <stddef.h>

#include "store/record.h"

typedef struct vault vault;

/* Öffnet den Vault unter path. Das Verzeichnis wird angelegt, falls es noch
 * nicht besteht - der Aufrufer will, dass es da ist, nicht dass er es selbst
 * anlegt. Bei einem Fehler NULL und eine Meldung in err. */
vault *vault_open(const char *path, char *err, size_t err_size);
void   vault_close(vault *v);

/* Schreibt rec in die Sammlung collection.
 *
 * Trägt rec noch keine Kennung im Feld "id" seines Front Matter, wird eine
 * vergeben und in die geschriebene Datei eingetragen - rec selbst bleibt
 * unverändert, denn frontmatter.h kennt keinen Setter, über den sich das
 * Feld eines bestehenden Datensatzes ändern ließe. Die tatsächlich
 * verwendete Kennung (die vorhandene oder die neu vergebene) wird nach
 * id_out geschrieben (mindestens RECORD_ID_LEN + 1 Bytes).
 *
 * Das Sammlungsverzeichnis wird bei Bedarf angelegt. false bei einem Fehler,
 * mit einer Meldung in err. */
bool vault_save(vault *v, const char *collection, record *rec,
                 char *id_out, size_t id_out_size,
                 char *err, size_t err_size);

/* Liest den Datensatz mit der Kennung id aus der Sammlung collection. NULL
 * bei einem Fehler (Datei fehlt, ungültige Kennung, Parserfehler), mit einer
 * Meldung in err. Der Aufrufer übernimmt den Datensatz und muss ihn mit
 * record_free wieder freigeben. */
record *vault_load(vault *v, const char *collection, const char *id,
                    char *err, size_t err_size);

/* Löscht den Datensatz mit der Kennung id. true auch dann, wenn er schon
 * nicht mehr da war - wie plat_remove, aus demselben Grund: der Aufrufer
 * will, dass er weg ist, nicht dass er ihn persönlich gelöscht hat. */
bool vault_delete(vault *v, const char *collection, const char *id,
                   char *err, size_t err_size);

/* Liefert die Kennungen der Sammlung, aufsteigend sortiert, in ids_out
 * (Platz für cap Einträge à RECORD_ID_LEN + 1 Bytes). *count_out erhält die
 * tatsächliche Anzahl - wie bei plat_list werden überzählige Einträge
 * stillschweigend abgeschnitten statt einen Fehler zu melden.
 *
 * Dateien, deren Name keine gültige Kennung mit der Endung ".gmi" ist,
 * werden übersprungen statt einen Fehler auszulösen: in einem Verzeichnis,
 * das dem Nutzer gehört, liegt irgendwann etwas Fremdes.
 *
 * Eine Sammlung, die noch kein Verzeichnis hat, gilt als leer, nicht als
 * Fehler - es gibt dort schlicht noch keine Datensätze. */
bool vault_list(vault *v, const char *collection,
                 char (*ids_out)[RECORD_ID_LEN + 1], int cap, int *count_out,
                 char *err, size_t err_size);

#endif /* PDA_STORE_VAULT_H */
