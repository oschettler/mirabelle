/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Das Modell hinter einer Bildlaufleiste.
 *
 * Drei Zahlen, mehr ist Bildlauf nicht: wie viele Einheiten es insgesamt gibt,
 * wie viele gleichzeitig hineinpassen, und welche davon oben steht. Was eine
 * "Einheit" ist, bleibt offen - eine Listenzeile, eine Anzeigezeile im
 * Textfeld, ein Pixel. Das Modell rechnet nur.
 *
 * Getrennt vom Widget aus demselben Grund wie textbuf vom Textfeld (D-13):
 * Liste und Textfeld scrollen, lange bevor ein Rollbalken danebensteht, und
 * eine Bildlaufleiste ohne Bildschirm zu prüfen ist einfacher, als eine mit.
 * Wer scrollt, hält ein scrollmodel; wer es anzeigt, zeigt darauf.
 *
 * Alles hier klemmt statt umzubrechen. Am Ende einer Liste weiterzudrücken
 * bleibt am Ende - es springt nicht an den Anfang zurück.
 */
#ifndef PDA_UI_SCROLL_H
#define PDA_UI_SCROLL_H

#include <stdbool.h>

typedef struct {
    int total;   /* Einheiten insgesamt, nie negativ */
    int page;    /* gleichzeitig sichtbare Einheiten, nie negativ */
    int value;   /* erste sichtbare Einheit, immer zwischen 0 und scroll_max */
} scrollmodel;

/* Setzt Umfang und Seitengröße und klemmt value nach. Das ist der Aufruf nach
 * jeder Änderung am Inhalt oder an der Fenstergröße.
 *
 * value bleibt dabei absichtlich stehen, soweit es geht: wer eine Liste
 * nachlädt oder das Fenster schmaler zieht, will nicht wieder oben landen. */
void scroll_set(scrollmodel *m, int total, int page);

/* Größtes zulässiges value: so weit, dass die letzte Einheit gerade noch
 * sichtbar ist. Nie negativ - passt alles hinein, ist das Ergebnis 0. */
int scroll_max(const scrollmodel *m);

/* true, wenn nicht alles gleichzeitig hineinpasst. Nur dann ist ein
 * Rollbalken überhaupt bedienbar. */
bool scroll_needed(const scrollmodel *m);

/* Setzt value, geklemmt auf [0, scroll_max]. */
void scroll_to(scrollmodel *m, int value);

/* Verschiebt um delta Einheiten; negativ nach oben. Das sind die Pfeilfelder
 * und das Mausrad. */
void scroll_by(scrollmodel *m, int delta);

/* Verschiebt um pages Seiten. Eine Seite ist page Einheiten, mindestens aber
 * eine - sonst käme bei einem noch nicht vermessenen Widget ein Klick in die
 * Rinne nicht von der Stelle. */
void scroll_pages(scrollmodel *m, int pages);

/* Rückt value gerade so weit, dass index sichtbar wird, und lässt es sonst in
 * Ruhe. Liefert true, wenn sich etwas bewegt hat.
 *
 * Das ist der Aufruf, der eine Auswahl im Blick behält, und er gehört an jede
 * Stelle, die die Auswahl ändert - nicht in das Zeichnen. */
bool scroll_reveal(scrollmodel *m, int index);

/* --- Geometrie des Schiebers ------------------------------------------------
 *
 * Auch das ist reine Rechnung und liegt deshalb hier und nicht im Widget: so
 * lässt sich das Ziehen am Schieber prüfen, ohne ein Ereignis zu erfinden.
 *
 * track ist die Länge der Rinne in Pixeln, also die Leiste ohne die beiden
 * Pfeilfelder. min_len hält den Schieber auch bei sehr langen Listen noch
 * greifbar.
 */

/* Lage und Länge des Schiebers in der Rinne. Ist nichts zu scrollen, füllt er
 * die ganze Rinne - gezeichnet wird er dann ohnehin nicht.
 *
 * pos und len dürfen NULL sein. */
void scroll_thumb(const scrollmodel *m, int track, int min_len,
                  int *pos, int *len);

/* Die Umkehrung: welches value gehört zu einem Schieber, dessen Vorderkante
 * bei pos steht? Für das Ziehen. Geklemmt wie scroll_to. */
int scroll_value_at(const scrollmodel *m, int track, int min_len, int pos);

#endif /* PDA_UI_SCROLL_H */
