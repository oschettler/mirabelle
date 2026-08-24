/* Rollbalken, siehe widget.h für den Vertrag.
 *
 * Der Balken rechnet nichts selbst - alle Zahlen kommen aus scroll.h. Hier
 * steht nur, wo die Teile liegen, wie sie aussehen und welcher Klick welche
 * Modellfunktion aufruft. Das ist die ganze Aufgabe eines Rollbalkens: er ist
 * eine Sicht auf drei Zahlen, keine zweite Wahrheit daneben.
 *
 * Deshalb gibt es hier auch kein eigenes Feld für die Position. Gäbe es eins,
 * müsste es mit dem Modell abgeglichen werden, und der erste Abgleich, den
 * jemand vergisst, lässt Balken und Inhalt auseinanderlaufen.
 */
#include "ui/widget.h"

#include <stdbool.h>
#include <stdlib.h>

#include "core/i18n.h"
#include "gfx/draw.h"
#include "gfx/pattern.h"
#include "plat/plat.h"
#include "ui/scroll.h"
#include "ui/theme.h"

typedef struct {
    widget        base;
    scrollbar_dir dir;
    scrollmodel  *m;         /* gehört dem Aufrufer, siehe widget.h */
    bool          dragging;
    int           grab;      /* Abstand Zeiger zur Schiebervorderkante */
} scrollbar_widget;

/* --- Geometrie ---------------------------------------------------------------
 *
 * Ein Rollbalken zerfällt in drei Teile: zwei quadratische Pfeilfelder an den
 * Enden und die Rinne dazwischen. Alles hier rechnet entlang einer Achse, und
 * die beiden Richtungen unterscheiden sich nur darin, welche Koordinate die
 * Achse ist. Deshalb eine Struktur mit Achsenwerten statt zweier Rechenwege.
 */

typedef struct {
    rect arrow_lo;   /* oben beziehungsweise links */
    rect arrow_hi;
    int  track_pos;  /* Anfang der Rinne auf der Achse */
    int  track_len;  /* Länge der Rinne, nie negativ */
} sb_geom;

static bool sb_vertical(const scrollbar_widget *sb)
{
    return sb->dir == SCROLLBAR_VERTICAL;
}

/* Die Koordinate eines Punktes entlang der Achse des Balkens. */
static int sb_axis(const scrollbar_widget *sb, int x, int y)
{
    return sb_vertical(sb) ? y : x;
}

static sb_geom sb_layout(const scrollbar_widget *sb)
{
    rect r = sb->base.frame;
    int  s = sb->base.th->scrollbar_w;

    /* Rinne und Pfeilfelder überlappen sich um genau ein Pixel: die Rinne
     * beginnt auf der Randlinie des Pfeilfelds und endet auf der des anderen.
     * Damit teilen sich beide diese Linie. Stießen sie stattdessen aneinander,
     * stünden dort zwei Rahmen nebeneinander und die Trennung wäre doppelt so
     * dick wie jede andere Linie im Balken. */
    sb_geom g;
    if (sb_vertical(sb)) {
        g.arrow_lo  = rect_make(r.x, r.y, r.w, s);
        g.arrow_hi  = rect_make(r.x, r.y + r.h - s, r.w, s);
        g.track_pos = r.y + s - 1;
        g.track_len = r.h - 2 * s + 2;
    } else {
        g.arrow_lo  = rect_make(r.x, r.y, s, r.h);
        g.arrow_hi  = rect_make(r.x + r.w - s, r.y, s, r.h);
        g.track_pos = r.x + s - 1;
        g.track_len = r.w - 2 * s + 2;
    }

    /* Ein zu kurzer Balken hat gar keine Rinne, und track_len kommt dann
     * negativ heraus. Das bleibt so stehen: scroll.h behandelt jede Rinne von
     * null oder weniger als "keine Rinne", und diese Regel soll an genau einer
     * Stelle stehen. Hier noch einmal auf null zu klemmen sähe sorgfältig aus,
     * wäre aber ein zweiter Wächter für dieselbe Sache - und der, der nie
     * feuert, lässt sich auch nicht prüfen. */
    return g;
}

/* Die kleinste Schieberlänge ist die Breite des Balkens: ein Quadrat ist das
 * Kleinste, was sich noch fassen lässt. Damit steht auch diese Zahl im Thema
 * und nicht im Code. */
static int sb_min_thumb(const scrollbar_widget *sb)
{
    return sb->base.th->scrollbar_w;
}

static rect sb_thumb_rect(const scrollbar_widget *sb, const sb_geom *g)
{
    rect r = sb->base.frame;
    int  pos, len;
    scroll_thumb(sb->m, g->track_len, sb_min_thumb(sb), &pos, &len);

    if (sb_vertical(sb))
        return rect_make(r.x, g->track_pos + pos, r.w, len);
    return rect_make(g->track_pos + pos, r.y, len, r.h);
}

/* --- Zeichnen ------------------------------------------------------------------ */

/* Ein gefülltes Dreieck, mittig in box, das nach dx/dy zeigt. Gezeichnet statt
 * als Bitmap abgelegt, damit es jeder Balkenbreite folgt: ein größeres Thema
 * für die Bedienung mit dem Finger (M15) bekommt so größere Pfeile, ohne dass
 * jemand eine zweite Grafik pflegen muss. */
static void draw_arrow(gc *g, rect box, int dx, int dy)
{
    int size = box.w < box.h ? box.w : box.h;
    int n    = size / 4;          /* Höhe des Dreiecks, zugleich halbe Basis */
    if (n < 1) n = 1;

    int cx = box.x + box.w / 2;
    int cy = box.y + box.h / 2;
    int x0 = cx - n / 2;
    int y0 = cy - n / 2;

    /* Zeile i vom Scheitel aus ist 2i+1 Pixel breit. */
    for (int i = 0; i < n; i++) {
        int len = 2 * i + 1;
        if (dy < 0)      gfx_hline(g, cx - i, y0 + i, len);
        else if (dy > 0) gfx_hline(g, cx - i, y0 + n - 1 - i, len);
        else if (dx < 0) gfx_vline(g, x0 + i, cy - i, len);
        else             gfx_vline(g, x0 + n - 1 - i, cy - i, len);
    }
}

static void draw_arrow_box(gc *g, rect box, int dx, int dy)
{
    g->pat = PAT_WHITE;
    gfx_fill_rect(g, box);
    g->pat = PAT_BLACK;
    gfx_frame_rect(g, box);
    draw_arrow(g, box, dx, dy);
}

static void scrollbar_draw(const widget *w, gc *g)
{
    const scrollbar_widget *sb = (const scrollbar_widget *)w;
    rect                    r  = w->frame;

    g->mode = GFX_COPY;

    /* Passt alles hinein, ist der Balken leer: nur der Umriss, keine Pfeile,
     * kein Schieber. So sah es in System 1 aus, und es sagt auf einen Blick,
     * dass es hier nichts zu bedienen gibt. */
    if (!scroll_needed(sb->m)) {
        g->pat = PAT_WHITE;
        gfx_fill_rect(g, r);
        g->pat = PAT_BLACK;
        gfx_frame_rect(g, r);
        return;
    }

    sb_geom geom = sb_layout(sb);

    /* Die Rinne ist das graue Schachbrett - dasselbe Muster wie der
     * Schreibtisch, und aus demselben Grund: bei einem Bit je Pixel ist ein
     * Raster das, was anderswo ein Grauton wäre. */
    g->pat = PAT_GRAY50;
    gfx_fill_rect(g, r);
    g->pat = PAT_BLACK;
    gfx_frame_rect(g, r);

    if (sb_vertical(sb)) {
        draw_arrow_box(g, geom.arrow_lo, 0, -1);
        draw_arrow_box(g, geom.arrow_hi, 0, +1);
    } else {
        draw_arrow_box(g, geom.arrow_lo, -1, 0);
        draw_arrow_box(g, geom.arrow_hi, +1, 0);
    }

    if (geom.track_len > 0) {
        rect thumb = sb_thumb_rect(sb, &geom);
        g->pat = PAT_WHITE;
        gfx_fill_rect(g, thumb);
        g->pat = PAT_BLACK;
        gfx_frame_rect(g, thumb);
    }

    if (!w->enabled) {
        g->pat  = PAT_GRAY50;
        g->mode = GFX_CLEAR;
        gfx_fill_rect(g, r);
        g->mode = GFX_COPY;
    }
}

/* --- Wunschgröße ---------------------------------------------------------------
 *
 * Quer zur Achse genau die Balkenbreite, längs mindestens die beiden
 * Pfeilfelder und ein Stück Rinne dazwischen. */
static void scrollbar_measure(widget *w, int *pw, int *ph)
{
    const scrollbar_widget *sb = (const scrollbar_widget *)w;
    int                     s  = w->th->scrollbar_w;

    if (sb_vertical(sb)) {
        if (pw) *pw = s;
        if (ph) *ph = 3 * s;
    } else {
        if (pw) *pw = 3 * s;
        if (ph) *ph = s;
    }
}

/* --- Ereignisse ----------------------------------------------------------------
 *
 * Vier Trefferflächen, von außen nach innen abgefragt: die beiden Pfeilfelder,
 * der Schieber, und was davon übrigbleibt, ist Rinne. Die Reihenfolge ist
 * wichtig - der Schieber kann bis an ein Pfeilfeld heranreichen.
 */

static bool scrollbar_event(widget *w, const event *e)
{
    scrollbar_widget *sb = (scrollbar_widget *)w;

    switch (e->kind) {
    case EV_MOUSE_DOWN: {
        if (!rect_contains(w->frame, e->x, e->y)) return false;
        if (!scroll_needed(sb->m)) return true;   /* da ist nichts zu holen */

        sb_geom geom = sb_layout(sb);

        if (rect_contains(geom.arrow_lo, e->x, e->y)) {
            scroll_by(sb->m, -1);
            return true;
        }
        if (rect_contains(geom.arrow_hi, e->x, e->y)) {
            scroll_by(sb->m, +1);
            return true;
        }

        rect thumb = sb_thumb_rect(sb, &geom);
        if (rect_contains(thumb, e->x, e->y)) {
            /* Der Schieber springt nicht unter den Zeiger, sondern behält
             * seinen Abstand zu ihm. Sonst zuckte er beim Anfassen. */
            sb->dragging = true;
            sb->grab     = sb_axis(sb, e->x, e->y) -
                           sb_axis(sb, thumb.x, thumb.y);
            return true;
        }

        /* Bleibt die Rinne: eine Seite weiter, in die Richtung, in die
         * geklickt wurde. */
        scroll_pages(sb->m, sb_axis(sb, e->x, e->y) <
                            sb_axis(sb, thumb.x, thumb.y) ? -1 : +1);
        return true;
    }

    case EV_MOUSE_MOVE: {
        if (!sb->dragging) return false;

        /* Beim Ziehen zählt der Zeiger auch außerhalb des Balkens - wer die
         * Maus seitlich wegführt, will den Schieber nicht verlieren. */
        sb_geom geom = sb_layout(sb);
        int     pos  = sb_axis(sb, e->x, e->y) - sb->grab - geom.track_pos;
        scroll_to(sb->m, scroll_value_at(sb->m, geom.track_len,
                                         sb_min_thumb(sb), pos));
        return true;
    }

    case EV_MOUSE_UP:
        if (!sb->dragging) return false;
        sb->dragging = false;
        return true;

    case EV_WHEEL:
        if (!rect_contains(w->frame, e->x, e->y)) return false;
        scroll_by(sb->m, -e->wheel);
        return true;

    default:
        return false;
    }
}

static const widget_class scrollbar_class = {
    .name    = "scrollbar",
    .measure = scrollbar_measure,
    .draw    = scrollbar_draw,
    .event   = scrollbar_event,
};

/* --- Verwaltung -------------------------------------------------------------- */

widget *scrollbar_create(const theme *th, const catalog *cat,
                         scrollbar_dir dir, scrollmodel *m)
{
    if (!m) return NULL;

    scrollbar_widget *sb = calloc(1, sizeof *sb);
    if (!sb) return NULL;

    sb->base.cls     = &scrollbar_class;
    sb->base.th      = th;
    sb->base.cat     = cat;
    sb->base.enabled = true;
    /* Nimmt den Fokus nicht an: in System 1 war ein Rollbalken keine Station
     * für die Tabulatortaste. Wer mit der Tastatur blättert, spricht ohnehin
     * den Inhalt an, nicht den Balken daneben. */
    sb->base.wants_focus = false;
    sb->dir              = dir;
    sb->m                = m;

    return &sb->base;
}

rect scrollbar_thumb(const widget *w)
{
    const scrollbar_widget *sb   = (const scrollbar_widget *)w;
    sb_geom                 geom = sb_layout(sb);
    return sb_thumb_rect(sb, &geom);
}

scrollmodel *scrollbar_model(widget *w)
{
    return ((scrollbar_widget *)w)->m;
}

bool scrollbar_is_dragging(const widget *w)
{
    return ((const scrollbar_widget *)w)->dragging;
}
