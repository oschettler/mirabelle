/* Die Vorführung aus M4 und M5.
 *
 * Sie ist vom Hauptprogramm getrennt, damit sie ohne Bildschirm geprüft werden
 * kann: Zustand, Ereignisverarbeitung und Zeichnen sind reine Funktionen, die
 * Schleife in main.c ruft sie nur auf. Ab M6 ersetzt die Fensterverwaltung
 * das hier.
 */
#ifndef PDA_DEMO_H
#define PDA_DEMO_H

#include <stdbool.h>

#include "core/keymap.h"
#include "gfx/draw.h"
#include "plat/plat.h"

typedef struct {
    const keymap *km;
    char          typed[256];
    char          last_action[64];
    int           click_x, click_y, click_count;
    bool          running;
} demo_state;

/* km darf NULL sein; dann bleiben Kürzel wirkungslos. */
void demo_init(demo_state *st, const keymap *km);
void demo_event(demo_state *st, const event *e);
void demo_draw(const demo_state *st, gc *g, int w, int h);

#endif /* PDA_DEMO_H */
