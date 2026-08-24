/* Ein Schema beschreibt eine Sammlung: welche Felder sie hat, wie sie in einer
 * Liste aussieht und in welcher Reihenfolge sie im Formular stehen.
 *
 * Aufgaben, Kontakte und Notizen sind derselbe Code (D-7). Was sie
 * unterscheidet, steht hier - und zwar als Daten, nicht als Programm.
 *
 * ## Warum eine Textdatei und nicht Lua
 *
 * DESIGN.md Abschnitt 10 zeigt Schemata als Lua-Tabellen, und das bleibt das
 * Ziel. Nur kommt Lua erst mit M13, und der Browser soll vorher laufen.
 *
 * Die Auflösung ist keine Notlösung, sondern besser als das Ziel: nicht Lua
 * ist der Vertrag, sondern diese Struktur. Wer ein Schema hat, hat ein
 * `schema` - ob es aus einer Textdatei kam oder aus einer Lua-Tabelle, sieht
 * der Browser nicht. M13 stellt einen zweiten Lader daneben, keinen Ersatz.
 * Nebenbei bleibt das Gerät dadurch frei: dort kann Lua fehlen, und die
 * Anwendungen laufen trotzdem.
 *
 * ## Das Format
 *
 *     # data/schema/task.schema
 *     type     task
 *     folder   Aufgaben
 *     label    app.tasks
 *     sort     due
 *     columns  done title due
 *     form     title due priority done body
 *
 *     field title
 *         kind      text
 *         label     field.title
 *         required  yes
 *
 *     field due
 *         kind   date
 *         label  field.due
 *
 * Eine Zeile ohne Einzug beginnt etwas Neues, eine eingerückte gehört zum
 * zuletzt begonnenen Feld. Mehr Struktur braucht ein Schema nicht, und mehr
 * Format wäre eine zweite Sprache im Projekt.
 *
 * Geprüft wird beim Laden, nicht beim Benutzen: jeder Name in `columns`,
 * `sort` und `form` muss ein Feld sein, das es gibt. Ein Tippfehler in einer
 * Schemadatei soll beim Start auffallen und nicht als leere Spalte.
 */
#ifndef PDA_APP_SCHEMA_H
#define PDA_APP_SCHEMA_H

#include <stdbool.h>
#include <stddef.h>

#define SCHEMA_NAME_MAX    48
#define SCHEMA_FIELDS_MAX  24
#define SCHEMA_VALUES_MAX  12
#define SCHEMA_COLUMNS_MAX  6

/* Die Feldtypen. Ein neuer Typ ist ein Eintrag hier und einer in der
 * Registratur (fieldkind.h) - kein Fall in einer Verzweigung irgendwo im
 * Zeichencode. */
typedef enum {
    FIELD_TEXT,     /* eine Zeile Text */
    /* Mehrere Zeilen. Ein solches Feld ist NICHT ein Eintrag im Front Matter,
     * sondern der Gemtext-Körper des Datensatzes selbst - deshalb kann es je
     * Schema höchstens eines geben, und schema_load besteht darauf. */
    FIELD_GEMTEXT,
    FIELD_DATE,     /* JJJJ-MM-TT; sortiert und vergleicht sich als Text */
    FIELD_BOOL,     /* ja/nein */
    FIELD_CHOICE    /* einer aus values */
} field_kind;

typedef struct {
    char       name[SCHEMA_NAME_MAX];    /* Schlüssel im Front Matter */
    field_kind kind;
    char       label[SCHEMA_NAME_MAX];   /* Schlüssel im Textkatalog */
    bool       required;

    char values[SCHEMA_VALUES_MAX][SCHEMA_NAME_MAX];
    int  value_count;
} schema_field;

/* Die Ansichten, in die ein Schema eintreten kann.
 *
 * Das ist die Registratur, von der DESIGN.md Abschnitt 10 spricht. Sie ist
 * kurz, und das soll sie bleiben: eine Ansicht kommt dazu, wenn sich ihr
 * Sonderfall nicht mehr sinnvoll konfigurieren lässt (monthview.h), nicht
 * jedes Mal, wenn eine Sammlung anders aussehen soll.
 */
typedef enum {
    VIEW_LIST,    /* die Übersicht als Liste - der Normalfall */
    VIEW_MONTH    /* ein Monatsraster; braucht ein Feld vom Typ date */
} schema_view;

typedef struct {
    char type[SCHEMA_NAME_MAX];     /* kurzer Name, etwa "task" */
    char folder[SCHEMA_NAME_MAX];   /* Sammlung im Vault, etwa "Aufgaben" */
    char label[SCHEMA_NAME_MAX];    /* Katalogschlüssel für den Fenstertitel */

    schema_field fields[SCHEMA_FIELDS_MAX];
    int          field_count;

    /* Die Listenansicht: welche Felder als Spalten, wonach sortiert. */
    char columns[SCHEMA_COLUMNS_MAX][SCHEMA_NAME_MAX];
    int  column_count;
    char sort[SCHEMA_NAME_MAX];
    bool sort_desc;

    /* Die Formularansicht: welche Felder in welcher Reihenfolge. */
    char form[SCHEMA_FIELDS_MAX][SCHEMA_NAME_MAX];
    int  form_count;

    /* Die Übersichtsansicht. Ohne Angabe eine Liste.
     *
     *     view month date
     *
     * Bei VIEW_MONTH nennt view_field das Feld, das den Tag trägt; es muss
     * vom Typ date sein, und schema_load besteht darauf. Ein Kalender ohne
     * Datum wäre ein leeres Raster, und der Fehler fiele erst auf, wenn
     * jemand ihn öffnet. */
    schema_view view;
    char        view_field[SCHEMA_NAME_MAX];
} schema;

/* Bei einem Fehler false und eine Meldung "datei:zeile: text" in err. Ein
 * halbfertiges Schema kommt nie zurück. */
bool schema_load(schema *s, const char *path, char *err, size_t err_size);

/* Das Feld mit diesem Namen, oder NULL. */
const schema_field *schema_field_by_name(const schema *s, const char *name);

/* Der Name eines Feldtyps, wie er in der Datei steht - für Meldungen. */
const char *schema_kind_name(field_kind kind);

#endif /* PDA_APP_SCHEMA_H */
