/* Die Fensterverwaltung.
 *
 * Eine z-sortierte Liste, mehr ist es nicht: von hinten nach vorn zeichnen,
 * von vorn nach hinten treffen. Überlappung entsteht daraus ohne Zusatzaufwand.
 *
 * Der Schirm wird jedes Bild vollständig neu gezeichnet (Entscheidung D-5).
 * Es gibt deshalb kein Damage-Tracking, keine Regionenarithmetik und keine
 * Expose-Ereignisse - die komplizierteste Fehlerklasse in GUI-Systemen
 * entfällt ersatzlos.
 *
 * Verschoben und vergrößert wird mit einem XOR-Umriss, der erst beim Loslassen
 * springt. Das ist historisch korrekt und zugleich der billigste mögliche Weg.
 */
#ifndef PDA_UI_WM_H
#define PDA_UI_WM_H

#include <stdbool.h>

#include "gfx/draw.h"
#include "plat/plat.h"
#include "ui/theme.h"
#include "ui/window.h"

typedef struct wm wm;

wm  *wm_create(const theme *th, int screen_w, int screen_h);
void wm_destroy(wm *m);

/* Öffnet ein Fenster ganz vorn und macht es aktiv. NULL bei Speichermangel. */
window *wm_open(wm *m, rect frame, const char *title, unsigned flags);

/* Schließt und gibt frei. Ist es das aktive Fenster, wird das nächste
 * darunterliegende aktiv. */
void wm_close(wm *m, window *w);

/* Zeichnet Schreibtisch, alle Fenster von hinten nach vorn und zuletzt einen
 * etwaigen Ziehumriss. */
void wm_draw(wm *m, gc *g);

/* Verarbeitet ein Ereignis. true, wenn die Verwaltung es verbraucht hat -
 * dann darf die Anwendung es nicht noch einmal sehen. */
bool wm_event(wm *m, const event *e);

window  *wm_active(const wm *m);
void     wm_activate(wm *m, window *w);
int      wm_count(const wm *m);

/* Index 0 ist das hinterste Fenster. NULL bei ungültigem Index. */
window  *wm_at_z(const wm *m, int index);

/* Vorderstes Fenster an dieser Stelle, oder NULL. part darf NULL sein.
 * Trefferflächen der Felder wachsen um theme.hit_slop. */
window  *wm_hit(const wm *m, int x, int y, hit_part *part);

/* true, solange verschoben oder vergrößert wird. */
bool wm_is_dragging(const wm *m);

#endif /* PDA_UI_WM_H */
