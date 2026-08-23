/* Das Textmodell hinter Textfeldern.
 *
 * Bewusst getrennt vom Widget: Puffer, Schreibmarke, Auswahl und Widerrufen
 * haben mit Zeichnen nichts zu tun. Dadurch lässt sich der schwierigste Teil
 * eines Texteditors vollständig ohne Bildschirm prüfen - und der schwierigste
 * Teil ist nicht das Zeichnen, sondern dass die Schreibmarke nach jeder
 * Änderung noch an der richtigen Stelle steht.
 *
 * Alle Positionen sind BYTE-Versätze in einen UTF-8-Puffer, niemals
 * Zeichenzahlen. Jede Bewegung geht über utf8_next und utf8_prev; deshalb kann
 * die Schreibmarke nie in der Mitte eines Mehrbytezeichens landen. Wer das
 * anders macht, zerlegt beim ersten ü den Text.
 *
 * Der Puffer wächst und schrumpft mit memmove. Das ist für die Textmengen
 * eines Notizbuchs schnell genug, und ein Lückenpuffer wäre hier Klugscheißerei.
 */
#ifndef PDA_UI_TEXTBUF_H
#define PDA_UI_TEXTBUF_H

#include <stdbool.h>
#include <stddef.h>

typedef struct textbuf textbuf;

textbuf *textbuf_create(void);
void     textbuf_destroy(textbuf *tb);

/* --- Inhalt -------------------------------------------------------------- */

/* Nullterminiert, gehört dem textbuf, gilt bis zur nächsten Änderung. */
const char *textbuf_text(const textbuf *tb);
size_t      textbuf_len(const textbuf *tb);      /* in Bytes */

/* Ersetzt den ganzen Inhalt, setzt Schreibmarke ans Ende und leert die
 * Widerruf-Geschichte. Für das Befüllen aus einem Datensatz. */
bool textbuf_set(textbuf *tb, const char *utf8);

/* --- Schreibmarke und Auswahl -------------------------------------------- */

/* Die Schreibmarke. Die Auswahl reicht vom Anker bis zur Schreibmarke; sind
 * beide gleich, ist nichts ausgewählt. */
size_t textbuf_cursor(const textbuf *tb);
size_t textbuf_anchor(const textbuf *tb);
bool   textbuf_has_selection(const textbuf *tb);

/* Ausgewählter Bereich, immer aufsteigend sortiert. */
void textbuf_selection(const textbuf *tb, size_t *from, size_t *to);

/* Setzt die Schreibmarke. Ein Versatz mitten in einem Zeichen wird auf dessen
 * Anfang zurückgezogen. extend == true zieht die Auswahl mit, sonst wird sie
 * aufgehoben. */
void textbuf_set_cursor(textbuf *tb, size_t pos, bool extend);
void textbuf_select_all(textbuf *tb);

/* --- Bewegen ------------------------------------------------------------- */

typedef enum {
    MOVE_LEFT,        /* ein Zeichen */
    MOVE_RIGHT,
    MOVE_WORD_LEFT,   /* an den Anfang des vorigen Wortes */
    MOVE_WORD_RIGHT,
    MOVE_LINE_START,  /* Zeile im Sinne von Zeilenumbruch, nicht Umbruch im Fenster */
    MOVE_LINE_END,
    MOVE_UP,          /* eine Zeile, Spalte möglichst beibehalten */
    MOVE_DOWN,
    MOVE_TEXT_START,
    MOVE_TEXT_END
} textbuf_move;

void textbuf_move_cursor(textbuf *tb, textbuf_move how, bool extend);

/* --- Ändern -------------------------------------------------------------- */

/* Fügt an der Schreibmarke ein. Gibt es eine Auswahl, wird sie zuvor ersetzt. */
bool textbuf_insert(textbuf *tb, const char *utf8);

/* Löscht die Auswahl, sonst ein Zeichen vor beziehungsweise hinter der
 * Schreibmarke. Beides zählt in CODEPUNKTEN, nicht in Bytes - sonst bliebe
 * von einem ü ein halbes übrig. */
bool textbuf_delete_back(textbuf *tb);
bool textbuf_delete_forward(textbuf *tb);

/* --- Widerrufen ---------------------------------------------------------- */

/* Aufeinanderfolgende Eingaben werden zu einem Schritt zusammengefasst,
 * solange sie an derselben Stelle weiterschreiben. Sonst müsste man jeden
 * Buchstaben einzeln zurücknehmen, und das will niemand.
 *
 * textbuf_break_undo() beendet die laufende Zusammenfassung von Hand - etwa
 * wenn die Anwendung zwischendurch etwas anderes tut. */
bool textbuf_undo(textbuf *tb);
bool textbuf_redo(textbuf *tb);
bool textbuf_can_undo(const textbuf *tb);
bool textbuf_can_redo(const textbuf *tb);
void textbuf_break_undo(textbuf *tb);

/* --- Zeilen -------------------------------------------------------------- */

/* Zeilen im Sinne von Zeilenumbrüchen im Text. Der Umbruch im Fenster ist
 * Sache des Widgets und ändert diese Zählung nicht. */
int    textbuf_line_count(const textbuf *tb);
size_t textbuf_line_start(const textbuf *tb, int line);
int    textbuf_line_at(const textbuf *tb, size_t pos);

#endif /* PDA_UI_TEXTBUF_H */
