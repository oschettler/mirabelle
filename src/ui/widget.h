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
    const theme        *th;    /* zeigt auf das Thema des Panels, das es besitzt */
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

#endif /* PDA_UI_WIDGET_H */
