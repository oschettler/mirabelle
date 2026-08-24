/* Siehe widget.h für den Vertrag.
 *
 * Jede konkrete Klasse legt struct widget als ERSTES Feld einer eigenen,
 * größeren Struktur an (Beschriftung, Aktionsname, Zustand). In C hat das
 * erste Feld eines struct dieselbe Adresse wie das struct selbst - deshalb
 * genügt ein Wurf zwischen `widget *` und dem konkreten Typ, und ein
 * einziges free(w) in widget_destroy gibt alles auf einmal wieder her. Keine
 * der drei Klassen hier braucht deshalb eine eigene destroy-Funktion.
 *
 * Der Aktionsname eines Knopfes liegt in dessen eigener Struktur und wird
 * über button_action() herausgegeben. Er gehört ausdrücklich NICHT in
 * widget.user: das Feld gehört der Anwendung.
 */
#include "ui/widget.h"

#include <stdbool.h>
#include <stdlib.h>

#include "core/i18n.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/pattern.h"
#include "gfx/text.h"
#include "plat/plat.h"
#include "ui/theme.h"

extern const font system12;

/* --- Gemeinsame Handgriffe ------------------------------------------------- */

void widget_destroy(widget *w)
{
    if (!w) return;
    if (w->cls && w->cls->destroy) w->cls->destroy(w);
    free(w);
}

void widget_measure(widget *w, int *pw, int *ph)
{
    if (w->cls && w->cls->measure) {
        w->cls->measure(w, pw, ph);
        return;
    }
    if (pw) *pw = 0;
    if (ph) *ph = 0;
}

void widget_draw(const widget *w, gc *g)
{
    if (w->cls && w->cls->draw) w->cls->draw(w, g);
}

bool widget_event(widget *w, const event *e)
{
    if (!w->enabled) return false;
    if (w->cls && w->cls->event) return w->cls->event(w, e);
    return false;
}

/* --- Beschriftung ----------------------------------------------------------- */

typedef struct {
    widget      base;
    const char *key;
} label_widget;

static void label_measure(widget *w, int *pw, int *ph)
{
    const label_widget *lw   = (const label_widget *)w;
    const char         *text = T(w->cat, lw->key);

    if (pw) *pw = text_width(&system12, text);
    if (ph) *ph = system12.size;
}

static void label_draw(const widget *w, gc *g)
{
    const label_widget *lw   = (const label_widget *)w;
    const char         *text = T(w->cat, lw->key);

    g->pat  = PAT_BLACK;
    g->mode = GFX_COPY;
    int ty = w->frame.y + (w->frame.h - system12.size) / 2 + system12.ascent;
    gfx_text(g, &system12, w->frame.x, ty, text);

    if (!w->enabled) {
        g->pat  = PAT_GRAY50;
        g->mode = GFX_CLEAR;
        gfx_fill_rect(g, w->frame);
        g->mode = GFX_COPY;
    }
}

static const widget_class label_class = {
    .name    = "label",
    .measure = label_measure,
    .draw    = label_draw,
};

widget *label_create(const theme *th, const catalog *cat, const char *key)
{
    label_widget *lw = calloc(1, sizeof *lw);
    if (!lw) return NULL;

    lw->base.cls     = &label_class;
    lw->base.th      = th;
    lw->base.cat     = cat;
    lw->base.enabled = true;
    lw->key          = key;

    return &lw->base;
}

/* --- Knopf -------------------------------------------------------------------
 *
 * Die Optik folgt dialog.c: weißes Rechteck, schwarzer Rahmen, beim
 * Voreinstellungsknopf ein zweiter Rahmen zwei Pixel weiter außen, dann der
 * mittige Text, zuletzt die Invertierung bei Fokus.
 */

typedef struct {
    widget      base;
    const char *key;
    const char *action;
    bool        is_default;
    bool        pressed;   /* Merkerbit, button_was_pressed liest und löscht */
} button_widget;

static void button_measure(widget *w, int *pw, int *ph)
{
    const button_widget *bw = (const button_widget *)w;
    int width = text_width(&system12, T(w->cat, bw->key)) + 2 * w->th->button_pad;
    if (width < w->th->button_min_w) width = w->th->button_min_w;

    if (pw) *pw = width;
    if (ph) *ph = w->th->button_h;
}

static void button_draw(const widget *w, gc *g)
{
    const button_widget *bw = (const button_widget *)w;
    rect                 r  = w->frame;

    int radius = w->th->button_radius;

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_round_rect(g, r, radius);
    g->pat = PAT_BLACK;
    gfx_frame_round_rect(g, r, radius);

    /* Der Voreinstellungsknopf bekommt einen dicken Außenrahmen mit einem
     * schmalen Weißraum davor - so hob System 1 die Voreinstellung hervor. */
    if (bw->is_default) {
        int gap  = w->th->default_gap;
        int ring = w->th->default_ring;
        for (int i = 0; i < ring; i++) {
            int off = gap + 1 + i;
            gfx_frame_round_rect(g, rect_make(r.x - off, r.y - off,
                                              r.w + 2 * off, r.h + 2 * off),
                                 radius + off);
        }
    }

    const char *text = T(w->cat, bw->key);
    int         tw   = text_width(&system12, text);
    int         tx   = r.x + (r.w - tw) / 2;
    int         ty   = r.y + (r.h - system12.size) / 2 + system12.ascent;
    gfx_text(g, &system12, tx, ty, text);

    if (w->focused) gfx_invert_rect(g, r);

    if (!w->enabled) {
        g->pat  = PAT_GRAY50;
        g->mode = GFX_CLEAR;
        gfx_fill_rect(g, r);
        g->mode = GFX_COPY;
    }
}

static bool button_event(widget *w, const event *e)
{
    button_widget *bw = (button_widget *)w;

    if (e->kind == EV_MOUSE_DOWN && rect_contains(w->frame, e->x, e->y)) {
        bw->pressed = true;
        return true;
    }

    if (e->kind == EV_KEY_DOWN && w->focused &&
        (e->key == KEY_RETURN || e->key == KEY_SPACE)) {
        bw->pressed = true;
        return true;
    }

    return false;
}

static const widget_class button_class = {
    .name    = "button",
    .measure = button_measure,
    .draw    = button_draw,
    .event   = button_event,
};

widget *button_create(const theme *th, const catalog *cat,
                      const char *key, const char *action)
{
    button_widget *bw = calloc(1, sizeof *bw);
    if (!bw) return NULL;

    bw->base.cls         = &button_class;
    bw->base.th          = th;
    bw->base.cat         = cat;
    bw->base.enabled     = true;
    bw->base.wants_focus = true;
    bw->action           = action;
    bw->key              = key;

    return &bw->base;
}

void button_set_default(widget *w, bool is_default)
{
    ((button_widget *)w)->is_default = is_default;
}

/* Ein Panel reicht seine Elemente durch, ohne ihre Klassen zu kennen. Beide
 * Knopf-Abfragen müssen deshalb aushalten, dass man sie auf eine Beschriftung
 * oder ein Kästchen anwendet - sonst lesen sie hinter deren Struktur hinaus.
 * Genau das ist beim Zusammenbau von M8 passiert und vom Sanitizer gefunden
 * worden, während der gewöhnliche Bau grün blieb. */
static bool is_button(const widget *w)
{
    return w && w->cls == &button_class;
}

bool button_was_pressed(widget *w)
{
    if (!is_button(w)) return false;

    button_widget *bw = (button_widget *)w;
    bool           p  = bw->pressed;
    bw->pressed = false;
    return p;
}

const char *button_action(const widget *w)
{
    if (!is_button(w)) return NULL;
    return ((const button_widget *)w)->action;
}

/* --- Kontrollkästchen ---------------------------------------------------------
 *
 * Kantenlänge wie das Schließfeld (theme.close_box) - passt optisch und
 * spart ein eigenes Themafeld, genau wie im Meilenstein verlangt. Aus
 * demselben Grund dient theme.menu_pad als Abstand zwischen Quadrat und
 * Text: dieselbe Zahl polstert schon den Text in Knöpfen und Menüs.
 */

typedef struct {
    widget      base;
    const char *key;
    bool        value;
} checkbox_widget;

static void checkbox_measure(widget *w, int *pw, int *ph)
{
    const checkbox_widget *cw  = (const checkbox_widget *)w;
    int                     box = w->th->close_box;
    int                     tw  = text_width(&system12, T(w->cat, cw->key));

    if (pw) *pw = box + w->th->check_gap + tw;
    if (ph) *ph = box > system12.size ? box : system12.size;
}

static void checkbox_draw(const widget *w, gc *g)
{
    const checkbox_widget *cw  = (const checkbox_widget *)w;
    int                     box = w->th->close_box;
    rect r = rect_make(w->frame.x, w->frame.y + (w->frame.h - box) / 2, box, box);

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_rect(g, r);
    g->pat = PAT_BLACK;
    gfx_frame_rect(g, r);

    if (w->focused) {
        rect outer = rect_make(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
        gfx_frame_rect(g, outer);
    }

    if (cw->value) {
        gfx_line(g, r.x + 2, r.y + box / 2, r.x + box / 2, r.y + box - 3);
        gfx_line(g, r.x + box / 2, r.y + box - 3, r.x + box - 2, r.y + 2);
    }

    const char *text = T(w->cat, cw->key);
    int         ty   = w->frame.y + (w->frame.h - system12.size) / 2 + system12.ascent;
    gfx_text(g, &system12, r.x + box + w->th->check_gap, ty, text);

    if (!w->enabled) {
        g->pat  = PAT_GRAY50;
        g->mode = GFX_CLEAR;
        gfx_fill_rect(g, w->frame);
        g->mode = GFX_COPY;
    }
}

static bool checkbox_event(widget *w, const event *e)
{
    checkbox_widget *cw = (checkbox_widget *)w;

    if (e->kind == EV_MOUSE_DOWN && rect_contains(w->frame, e->x, e->y)) {
        cw->value = !cw->value;
        return true;
    }

    /* kbd:[Return] löst hier bewusst nicht aus - das ist im Formular für
     * "annehmen" reserviert. */
    if (e->kind == EV_KEY_DOWN && w->focused && e->key == KEY_SPACE) {
        cw->value = !cw->value;
        return true;
    }

    return false;
}

static const widget_class checkbox_class = {
    .name    = "checkbox",
    .measure = checkbox_measure,
    .draw    = checkbox_draw,
    .event   = checkbox_event,
};

widget *checkbox_create(const theme *th, const catalog *cat,
                        const char *key, bool value)
{
    checkbox_widget *cw = calloc(1, sizeof *cw);
    if (!cw) return NULL;

    cw->base.cls         = &checkbox_class;
    cw->base.th          = th;
    cw->base.cat         = cat;
    cw->base.enabled     = true;
    cw->base.wants_focus = true;
    cw->key              = key;
    cw->value            = value;

    return &cw->base;
}

bool checkbox_value(const widget *w)
{
    return ((const checkbox_widget *)w)->value;
}

void checkbox_set_value(widget *w, bool value)
{
    ((checkbox_widget *)w)->value = value;
}
