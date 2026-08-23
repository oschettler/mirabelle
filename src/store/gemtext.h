/* Gemtext: das Format der gespeicherten Datensätze.
 *
 * Drei Kernzeilentypen und drei optionale, keine Auszeichnung im Fließtext,
 * ein einziges Bit Parserzustand. Der Parser läuft von oben nach unten in
 * einem Durchgang und blickt nie zurück.
 *
 * Derselbe Parser bedient die Notizen und später den SPARTAN-Browser. Eine
 * Notiz anzuzeigen und eine abgerufene Seite anzuzeigen ist derselbe Vorgang.
 */
#ifndef PDA_STORE_GEMTEXT_H
#define PDA_STORE_GEMTEXT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    GEM_TEXT,      /* alles ohne besonderes Präfix, auch Leerzeilen */
    GEM_LINK,      /* "=> url [name]" */
    GEM_PRE,       /* Zeile innerhalb eines vorformatierten Blocks */
    GEM_HEADING,   /* "#", "##", "###" */
    GEM_ITEM,      /* "* " */
    GEM_QUOTE      /* ">" */
} gem_kind;

typedef struct {
    gem_kind    kind;

    /* Zeigt in den übergebenen Text, ist NICHT nullterminiert. Immer mit
     * text_len benutzen. Bei GEM_LINK ist es der Anzeigename, bei GEM_HEADING
     * die Überschrift ohne Doppelkreuze. */
    const char *text;
    size_t      text_len;

    /* Nur bei GEM_LINK: die Adresse, ebenfalls nicht nullterminiert. */
    const char *url;
    size_t      url_len;

    /* Nur bei GEM_HEADING: 1, 2 oder 3. */
    int level;
} gem_line;

/* Ruft fn für jede Zeile auf. Liefert die Zahl der gemeldeten Zeilen.
 *
 * Der Text muss nicht nullterminiert sein; len bestimmt das Ende. Zeilenenden
 * dürfen LF oder CRLF sein - die Spezifikation verlangt CRLF, in Dateien auf
 * der Platte steht aber meist LF, und beides zu nehmen kostet nichts.
 *
 * Umschaltzeilen (drei Rückwärts-Anführungszeichen) werden NICHT gemeldet: sie
 * gehören nicht in die Darstellung. Der Text dahinter ist laut Spezifikation
 * ein Hinweis für den Darsteller und wird hier verworfen. */
typedef void (*gem_line_fn)(const gem_line *line, void *user);

int gemtext_parse(const char *text, size_t len, gem_line_fn fn, void *user);

#endif /* PDA_STORE_GEMTEXT_H */
