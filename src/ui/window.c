/* Siehe window.h für den Vertrag. Die Optik folgt dem Rahmen, den demo.c bis
 * M5 von Hand gezeichnet hat - hier nur mit den Maßen aus einem theme statt
 * fest verdrahteten Zahlen.
 */
#include "ui/window.h"
#include "ui/wm_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "gfx/font.h"
#include "gfx/pattern.h"
#include "gfx/text.h"

extern const font system12;

window *window_create(const theme *th, rect frame, const char *title, unsigned flags)
{
    window *w = calloc(1, sizeof *w);
    if (!w) return NULL;

    w->th    = th;
    w->frame = frame;
    w->flags = flags;
    window_set_title(w, title);

    rect cr = window_content_rect(w);
    if (!bitmap_init(&w->content, cr.w, cr.h)) {
        free(w);
        return NULL;
    }

    return w;
}

void window_destroy(window *w)
{
    if (!w) return;
    bitmap_free(&w->content);
    free(w);
}

bool window_set_frame(window *w, rect frame)
{
    rect old = w->frame;
    w->frame = frame;

    rect cr = window_content_rect(w);
    if (cr.w == w->content.w && cr.h == w->content.h) return true;

    bitmap fresh;
    if (!bitmap_init(&fresh, cr.w, cr.h)) {
        w->frame = old;   /* Speicher reicht nicht: die alte Größe bleibt */
        return false;
    }

    bitmap_free(&w->content);
    w->content = fresh;
    return true;
}

void window_set_active(window *w, bool active)
{
    w->active = active;
}

rect window_titlebar_rect(const window *w)
{
    return rect_make(w->frame.x, w->frame.y, w->frame.w, w->th->titlebar_h);
}

rect window_close_box_rect(const window *w)
{
    int cb = w->th->close_box;
    return rect_make(w->frame.x + w->th->box_margin,
                     w->frame.y + (w->th->titlebar_h - cb) / 2, cb, cb);
}

rect window_grow_box_rect(const window *w)
{
    int gb = w->th->grow_box;
    return rect_make(w->frame.x + w->frame.w - w->th->box_margin - gb,
                     w->frame.y + w->frame.h - w->th->box_margin - gb, gb, gb);
}

rect window_frame(const window *w)
{
    return w->frame;
}

const char *window_title(const window *w)
{
    return w->title;
}

unsigned window_flags(const window *w)
{
    return w->flags;
}

bool window_is_active(const window *w)
{
    return w->active;
}

rect window_content_rect(const window *w)
{
    const theme *th = w->th;
    int          b  = th->border;

    return rect_make(w->frame.x + b, w->frame.y + th->titlebar_h,
                     w->frame.w - 2 * b, w->frame.h - th->titlebar_h - b);
}

void window_gc(window *w, gc *g)
{
    gc_init(g, &w->content);
}

void window_set_title(window *w, const char *title)
{
    snprintf(w->title, sizeof w->title, "%s", title ? title : "");
}

void *window_user(const window *w)
{
    return w->user;
}

void window_set_user(window *w, void *user)
{
    w->user = user;
}

/* --- Zeichnen ---------------------------------------------------------------
 *
 * Reihenfolge wie im Milestone-Text: Inhaltsfläche weiß, Inhalt hinein, dann
 * Rahmen, Trennlinie, Streifen (nur aktiv), Titel mit weißem Hintergrund,
 * zuletzt Schließ- und Größenfeld (nur aktiv). Die gesamte Fensterfläche wird
 * zuerst weiß gefüllt, nicht nur der Inhaltsbereich, sonst schiene durch die
 * Titelleiste hindurch, was vorher an dieser Stelle im Bild stand.
 */
void window_draw(const window *w, gc *g)
{
    const theme *th = w->th;
    rect         r  = w->frame;

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_rect(g, r);

    rect cr = window_content_rect(w);
    gfx_blit(g, &w->content, cr.x, cr.y);

    g->pat = PAT_BLACK;
    for (int i = 0; i < th->border; i++)
        gfx_frame_rect(g, rect_make(r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i));

    gfx_hline(g, r.x, r.y + th->titlebar_h, r.w);

    if (w->active) {
        int top    = r.y + th->border + 2;
        int bottom = r.y + th->titlebar_h - 2;
        for (int y = top; y < bottom; y += th->stripe_gap)
            gfx_hline(g, r.x + th->border, y, r.w - 2 * th->border);
    }

    int tw = text_width(&system12, w->title);
    int tx = r.x + (r.w - tw) / 2;
    int ty = r.y + (th->titlebar_h - system12.size) / 2 + system12.ascent;

    g->pat = PAT_WHITE;
    gfx_fill_rect(g, rect_make(tx - th->title_pad, r.y + th->border,
                               tw + 2 * th->title_pad, th->titlebar_h - th->border));
    g->pat = PAT_BLACK;
    gfx_text(g, &system12, tx, ty, w->title);

    if (w->active && (w->flags & WIN_CLOSABLE)) {
        rect box = window_close_box_rect(w);
        g->pat = PAT_WHITE;
        gfx_fill_rect(g, box);
        g->pat = PAT_BLACK;
        gfx_frame_rect(g, box);
    }

    if (w->active && (w->flags & WIN_RESIZABLE)) {
        rect box = window_grow_box_rect(w);
        g->pat = PAT_WHITE;
        gfx_fill_rect(g, box);
        g->pat = PAT_BLACK;
        gfx_frame_rect(g, box);
    }
}
