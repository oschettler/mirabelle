/* Gemtext anzeigen: Überschriften, Absätze, Aufzählungen, Verweise.
 *
 * Dasselbe Widget zeigt eine Notiz aus dem Vault und eine abgerufene Seite -
 * das ist keine Sparsamkeit, sondern folgt daraus, dass beides Gemtext ist
 * (D-12). Der Browser aus M17 ist deshalb wenig mehr als eine Adresszeile über
 * diesem Widget.
 *
 * ## Was es kann und was nicht
 *
 * Es bricht um, es hebt Überschriften hervor, es zählt Verweise durch und
 * lässt einen davon auswählen. Es kann nicht: Bilder, Farben, Schriftgrößen.
 * Gemtext kennt nichts davon, und was das Format nicht kennt, muss die
 * Anzeige nicht können.
 *
 * ## Verweise sind nummeriert
 *
 * Jeder Verweis bekommt eine Nummer und wird als „[3] Name" gezeigt. Das ist
 * die Darstellung, die auch textbasierte Gemini-Browser wählen, und sie hat
 * einen handfesten Grund: bei einem Bit je Pixel gibt es keine Farbe, an der
 * ein Verweis zu erkennen wäre, und Unterstreichen macht Text schlechter
 * lesbar. Die Nummer sagt außerdem, was man tippen kann.
 */
#ifndef PDA_APP_GEMVIEW_H
#define PDA_APP_GEMVIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "core/i18n.h"
#include "ui/scroll.h"
#include "ui/theme.h"
#include "ui/widget.h"

/* Der Text gehört dem Aufrufer und muss das Widget überleben; er wird nicht
 * kopiert - genauso wie bei list_set_items(). Für eine abgerufene Seite liegt
 * er im Puffer des Abrufs, für eine Notiz im Datensatz. */
widget *gemview_create(const theme *th, const catalog *cat);

/* Setzt den anzuzeigenden Text. NULL leert die Ansicht.
 *
 * Der Umbruch entsteht beim nächsten Zeichnen oder Messen, nicht hier: erst
 * dann steht die Breite fest. */
void gemview_set_text(widget *w, const char *gemtext, size_t len);

/* Das Bildlaufmodell, für einen Rollbalken daneben - wie bei der Liste. */
scrollmodel *gemview_scroll(widget *w);

/* --- Verweise ------------------------------------------------------------------ */

int  gemview_link_count(const widget *w);

/* Die Adresse des Verweises mit dieser Nummer (ab 1, wie angezeigt), oder
 * NULL. Zeigt in den übergebenen Text. */
const char *gemview_link_url(const widget *w, int number, size_t *len_out);

/* Der ausgewählte Verweis, oder 0. Ausgewählt wird mit den Zifferntasten oder
 * mit der Maus. */
int  gemview_selected_link(const widget *w);
void gemview_select_link(widget *w, int number);

/* true, WENN ein Verweis geöffnet wurde - Klick oder Return - und setzt den
 * Merker zurück. Wie list_was_opened. */
bool gemview_was_opened(widget *w);

#endif /* PDA_APP_GEMVIEW_H */
