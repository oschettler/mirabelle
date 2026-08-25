/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Gemeinsamer, nicht öffentlicher Zustand von window.c und wm.c.
 *
 * Kein Aufrufer außerhalb dieser beiden Dateien darf diese Datei einbinden -
 * die öffentlichen Verträge stehen in window.h und wm.h, hier steht nur, wie
 * beide Seiten intern zusammenarbeiten.
 */
#ifndef PDA_UI_WM_INTERNAL_H
#define PDA_UI_WM_INTERNAL_H

#include <stdbool.h>

#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "ui/theme.h"
#include "ui/window.h"

#define WINDOW_TITLE_MAX 63

struct window {
    const theme *th;        /* Maße, nicht Eigentum dieses Fensters */
    rect         frame;
    char         title[WINDOW_TITLE_MAX + 1];
    unsigned     flags;
    bitmap       content;
    bool         active;
    void        *user;

    window_close_fn on_close;
    void           *on_close_user;
};

typedef enum {
    DRAG_NONE = 0,
    DRAG_MOVE,
    DRAG_RESIZE
} drag_kind;

struct wm {
    theme        th;        /* Kopie: die Verwaltung besitzt ihr Thema,
                             * damit kein Aufrufer es überleben muss */
    int          screen_w, screen_h;

    window     **z;         /* z-sortierte Liste, [0] hinten, Eigentümer der Fenster */
    int          count;
    int          cap;

    drag_kind    drag;
    window      *drag_win;
    int          anchor_x, anchor_y;   /* Mausposition bei Ziehbeginn */
    rect         start_frame;          /* Fensterrahmen bei Ziehbeginn */
    rect         outline;              /* fortgeschriebene Lage des Umrisses */
};

/* --- window.c, von wm.c genutzt -------------------------------------------- */

window *window_create(const theme *th, rect frame, const char *title, unsigned flags);
void    window_destroy(window *w);

/* Setzt einen neuen Rahmen. Ändert sich die Größe des Inhaltsbereichs, wird
 * die Inhaltsbitmap neu angelegt; schlägt das fehl, bleiben Rahmen und Inhalt
 * unverändert und die Funktion liefert false. */
bool window_set_frame(window *w, rect frame);

void window_set_active(window *w, bool active);

/* Gezeichnete (nicht um hit_slop gewachsene) Trefferflächen. */
rect window_titlebar_rect(const window *w);
rect window_close_box_rect(const window *w);
rect window_grow_box_rect(const window *w);

void window_draw(const window *w, gc *g);

#endif /* PDA_UI_WM_INTERNAL_H */
