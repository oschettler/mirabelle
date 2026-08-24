/* Der generische Browser: eine Anwendung, die aus einem Schema entsteht.
 *
 * Aufgaben, Kontakte und Notizen sind derselbe Code (D-7). Was sie
 * unterscheidet, steht in data/schema (Endung .schema) - drei Dateien, kein
 * Programm. Diese Datei ist das Programm, das es für alle drei gibt.
 *
 * ## Zwei Ansichten
 *
 * Eine Liste und ein Formular. Die Liste zeigt die Spalten, die das Schema
 * nennt, sortiert nach dem Feld, das es nennt. Das Formular zeigt die Felder in
 * der Reihenfolge, die es nennt, jedes mit dem Bedienelement, das sein Feldtyp
 * mitbringt (fieldkind.h).
 *
 * ## Wo die Daten liegen
 *
 * Im Vault, als Datensätze (record.h). Jedes Feld des Schemas ist ein Eintrag
 * im Front Matter - mit einer Ausnahme: ein Feld vom Typ `gemtext` IST der
 * Körper des Datensatzes. Deshalb darf es höchstens eines geben, und
 * schema_load besteht darauf.
 *
 * ## Warum kein Index nötig ist
 *
 * Der Browser sucht über query_matches() direkt in den Datensätzen. Das ist
 * langsamer als der Index, aber es ist der Weg, den es immer gibt (D-3) - und
 * die Sammlungen eines Taschencomputers sind klein genug. Ein Index kann später
 * davorgeschaltet werden, ohne dass sich hier etwas ändert: die Abfrage ist
 * dieselbe Datenstruktur.
 */
#ifndef PDA_APP_BROWSER_H
#define PDA_APP_BROWSER_H

#include <stdbool.h>
#include <stddef.h>

#include "app/schema.h"
#include "core/collate.h"
#include "core/i18n.h"
#include "gfx/draw.h"
#include "plat/plat.h"
#include "store/vault.h"
#include "ui/theme.h"
#include "ui/widget.h"

typedef struct browser browser;

typedef enum {
    BROWSE_LIST,    /* die Übersicht - Liste oder Monatsraster, je nach Schema */
    BROWSE_FORM     /* ein einzelner Datensatz */
} browser_view;

/* Welche Übersicht das Schema will, steht im Schema (schema.h). Der Browser
 * baut sie daraus; von außen ist der Unterschied nur zu sehen, wenn man ihn
 * sucht:
 *
 *   - browser_month() liefert das Kalender-Widget, sonst NULL.
 *   - browser_list()  liefert das Listen-Widget, sonst NULL.
 *
 * Alles andere - laden, filtern, öffnen, speichern, löschen - ist für beide
 * dasselbe. Das ist der Sinn einer Ansichtsregistratur: die Ansicht wechselt,
 * die Anwendung nicht. */

/* Legt einen Browser für dieses Schema an.
 *
 * schema, vault und die Faltungstabellen müssen den Browser überleben; er
 * kopiert sie nicht. Das Thema und der Katalog dagegen werden weitergereicht
 * wie überall in der Oberfläche (widget.h).
 *
 * sort und search dürfen NULL sein - dann wird ungefaltet sortiert und
 * gesucht. */
browser *browser_create(const schema *s, vault *v, const theme *th,
                        const catalog *cat,
                        const collate *sort, const collate *search);
void     browser_destroy(browser *b);

/* Liest die Sammlung neu ein und baut die Liste. Nach jeder Änderung am Vault
 * nötig - der Browser hält keine Verbindung dorthin offen. */
bool browser_reload(browser *b, char *err, size_t err_size);

browser_view browser_view_of(const browser *b);

/* Anzahl und Auswahl in der Liste. */
int  browser_count(const browser *b);
int  browser_selected(const browser *b);

/* Setzt die Auswahl. Ein Index außerhalb lässt sie, wie sie war - wie
 * list_select(), und aus demselben Grund: eine Auswahl, die auf nichts zeigt,
 * wäre schlechter als die alte. */
void browser_select(browser *b, int index);

/* Die Kennung des ausgewählten Datensatzes, oder NULL. */
const char *browser_selected_id(const browser *b);

/* Der Text einer Listenzeile, wie er dasteht - für Tests und für alles, was
 * die Liste woanders anzeigen will. NULL außerhalb. */
const char *browser_row_text(const browser *b, int index);

/* Das Listen-Widget selbst, etwa um einen Rollbalken daranzuhängen
 * (list_scroll) oder zu prüfen, was es anzeigt. NULL, wenn das Schema ein
 * Monatsraster will. Gehört dem Browser. */
widget *browser_list(const browser *b);

/* Das Kalender-Widget, oder NULL. Gehört dem Browser. */
widget *browser_month(const browser *b);

/* --- Was der Nutzer tun kann ---------------------------------------------------
 *
 * Alle diese Funktionen wechseln höchstens die Ansicht; keine zeichnet. Wer
 * sie aufruft, zeichnet danach neu (D-5).
 */

/* Öffnet den ausgewählten Datensatz im Formular. false, wenn nichts ausgewählt
 * ist oder er sich nicht lesen lässt. */
bool browser_open_selected(browser *b, char *err, size_t err_size);

/* Öffnet ein leeres Formular für einen neuen Datensatz. */
bool browser_new(browser *b, char *err, size_t err_size);

/* Schreibt das Formular in den Vault und geht zurück zur Liste.
 *
 * false und eine Meldung, wenn ein Pflichtfeld leer ist oder eine Eingabe nicht
 * zu ihrem Feldtyp passt - dann bleibt das Formular offen und der Nutzer sieht,
 * was er getippt hat. Stillschweigend etwas anderes zu speichern wäre der
 * schlimmere Ausgang. */
bool browser_save(browser *b, char *err, size_t err_size);

/* Zurück zur Liste, ohne zu speichern. */
void browser_cancel(browser *b);

/* Löscht den ausgewählten Datensatz und liest neu ein. */
bool browser_delete_selected(browser *b, char *err, size_t err_size);

/* Volltextfilter für die Liste. NULL oder leer hebt ihn auf. Wirkt beim
 * nächsten browser_reload(). */
bool browser_set_filter(browser *b, const char *text);

/* Das Bedienelement zu einem Feld im offenen Formular, oder NULL.
 *
 * Damit lässt sich ein Feld vorbelegen, den Fokus dorthin setzen oder von
 * außen prüfen, was drinsteht. Welches Bedienelement es ist, sagt der Feldtyp
 * (fieldkind.h) - ein Aufrufer, der etwas hineinschreiben will, geht über
 * dessen write() und nicht am Feldtyp vorbei. */
widget *browser_form_widget(const browser *b, const char *field);

/* --- Oberfläche ------------------------------------------------------------------ */

void browser_layout(browser *b, rect area);
void browser_draw(const browser *b, gc *g);

/* true, wenn das Ereignis verbraucht wurde. */
bool browser_event(browser *b, const event *e);

/* true, WENN in der Liste ein Eintrag geöffnet wurde - Doppelklick oder
 * Return - und setzt den Merker zurück. Wie list_was_opened, und aus demselben
 * Grund: das Öffnen ist eine Entscheidung der Anwendung, nicht des Widgets. */
bool browser_was_opened(browser *b);

#endif /* PDA_APP_BROWSER_H */
