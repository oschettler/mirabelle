/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bedienelemente.
 *
 * Ein Widget ist wenig: eine Klasse mit vier Funktionen und ein Rechteck.
 * Das ist die ganze Abstraktion, und sie reicht für Beschriftungen, Knöpfe,
 * Kontrollkästchen, Textfelder und Listen.
 *
 * Das Rechteck setzt nicht das Widget selbst, sondern das Layout. Damit gibt
 * es die Lage genau einmal - ein Widget, das seine eigene Position ausrechnet
 * und ein Layout, das sie auch ausrechnet, driften früher oder später
 * auseinander.
 *
 * Alle Maße kommen aus dem Thema. Kein Widget kennt eine Zahl.
 */
#ifndef PDA_UI_WIDGET_H
#define PDA_UI_WIDGET_H

#include <stdbool.h>

#include "core/i18n.h"
#include "gfx/draw.h"
#include "plat/plat.h"
#include "ui/scroll.h"
#include "ui/textbuf.h"
#include "ui/theme.h"

typedef struct widget widget;

typedef struct {
    const char *name;   /* für Fehlersuche und Tests, nicht sichtbar */

    /* Wunschgröße. Das Layout darf sie überschreiten, aber nicht
     * unterschreiten. */
    void (*measure)(widget *w, int *pw, int *ph);

    void (*draw)(const widget *w, gc *g);

    /* true, wenn verarbeitet. Das Widget darf sich auf w->frame verlassen. */
    bool (*event)(widget *w, const event *e);

    /* Gibt NUR zurück, was die Klasse zusätzlich belegt hat - etwa einen
     * Textpuffer. Das Widget selbst gibt widget_destroy() frei.
     *
     * Wer hier free(w) schreibt, gibt zweimal frei. Klassen, die außer der
     * eigenen Struktur nichts belegen, lassen dieses Feld einfach NULL. */
    void (*destroy)(widget *w);
} widget_class;

struct widget {
    const widget_class *cls;
    /* Zeigt auf die Themakopie des Panels, sobald das Widget aufgenommen
     * wurde - panel_add hängt es dorthin um. Bis dahin auf das, was beim
     * Anlegen übergeben wurde.
     *
     * Ein Thema wird nie als Zeiger festgehalten, sondern immer kopiert, und
     * alles hängt an der Kopie. Diese Regel gibt es, weil das Gegenteil hier
     * mehrfach zu Abstürzen und falschen Maßen geführt hat.
     *
     * Achtung bei Widgets, die in KEIN Panel wandern - etwa einem Rollbalken
     * neben einem Formular. Die hängt niemand um; sie behalten den Zeiger vom
     * Anlegen. Wer so eines baut, hält selbst eine Kopie des Themas, die das
     * Widget überlebt. Ein Thema auf dem Stapel einer Funktion, die
     * zurückkehrt, taugt dafür nicht. */
    const theme        *th;
    const catalog      *cat;

    rect  frame;               /* setzt das Layout */
    bool  enabled;
    bool  focused;
    bool  wants_focus;         /* nimmt den Fokus über Tab an */
    void *user;
};

/* Gemeinsame Handgriffe, damit nicht jede Klasse dasselbe schreibt. */
void widget_destroy(widget *w);
void widget_measure(widget *w, int *pw, int *ph);
void widget_draw(const widget *w, gc *g);
bool widget_event(widget *w, const event *e);

/* --- Die einfachen Bedienelemente ---------------------------------------- */

/* Beschriftung. Nimmt keinen Fokus an. */
widget *label_create(const theme *th, const catalog *cat, const char *key);

/* Knopf. action ist der Name, den er beim Auslösen meldet; er landet in
 * *out_action von panel_event. */
widget *button_create(const theme *th, const catalog *cat,
                      const char *key, const char *action);
void    button_set_default(widget *w, bool is_default);

/* Liefert true, WENN der Knopf seit dem letzten Aufruf ausgelöst wurde, und
 * setzt den Merker dabei zurück. Genau einmal je Ereignis abfragen. */
bool    button_was_pressed(widget *w);

/* Der beim Anlegen übergebene Aktionsname, oder NULL.
 *
 * Den braucht das Panel, um ihn als *out_action zu melden. Er liegt bewusst
 * NICHT in widget.user: das Feld gehört der Anwendung, damit sie eigene Daten
 * an ein Bedienelement hängen kann. Ein Widget, das es sich nimmt, nimmt es
 * ihr weg. */
const char *button_action(const widget *w);

/* Kontrollkästchen. */
widget *checkbox_create(const theme *th, const catalog *cat,
                        const char *key, bool value);
bool    checkbox_value(const widget *w);
void    checkbox_set_value(widget *w, bool value);

/* Liste.
 *
 * Die Einträge gehören dem Aufrufer und müssen die Liste überleben; sie werden
 * nicht kopiert. Für die Anwendungen dieses Projekts kommen sie ohnehin aus
 * dem Speicher und liegen dort ohnehin.
 *
 * Gescrollt wird zeilenweise, und die Liste sorgt selbst dafür, dass die
 * Auswahl sichtbar bleibt. Bedient wird sie mit Tastatur und Mausrad; wer
 * einen Rollbalken daneben will, hängt ihn an list_scroll(). */
widget *list_create(const theme *th, const catalog *cat);

/* keys sind Katalogschlüssel, count ihre Anzahl. Setzt die Auswahl auf den
 * ersten Eintrag, oder auf -1 bei leerer Liste. */
void list_set_items(widget *w, const char *const *keys, int count);

/* Wie list_set_items, aber die Liste legt eine eigene Kopie an und gibt sie
 * beim Zerstören frei - und sie zeigt die Einträge, wie sie sind, statt sie
 * durch den Katalog zu schicken.
 *
 * Das ist der Unterschied zwischen den beiden Funktionen und der Grund, warum
 * es zwei gibt: ein Menü zeigt übersetzte Namen und bekommt Schlüssel, eine
 * Trefferliste zeigt Daten und bekommt Text. Daten durch T() zu schicken hieße,
 * dass ein Datensatz, der zufällig „button.ok" heißt, plötzlich „OK" anzeigt.
 *
 * false, wenn kein Speicher da ist; die Liste bleibt dann unverändert, statt
 * halb gefüllt dazustehen. */
bool list_set_items_copy(widget *w, const char *const *keys, int count);

/* Der Text, der in Zeile index steht - schon durch den Katalog geschickt oder
 * eben nicht, je nachdem, womit die Liste gefüllt wurde. NULL außerhalb.
 *
 * Der Unterschied zwischen den beiden Füllfunktionen ist von außen sonst nicht
 * zu sehen, und genau er ist es, der zählt. */
const char *list_item_text(const widget *w, int index);

int  list_count(const widget *w);
int  list_selected(const widget *w);        /* -1, wenn nichts ausgewählt ist */
void list_select(widget *w, int index);     /* außerhalb: bleibt, wie es war */

/* Hebt die Auswahl auf. Nach dem Befüllen steht sie auf dem ersten Eintrag -
 * für ein Menü ist das richtig, für ein Auswahlfeld nicht: dort hieße es, dass
 * eine Wahl getroffen wurde, die niemand getroffen hat. */
void list_select_none(widget *w);

/* true, WENN seit dem letzten Aufruf ein Eintrag geöffnet wurde - per
 * Doppelklick oder Return - und setzt den Merker dabei zurück. Genau einmal
 * je Ereignis abfragen, wie bei button_was_pressed. */
bool list_was_opened(widget *w);

/* Erster sichtbarer Eintrag. */
int list_top(const widget *w);

/* Das Bildlaufmodell der Liste, für einen Rollbalken daneben:
 *
 *     widget *bar = scrollbar_create(th, cat, SCROLLBAR_VERTICAL,
 *                                    list_scroll(lst));
 *
 * Es gehört der Liste und lebt genauso lange wie sie. Umfang und Seitengröße
 * zieht die Liste selbst nach - der Balken zeigt, was sie zuletzt wusste. */
scrollmodel *list_scroll(widget *w);

/* --- Rollbalken -----------------------------------------------------------
 *
 * Ein Rollbalken ist eine Sicht auf ein scrollmodel (scroll.h), keine zweite
 * Wahrheit daneben. Er hält deshalb einen Zeiger darauf und keine Kopie: was
 * er anzeigt und was der Inhalt anzeigt, muss dieselbe Zahl sein.
 *
 * Das ist die eine Stelle im Projekt, an der ein Widget bewusst auf fremde
 * Daten zeigt, statt sie zu kopieren - anders ließe sich ein Balken nicht mit
 * dem verbinden, was er rollt. Der Preis: das Modell muss den Balken
 * überleben. Es gehört dem, der scrollt, und liegt typischerweise in derselben
 * Struktur wie der Inhalt.
 *
 * Der Balken nimmt den Fokus nicht an. Wer mit der Tastatur blättert, spricht
 * den Inhalt an, nicht den Balken daneben.
 */
typedef enum {
    SCROLLBAR_VERTICAL,     /* Pfeile oben und unten */
    SCROLLBAR_HORIZONTAL    /* Pfeile links und rechts */
} scrollbar_dir;

/* m darf nicht NULL sein; dann liefert die Funktion NULL. */
widget *scrollbar_create(const theme *th, const catalog *cat,
                         scrollbar_dir dir, scrollmodel *m);

/* Das angezeigte Modell - dasselbe, das beim Anlegen übergeben wurde. */
scrollmodel *scrollbar_model(widget *w);

/* Das Rechteck des Schiebers, in denselben Koordinaten wie frame.
 *
 * Wer den Balken bedienen will, muss wissen, wo der Schieber steht - und diese
 * Lage soll nirgends ein zweites Mal ausgerechnet werden. Deshalb gibt der
 * Balken sie heraus, statt dass Tests und Aufrufer seine Geometrie nachbauen.
 * Bei leerem Modell oder unvermessenem Rahmen kommt ein leeres Rechteck. */
rect scrollbar_thumb(const widget *w);

/* true, solange am Schieber gezogen wird. Für Tests und für Aufrufer, die
 * währenddessen nichts umbauen wollen. */
bool scrollbar_is_dragging(const widget *w);

/* --- Textfelder ----------------------------------------------------------
 *
 * Beide setzen auf textbuf auf und teilen sich fast alles. Der Unterschied ist
 * schmal, aber wesentlich: das einzeilige Feld bricht nicht um und schluckt
 * kbd:[Return] nicht - dort gehört Return dem Formular. Das mehrzeilige bricht
 * an der Feldbreite um und nimmt Return als Zeilenumbruch.
 *
 * Der Umbruch im mehrzeiligen Feld ist reine Anzeige. Er ändert den Text
 * nicht und fügt keine Zeilenumbrüche ein - wer das täte, würde den Text
 * verändern, sobald jemand das Fenster schmaler zieht.
 */
widget *text_field_create(const theme *th, const catalog *cat);
widget *text_area_create(const theme *th, const catalog *cat);

const char *text_widget_value(const widget *w);
bool        text_widget_set_value(widget *w, const char *utf8);

/* Das Modell dahinter, etwa um von außen zu widerrufen oder die Auswahl zu
 * setzen. Gehört dem Widget. */
textbuf *text_widget_buf(widget *w);

/* Erste sichtbare Anzeigezeile im mehrzeiligen Feld. Beim einzeiligen immer 0. */
int text_widget_top_line(const widget *w);

/* Das Bildlaufmodell des mehrzeiligen Felds, für einen Rollbalken daneben -
 * wie list_scroll(). Gezählt wird in Anzeigezeilen, also nach dem Umbruch:
 * ein Feld schmaler zu ziehen macht mehr Zeilen daraus, und der Balken folgt.
 *
 * NULL beim einzeiligen Feld. Das scrollt waagerecht mit der Schreibmarke und
 * hat nichts, was ein Balken anzeigen könnte. */
scrollmodel *text_widget_scroll(widget *w);

#endif /* PDA_UI_WIDGET_H */
