/* Siehe panel.h für den Vertrag.
 *
 * Das Feld der Widgetzeiger wächst wie die z-Liste in wm.c: verdoppeln statt
 * jedes Mal genau passend zu allozieren. Der Fokus steckt nicht im Widget,
 * sondern als Index im Panel - so gibt es nur eine Stelle, die "focused"
 * setzt, und höchstens ein Widget kann je diesen Zustand tragen.
 */
#include "ui/panel.h"

#include <stdlib.h>

struct panel {
    theme          th;    /* Kopie, siehe wm_create - das Panel besitzt sie */
    const catalog *cat;   /* derzeit nur mitgeführt, kein Layout braucht sie */

    widget **items;
    int      count;
    int      cap;

    layout_kind kind;
    int         gap;
    int         pad;

    int focus;   /* Index in items, -1 = kein Fokus */
};

panel *panel_create(const theme *th, const catalog *cat)
{
    panel *p = calloc(1, sizeof *p);
    if (!p) return NULL;

    p->th    = *th;
    p->cat   = cat;
    p->kind  = LAYOUT_VSTACK;
    p->focus = -1;

    return p;
}

void panel_destroy(panel *p)
{
    if (!p) return;

    for (int i = 0; i < p->count; i++) widget_destroy(p->items[i]);
    free(p->items);
    free(p);
}

void panel_set_layout(panel *p, layout_kind kind, int gap, int pad)
{
    p->kind = kind;
    p->gap  = gap;
    p->pad  = pad;
}

bool panel_add(panel *p, widget *w)
{
    if (p->count == p->cap) {
        int      newcap = p->cap ? p->cap * 2 : 8;
        widget **items  = realloc(p->items, (size_t)newcap * sizeof *items);
        if (!items) {
            widget_destroy(w);
            return false;
        }
        p->items = items;
        p->cap   = newcap;
    }

    p->items[p->count++] = w;
    return true;
}

int panel_count(const panel *p)
{
    return p->count;
}

widget *panel_at(const panel *p, int index)
{
    if (index < 0 || index >= p->count) return NULL;
    return p->items[index];
}

/* --- Layout ---------------------------------------------------------------
 *
 * pad verkleinert die Ausdehnung quer zur Stapelrichtung (bei VSTACK die
 * Breite, bei HSTACK die Höhe) - genau wie in panel.h beschrieben. Entlang
 * der Stapelrichtung selbst trägt jedes Element seine Wunschgröße, mit gap
 * dazwischen. panel_measure rechnet dieselbe Formel rückwärts, damit ein
 * Panel, das genau seine Wunschgröße bekommt, seine Elemente ohne Rest oder
 * Überhang unterbringt.
 */

static void layout_vstack(panel *p, rect area)
{
    int x = area.x + p->pad;
    int w = area.w - 2 * p->pad;
    int y = area.y;

    for (int i = 0; i < p->count; i++) {
        widget *w_i = p->items[i];
        int     mw, mh;
        widget_measure(w_i, &mw, &mh);

        w_i->frame = rect_make(x, y, w, mh);
        y += mh;
        if (i < p->count - 1) y += p->gap;
    }
}

static void measure_vstack(const panel *p, int *pw, int *ph)
{
    int w = 0, h = 0;

    for (int i = 0; i < p->count; i++) {
        int mw, mh;
        widget_measure(p->items[i], &mw, &mh);
        if (mw > w) w = mw;
        h += mh;
        if (i < p->count - 1) h += p->gap;
    }

    *pw = p->count > 0 ? w + 2 * p->pad : 0;
    *ph = h;
}

static void layout_hstack(panel *p, rect area)
{
    int y = area.y + p->pad;
    int h = area.h - 2 * p->pad;
    int x = area.x;

    for (int i = 0; i < p->count; i++) {
        widget *w_i = p->items[i];
        int     mw, mh;
        widget_measure(w_i, &mw, &mh);

        w_i->frame = rect_make(x, y, mw, h);
        x += mw;
        if (i < p->count - 1) x += p->gap;
    }
}

static void measure_hstack(const panel *p, int *pw, int *ph)
{
    int w = 0, h = 0;

    for (int i = 0; i < p->count; i++) {
        int mw, mh;
        widget_measure(p->items[i], &mw, &mh);
        w += mw;
        if (i < p->count - 1) w += p->gap;
        if (mh > h) h = mh;
    }

    *pw = w;
    *ph = p->count > 0 ? h + 2 * p->pad : 0;
}

/* Breite der linken Spalte: das breiteste Element mit geradem Index, das
 * auch tatsächlich einen Partner hat. Das letzte, unpaarige Element bei
 * ungerader Anzahl steht allein über die volle Breite und zählt hier nicht
 * mit. */
static int form_left_width(const panel *p)
{
    int w = 0;

    for (int i = 0; i + 1 < p->count; i += 2) {
        int mw, mh;
        widget_measure(p->items[i], &mw, &mh);
        if (mw > w) w = mw;
    }

    return w;
}

static void layout_form(panel *p, rect area)
{
    int content_x = area.x + p->pad;
    int content_w = area.w - 2 * p->pad;
    int left_w    = form_left_width(p);
    int y         = area.y;
    int i         = 0;

    while (i < p->count) {
        if (i + 1 < p->count) {
            widget *a = p->items[i];
            widget *b = p->items[i + 1];
            int     amw, amh, bmw, bmh;
            widget_measure(a, &amw, &amh);
            widget_measure(b, &bmw, &bmh);

            int row_h   = amh > bmh ? amh : bmh;
            int right_w = content_w - left_w - p->gap;

            a->frame = rect_make(content_x, y, left_w, row_h);
            b->frame = rect_make(content_x + left_w + p->gap, y, right_w, row_h);

            y += row_h;
            i += 2;
        } else {
            widget *a = p->items[i];
            int     amw, amh;
            widget_measure(a, &amw, &amh);

            a->frame = rect_make(content_x, y, content_w, amh);
            y += amh;
            i += 1;
        }

        if (i < p->count) y += p->gap;
    }
}

static void measure_form(const panel *p, int *pw, int *ph)
{
    int  left_w    = form_left_width(p);
    int  right_w   = 0;
    int  lone_w    = 0;
    bool have_pair = false;
    int  h         = 0;
    int  rows      = 0;
    int  i         = 0;

    while (i < p->count) {
        if (i + 1 < p->count) {
            int amw, amh, bmw, bmh;
            widget_measure(p->items[i],     &amw, &amh);
            widget_measure(p->items[i + 1], &bmw, &bmh);

            if (bmw > right_w) right_w = bmw;
            have_pair = true;
            h        += amh > bmh ? amh : bmh;
            i        += 2;
        } else {
            int amw, amh;
            widget_measure(p->items[i], &amw, &amh);
            if (amw > lone_w) lone_w = amw;
            h += amh;
            i += 1;
        }
        rows++;
    }

    int content_w = have_pair ? left_w + p->gap + right_w : 0;
    if (lone_w > content_w) content_w = lone_w;
    if (rows > 1) h += p->gap * (rows - 1);

    *pw = p->count > 0 ? content_w + 2 * p->pad : 0;
    *ph = h;
}

void panel_layout(panel *p, rect area)
{
    switch (p->kind) {
    case LAYOUT_VSTACK: layout_vstack(p, area); break;
    case LAYOUT_HSTACK: layout_hstack(p, area); break;
    case LAYOUT_FORM:   layout_form(p, area);   break;
    }
}

void panel_measure(panel *p, int *pw, int *ph)
{
    switch (p->kind) {
    case LAYOUT_VSTACK: measure_vstack(p, pw, ph); break;
    case LAYOUT_HSTACK: measure_hstack(p, pw, ph); break;
    case LAYOUT_FORM:   measure_form(p, pw, ph);   break;
    }
}

void panel_draw(const panel *p, gc *g)
{
    for (int i = 0; i < p->count; i++) widget_draw(p->items[i], g);
}

/* --- Fokus ------------------------------------------------------------------ */

widget *panel_focus(const panel *p)
{
    if (p->focus < 0) return NULL;
    return p->items[p->focus];
}

void panel_set_focus(panel *p, widget *w)
{
    if (p->focus >= 0) p->items[p->focus]->focused = false;

    p->focus = -1;
    for (int i = 0; i < p->count; i++) {
        if (p->items[i] == w) {
            p->focus = i;
            break;
        }
    }

    if (p->focus >= 0) p->items[p->focus]->focused = true;
}

static bool focusable(const widget *w)
{
    return w->wants_focus && w->enabled;
}

/* Läuft höchstens count Schritte weit, wie move_highlight in menu.c - so
 * bleibt ein Panel ohne fokussierbares Element ohne Endlosschleife
 * unverändert. */
static void step_focus(panel *p, int dir)
{
    int idx = p->focus;

    for (int steps = 0; steps < p->count; steps++) {
        idx += dir;
        if (idx < 0) idx = p->count - 1;
        else if (idx >= p->count) idx = 0;

        if (focusable(p->items[idx])) {
            panel_set_focus(p, p->items[idx]);
            return;
        }
    }
}

void panel_focus_next(panel *p)
{
    step_focus(p, 1);
}

void panel_focus_prev(panel *p)
{
    step_focus(p, -1);
}

/* --- Ereignisse --------------------------------------------------------------- */

bool panel_event(panel *p, const event *e, const char **out_action)
{
    if (out_action) *out_action = NULL;

    if (e->kind == EV_KEY_DOWN && e->key == KEY_TAB) {
        if (e->mods & MOD_SHIFT) panel_focus_prev(p);
        else                     panel_focus_next(p);
        return true;
    }

    widget *target = NULL;

    if (e->kind == EV_MOUSE_DOWN) {
        for (int i = p->count - 1; i >= 0; i--) {
            widget *w = p->items[i];
            if (rect_contains(w->frame, e->x, e->y)) {
                if (w->wants_focus && w->enabled) panel_set_focus(p, w);
                target = w;
                break;
            }
        }
    } else {
        target = panel_focus(p);
    }

    if (!target) return false;

    bool consumed = widget_event(target, e);

    /* Nach jedem weitergereichten Ereignis alle Knöpfe abfragen - genau
     * einmal je Widget, sonst geht der Merker verloren (siehe panel.h).
     *
     * HINWEIS auf eine fehlende Schnittstelle: widget.h bietet keine
     * Möglichkeit, von einem widget* auf den Aktionsnamen zurückzuschließen,
     * den button_create beim Erzeugen entgegengenommen hat - etwa ein
     * `const char *button_action(const widget *w)`. Ohne eine solche
     * Funktion kann *out_action hier nicht befüllt werden, auch wenn ein
     * Knopf feuert. */
    for (int i = 0; i < p->count; i++) button_was_pressed(p->items[i]);

    return consumed;
}
