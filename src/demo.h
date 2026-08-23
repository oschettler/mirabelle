/* Die Vorführung aus M4.
 *
 * Sie ist vom Hauptprogramm getrennt, damit sie ohne Bildschirm geprüft werden
 * kann: Zustand, Ereignisverarbeitung und Zeichnen sind reine Funktionen, die
 * Schleife in main.c ruft sie nur auf. Ab M6 ersetzt die Fensterverwaltung
 * das hier.
 */
#ifndef PDA_DEMO_H
#define PDA_DEMO_H

#include <stdbool.h>

#include "gfx/draw.h"
#include "plat/plat.h"

typedef struct {
    char typed[256];
    bool running;
} demo_state;

void demo_init(demo_state *st);
void demo_event(demo_state *st, const event *e);
void demo_draw(const demo_state *st, gc *g, int w, int h);

#endif /* PDA_DEMO_H */
