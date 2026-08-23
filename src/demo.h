/* Die Vorführung.
 *
 * Ab M6 zeichnet sie keine Fensterrahmen mehr selbst, sondern öffnet drei
 * Fenster bei der Fensterverwaltung. Zustand, Ereignisse und Zeichnen bleiben
 * getrennt von der Schleife, damit sich alles ohne Bildschirm prüfen lässt.
 */
#ifndef PDA_DEMO_H
#define PDA_DEMO_H

#include <stdbool.h>

#include "core/i18n.h"
#include "core/keymap.h"
#include "gfx/draw.h"
#include "plat/plat.h"
#include "ui/theme.h"
#include "ui/wm.h"

typedef struct {
    wm            *m;
    const keymap  *km;
    const catalog *cat;
    window       *w_desk;
    window       *w_keys;
    char          typed[256];
    char          last_action[64];
    int           click_x, click_y, click_count;
    bool          running;
} demo_state;

/* km, cat und th dürfen NULL sein; dann gelten Voreinstellungen, Kürzel
 * bleiben wirkungslos und statt der Texte erscheinen ihre Schlüssel.
 * false, wenn die Fenster nicht angelegt werden konnten. */
bool demo_init(demo_state *st, const keymap *km, const catalog *cat,
               const theme *th, int screen_w, int screen_h);
void demo_free(demo_state *st);

void demo_event(demo_state *st, const event *e);
void demo_draw(demo_state *st, gc *g);

#endif /* PDA_DEMO_H */
