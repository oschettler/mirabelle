/* Die Feldtyp-Registratur: alles, was ein Feldtyp können muss, an einer Stelle.
 *
 * Ein Datum wird anders angezeigt als gespeichert, ein Wahrheitswert braucht
 * ein Kästchen statt eines Textfelds, eine Auswahl eine Liste. Ohne Registratur
 * stünde diese Fallunterscheidung an jeder Stelle, die Felder anfasst - im
 * Zeichnen, im Formular, beim Speichern, beim Sortieren -, und ein neuer
 * Feldtyp hieße, sie alle zu finden.
 *
 * Mit Registratur ist ein neuer Feldtyp ein Eintrag in `schema.h` und eine
 * Struktur hier. Sonst ändert sich nichts.
 *
 * ## Speicherform und Anzeigeform
 *
 * Im Datensatz steht die Speicherform, im Formular die Anzeigeform. Bei Text
 * sind sie gleich, bei einem Datum nicht: gespeichert wird JJJJ-MM-TT, damit
 * sich Daten als Zeichenketten sortieren und vergleichen lassen (query.h);
 * angezeigt wird, was `date.format` im Katalog sagt.
 *
 * Die Trennung ist der Grund, warum die Sortierung ohne Datumstyp auskommt und
 * warum eine andere Sprache ein anderes Datumsformat bekommt, ohne dass sich
 * an den Daten etwas ändert.
 */
#ifndef PDA_APP_FIELDKIND_H
#define PDA_APP_FIELDKIND_H

#include <stdbool.h>
#include <stddef.h>

#include "app/schema.h"
#include "core/i18n.h"
#include "ui/theme.h"
#include "ui/widget.h"

typedef struct {
    const char *name;

    /* Speicherform nach Anzeigeform. Schreibt immer etwas, notfalls nichts. */
    void (*format)(const schema_field *f, const catalog *cat,
                   const char *stored, char *out, size_t out_size);

    /* Anzeigeform nach Speicherform. false, wenn die Eingabe nicht passt -
     * dann soll das Formular sie stehen lassen und den Nutzer fragen, statt
     * stillschweigend etwas anderes zu speichern. */
    bool (*parse)(const schema_field *f, const catalog *cat,
                  const char *input, char *out, size_t out_size);

    /* Das Bedienelement für ein Formular. Der Aufrufer übernimmt es. */
    widget *(*make_widget)(const schema_field *f, const theme *th,
                           const catalog *cat);

    /* Wert aus dem Bedienelement holen beziehungsweise hineinschreiben,
     * beide Male in der Speicherform. */
    bool (*read)(const schema_field *f, const catalog *cat, widget *w,
                 char *out, size_t out_size);
    void (*write)(const schema_field *f, const catalog *cat, widget *w,
                  const char *stored);
} field_kind_ops;

/* Nie NULL: jeder Wert aus field_kind hat einen Eintrag, und ein Test wacht
 * darüber. */
const field_kind_ops *fieldkind(field_kind kind);

/* Bequemer Zugriff über das Feld. */
const field_kind_ops *fieldkind_of(const schema_field *f);

#endif /* PDA_APP_FIELDKIND_H */
