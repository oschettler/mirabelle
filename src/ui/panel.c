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
    /* Das Widget auf die Themakopie des Panels umhängen.
     *
     * Beim Anlegen bekam es den Zeiger des Aufrufers, und der zeigt oft auf
     * eine lokale Variable, die gleich darauf verschwindet. Genau daran ist
     * dieses Projekt schon dreimal hängengeblieben: erst bei wm_create, dann
     * bei menubar_create, dann hier. Wer ein Thema aufnimmt, kopiert es und
     * hängt alles darauf um - das ist die Regel. */
    w->th = &p->th;

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
/* Rückt den Fokus um einen Schritt weiter und meldet, ob er dabei über das
 * Ende hinausgelaufen ist.
 *
 * Diese Meldung braucht das verschachtelte Panel: läuft der Fokus im inneren
 * hinten heraus, gehört der nächste Schritt dem äußeren. Ohne sie bliebe der
 * Fokus im inneren Panel gefangen, und die Tabulatortaste käme nie wieder
 * heraus. */
static bool step_focus(panel *p, int dir)
{
    int  idx     = p->focus;
    bool wrapped = false;

    /* Ohne Fokus fängt es vorn an - beziehungsweise hinten, wenn rückwärts
     * gegangen wird. Das ist der Fall, wenn ein äußeres Panel den Fokus
     * gerade hierher gereicht hat. */
    if (idx < 0) idx = dir > 0 ? -1 : p->count;

    for (int steps = 0; steps < p->count; steps++) {
        idx += dir;

        if (idx < 0)             { idx = p->count - 1; wrapped = true; }
        else if (idx >= p->count) { idx = 0;           wrapped = true; }

        if (focusable(p->items[idx])) {
            panel_set_focus(p, p->items[idx]);
            return wrapped;
        }
    }
    return true;
}

bool panel_focus_step(panel *p, int dir)
{
    return step_focus(p, dir > 0 ? 1 : -1);
}

/* Hebt den Fokus im ganzen Panel auf, auch in verschachtelten. Gebraucht,
 * wenn ein äußeres Panel den Fokus woandershin gibt. */
void panel_clear_focus(panel *p)
{
    for (int i = 0; i < p->count; i++) {
        p->items[i]->focused = false;

        panel *inner = panel_of_widget(p->items[i]);
        if (inner) panel_clear_focus(inner);
    }
    p->focus = -1;
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

/* --- Ein Panel als Widget ----------------------------------------------------
 *
 * Verschachteln geht, indem ein Panel selbst als Widget in ein anderes wandert.
 * Das braucht ein Formular, sobald darin etwas nebeneinander stehen soll:
 * zwei Knöpfe in einer Zeile, ein Textfeld mit einem Rollbalken daneben.
 *
 * Das Widget besitzt das Panel und gibt es beim Zerstören frei. Wer es anlegt,
 * gibt es damit ab - genau wie bei panel_add.
 */

typedef struct {
    widget base;
    panel *inner;

    /* Der Aktionsname, den ein Knopf im inneren Panel gemeldet hat, bis ihn
     * das äußere abholt.
     *
     * Ohne dieses Feld ginge er verloren: panel_event fragt nach jedem
     * Ereignis alle Knöpfe ab und löscht dabei ihren Merker (siehe panel.h).
     * Das innere Panel täte das mit out_action = NULL, und die Meldung wäre
     * weg, bevor irgendwer sie sehen könnte. */
    const char *pending;
} panel_widget;

/* Das Panel hinter einem Widget, oder NULL. Über den Klassenzeiger und nicht
 * über ein Merkmal im Widget: so kann sich niemand versehentlich als Panel
 * ausgeben. */
panel *panel_of_widget(const widget *w);

static void panel_widget_measure(widget *w, int *pw, int *ph)
{
    panel_measure(((panel_widget *)w)->inner, pw, ph);
}

static void panel_widget_draw(const widget *w, gc *g)
{
    /* Das Layout läuft beim Zeichnen, nicht beim Aufnehmen: die Größe setzt
     * das äußere Panel, und sie steht erst fest, wenn es selbst gelegt wurde.
     * Bei einem vollständigen Neuzeichnen je Bild (D-5) kostet das nichts. */
    panel_widget *pw = (panel_widget *)w;

    panel_layout(pw->inner, w->frame);
    panel_draw(pw->inner, g);
}

static bool panel_widget_event(widget *w, const event *e)
{
    panel_widget *pw = (panel_widget *)w;

    panel_layout(pw->inner, w->frame);

    const char *action = NULL;
    bool        used   = panel_event(pw->inner, e, &action);

    if (action) pw->pending = action;
    return used;
}

static const widget_class panel_widget_class;

/* Holt eine gemeldete Aktion aus einem verschachtelten Panel und löscht sie
 * dabei - genau wie button_was_pressed, und aus demselben Grund. */
static const char *take_nested_action(widget *w)
{
    if (!w || w->cls != &panel_widget_class) return NULL;

    panel_widget *pw = (panel_widget *)w;
    const char   *a  = pw->pending;
    pw->pending = NULL;
    return a;
}

static void panel_widget_destroy(widget *w)
{
    panel_destroy(((panel_widget *)w)->inner);
}

static const widget_class panel_widget_class = {
    .name    = "panel",
    .measure = panel_widget_measure,
    .draw    = panel_widget_draw,
    .event   = panel_widget_event,
    .destroy = panel_widget_destroy,
};

panel *panel_of_widget(const widget *w)
{
    if (!w || w->cls != &panel_widget_class) return NULL;
    return ((const panel_widget *)w)->inner;
}

widget *panel_as_widget(panel *p)
{
    if (!p) return NULL;

    panel_widget *pw = calloc(1, sizeof *pw);
    if (!pw) return NULL;

    pw->base.cls     = &panel_widget_class;
    pw->base.th      = &p->th;
    pw->base.cat     = p->cat;
    pw->base.enabled = true;

    /* Fokus nimmt es nur an, wenn drinnen etwas ist, das ihn annehmen kann -
     * sonst bliebe die Tabulatortaste an einer leeren Hülle hängen. */
    for (int i = 0; i < p->count; i++)
        if (p->items[i]->wants_focus) { pw->base.wants_focus = true; break; }

    pw->inner = p;
    return &pw->base;
}

bool panel_event(panel *p, const event *e, const char **out_action)
{
    if (out_action) *out_action = NULL;

    if (e->kind == EV_KEY_DOWN && e->key == KEY_TAB) {
        int dir = (e->mods & MOD_SHIFT) ? -1 : 1;

        /* Hat das fokussierte Element ein eigenes Panel, darf es den Schritt
         * zuerst versuchen. Erst wenn der Fokus dort hinten herausläuft, ist
         * dieses Panel an der Reihe. */
        widget *cur   = panel_focus(p);
        panel  *inner = cur ? panel_of_widget(cur) : NULL;

        if (inner && !panel_focus_step(inner, dir)) return true;
        if (inner) panel_clear_focus(inner);

        step_focus(p, dir);

        /* Landet der Fokus auf einem verschachtelten Panel, muss dort etwas
         * ausgewählt werden - sonst hätte es den Fokus, zeigte ihn aber
         * nirgends. */
        widget *next = panel_focus(p);
        panel  *dst  = next ? panel_of_widget(next) : NULL;
        if (dst) panel_focus_step(dst, dir);

        /* Immer verbraucht. Ein Panel, das Tab durchreicht, gibt die Taste an
         * seinen eigenen Rahmen weiter - und der weiß mit ihr nichts
         * anzufangen. Wo der Fokus als Nächstes hingehört, entscheidet ohnehin
         * das äußere Panel, indem es hier hereinschaut. */
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
     * button_was_pressed verträgt auch Nicht-Knöpfe und liefert dann false. */
    for (int i = 0; i < p->count; i++) {
        if (button_was_pressed(p->items[i]) && out_action)
            *out_action = button_action(p->items[i]);

        /* Und dasselbe für Knöpfe, die in einem verschachtelten Panel
         * stecken: ihre Meldung soll bis nach ganz außen durchkommen, sonst
         * hinge sie von der Verschachtelung ab. */
        const char *nested = take_nested_action(p->items[i]);
        if (nested && out_action) *out_action = nested;
    }

    return consumed;
}
