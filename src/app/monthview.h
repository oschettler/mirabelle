/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Der Monatskalender: ein Raster aus sieben Spalten.
 *
 * ## Warum das hier kein Schema ist
 *
 * Aufgaben, Kontakte und Notizen entstehen aus je einer Schemadatei, ohne eine
 * Zeile Programmcode (D-7). Der Kalender nicht. Man könnte auch ein Monatsraster
 * in Daten pressen - Spaltenzahl, Zeilenhöhe, wo die Zahl steht, wie ein
 * belegter Tag aussieht -, aber die Konfiguration dafür wäre komplizierter als
 * dieser Sonderfall.
 *
 * Das ist die Regel, und sie gilt allgemein: **generisch bis zu dem Punkt, an
 * dem die Konfiguration komplizierter würde als der Sonderfall.** Der Kalender
 * bleibt deshalb eine Schemadatei PLUS diese Ansicht - er tritt in dieselbe
 * Ansichtsregistratur ein wie Liste und Formular und ersetzt sie nicht.
 *
 * ## Was aus Daten kommt
 *
 * Trotzdem steht auch hier keine Sprachannahme im Code. Der Wochenbeginn kommt
 * aus `week.start` im Katalog, die Spaltenüberschriften aus `weekday.short`,
 * alle Maße aus dem Thema. Ein Kalender, der am Sonntag beginnt, ist eine
 * Katalogzeile.
 *
 * `week.start` zählt wie ISO 8601: 1 ist Montag, 7 ist Sonntag. Nicht wie
 * date_weekday(), das bei null anfängt - eine Katalogdatei liest ein Mensch,
 * und für den ist der erste Tag der Tag Nummer eins.
 */
#ifndef PDA_APP_MONTHVIEW_H
#define PDA_APP_MONTHVIEW_H

#include <stdbool.h>

#include "core/date.h"
#include "core/i18n.h"
#include "ui/theme.h"
#include "ui/widget.h"

/* Ein gewöhnliches Widget: es passt in ein Panel, misst sich selbst und nimmt
 * den Fokus an. Angezeigt wird der Monat, in dem `shown` liegt. */
widget *monthview_create(const theme *th, const catalog *cat, date shown);

/* Der angezeigte Monat. Der Tag im Ergebnis ist immer der erste. */
date monthview_month(const widget *w);

/* Der ausgewählte Tag. Immer ein Tag des angezeigten Monats. */
date monthview_selected(const widget *w);

/* Setzt die Auswahl und blättert, falls nötig, zum Monat des Datums.
 * Ein ungültiges Datum ändert nichts. */
bool monthview_select(widget *w, date d);

/* Blättert um months Monate. Die Auswahl wandert mit und wird auf den letzten
 * Tag gekürzt, wenn es ihren Tag im Zielmonat nicht gibt - wie
 * date_add_months(). */
void monthview_show_month(widget *w, int months);

/* --- Belegte Tage --------------------------------------------------------------
 *
 * Der Kalender weiß nicht, was ein Termin ist. Er bekommt gesagt, welche Tage
 * eine Markierung tragen, und zeichnet sie. Wer die Termine kennt - der
 * Browser -, setzt die Markierungen nach jedem Neuladen.
 */

void monthview_clear_marks(widget *w);

/* Markiert einen Tag. Tage außerhalb des angezeigten Monats werden
 * stillschweigend übergangen: die Terminliste enthält auch andere Monate, und
 * der Aufrufer soll sie nicht vorher aussieben müssen. */
void monthview_mark(widget *w, date d);

bool monthview_is_marked(const widget *w, int day);

/* true, WENN ein Tag geöffnet wurde - Doppelklick oder Return - und setzt den
 * Merker zurück. Wie list_was_opened. */
bool monthview_was_opened(widget *w);

#endif /* PDA_APP_MONTHVIEW_H */
