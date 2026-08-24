/* Ein Panel: eine Sammlung von Bedienelementen mit Layout und Fokus.
 *
 * Das Layout ist bewusst ein Stapel und kein Constraint-Solver. Formulare sind
 * ohnehin Beschriftung-links-Feld-rechts, und genau das kann LAYOUT_FORM.
 * Alles Übrige entsteht durch Verschachteln: ein Panel wird mit
 * panel_as_widget() selbst zu einem Widget und wandert in ein anderes.
 *
 * Der Fokus ist hier zu Hause und nicht in den Widgets: nur eine Stelle weiß,
 * welches Element gerade dran ist, und nur eine Stelle kennt die Reihenfolge.
 * Tab und Umschalt+Tab wandern durch alle Elemente mit wants_focus, mit
 * Umbruch an den Enden.
 */
#ifndef PDA_UI_PANEL_H
#define PDA_UI_PANEL_H

#include <stdbool.h>

#include "ui/widget.h"

typedef enum {
    LAYOUT_VSTACK,   /* untereinander, volle Breite */
    LAYOUT_HSTACK,   /* nebeneinander, volle Höhe */
    LAYOUT_FORM      /* paarweise: Beschriftung links, Element rechts */
} layout_kind;

typedef struct panel panel;

panel *panel_create(const theme *th, const catalog *cat);
void   panel_destroy(panel *p);

/* --- Verschachteln ----------------------------------------------------------
 *
 * Ein Panel wird selbst zu einem Widget und wandert damit in ein anderes.
 * Gebraucht, sobald in einem Formular etwas nebeneinander stehen soll: zwei
 * Knöpfe in einer Zeile, ein Textfeld mit einem Rollbalken daneben.
 *
 * Das Widget übernimmt das Panel und gibt es beim Zerstören frei - genau wie
 * panel_add ein Widget übernimmt. Wer es anlegt, gibt es damit ab.
 *
 * Die Tabulatortaste läuft durch die Verschachtelung hindurch: erst durch die
 * Elemente des inneren Panels, und wenn der Fokus dort hinten herausläuft,
 * weiter im äußeren. Von außen sieht es aus wie eine einzige Reihenfolge, und
 * genau so soll es sich anfühlen. */
widget *panel_as_widget(panel *p);

/* Das Panel hinter einem Widget, oder NULL, wenn es keins ist. */
panel *panel_of_widget(const widget *w);

void panel_set_layout(panel *p, layout_kind kind, int gap, int pad);

/* Das Panel übernimmt das Widget und gibt es später frei. false, wenn kein
 * Platz mehr ist; das Widget wird dann sofort freigegeben, damit der Aufrufer
 * es nicht versehentlich behält. */
bool    panel_add(panel *p, widget *w);
int     panel_count(const panel *p);
widget *panel_at(const panel *p, int index);

/* Verteilt area auf die Elemente und setzt deren frame. Muss vor dem ersten
 * Zeichnen und nach jeder Größenänderung laufen. */
void panel_layout(panel *p, rect area);

/* Wunschgröße aller Elemente zusammen, im gewählten Layout. */
void panel_measure(panel *p, int *pw, int *ph);

void panel_draw(const panel *p, gc *g);

/* Verarbeitet ein Ereignis: Tab und Umschalt+Tab wandern durch den Fokus, ein
 * Klick setzt ihn, alles andere geht an das fokussierte Element.
 *
 * Löst ein Knopf aus, steht sein Aktionsname danach in *out_action, sonst NULL.
 * out_action darf NULL sein. */
bool panel_event(panel *p, const event *e, const char **out_action);

widget *panel_focus(const panel *p);
void    panel_set_focus(panel *p, widget *w);
void    panel_focus_next(panel *p);
void    panel_focus_prev(panel *p);

/* Wie panel_focus_next/prev, meldet aber, ob der Fokus dabei über das Ende
 * hinausgelaufen ist. dir ist positiv für vorwärts.
 *
 * Das braucht die Verschachtelung: läuft der Fokus im inneren Panel hinten
 * heraus, gehört der nächste Schritt dem äußeren. */
bool panel_focus_step(panel *p, int dir);

/* Hebt den Fokus im ganzen Panel auf, auch in verschachtelten. */
void panel_clear_focus(panel *p);

#endif /* PDA_UI_PANEL_H */
