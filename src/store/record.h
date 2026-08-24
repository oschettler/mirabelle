/* Ein Datensatz: Front Matter plus Gemtext-Körper, in einer Datei.
 *
 * Der Datensatz ist die kleinste Einheit, mit der Anwendungen umgehen. Er
 * kennt seine Datei nicht - das ist Sache des Vaults. Dadurch lässt sich das
 * Zerlegen und Zusammensetzen ohne Dateisystem prüfen.
 */
#ifndef PDA_STORE_RECORD_H
#define PDA_STORE_RECORD_H

#include <stdbool.h>
#include <stddef.h>

#include "store/frontmatter.h"

typedef struct record record;

/* Legt einen leeren Datensatz an. */
record *record_create(void);
void    record_free(record *r);

/* Zerlegt eine ganze Datei. Bei einem Fehler NULL und eine Meldung
 * "name:zeile: text" in err. */
record *record_parse(const char *text, size_t len, const char *name,
                     char *err, size_t err_size);

/* Setzt den Text wieder zusammen: Front Matter, dann der Körper unverändert.
 *
 * Der Rundlauf muss byteweise stabil sein - wer eine Datei liest und ohne
 * Änderung zurückschreibt, darf keinen Unterschied erzeugen. Sonst meldet
 * jedes Sicherungswerkzeug Änderungen, die keine sind.
 *
 * false, wenn out zu klein ist; needed erhält dann die nötige Größe. */
bool record_write(const record *r, char *out, size_t out_size, size_t *needed);

/* Die Felder. Der Datensatz bleibt Eigentümer. */
frontmatter *record_fields(record *r);

/* Der Gemtext-Körper, nullterminiert. Nie NULL, notfalls leer. */
const char *record_body(const record *r);
bool        record_set_body(record *r, const char *gemtext);

/* --- Kennungen ------------------------------------------------------------
 *
 * Zeitsortiert und dateisystemtauglich: JJJJMMTThhmmss-xxxx. Sortierbar,
 * lesbar, kollisionsarm - und ohne eine Bibliothek dafür.
 *
 * Die vier Zeichen am Ende kommen aus einem Zähler und der Uhr, nicht aus
 * einem Zufallsgenerator: zwei Datensätze in derselben Sekunde sollen sich
 * unterscheiden, aber die Kennung soll bei gleichem Ablauf reproduzierbar
 * bleiben, damit Tests sie prüfen können. */
#define RECORD_ID_LEN 20   /* 15 + 1 + 4, ohne Null */

/* Schreibt eine Kennung nach out (mindestens RECORD_ID_LEN + 1 Bytes) aus der
 * übergebenen Zeit in Sekunden seit 1970. seq unterscheidet Datensätze
 * innerhalb derselben Sekunde. */
void record_make_id(char *out, size_t out_size, long unix_time, unsigned seq);

/* Prüft die Form einer Kennung. */
bool record_id_valid(const char *id);

#endif /* PDA_STORE_RECORD_H */
