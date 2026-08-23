/* Siehe wm.h für den Vertrag. */
#include "ui/wm.h"
#include "ui/wm_internal.h"

#include <stdlib.h>

#include "gfx/pattern.h"

wm *wm_create(const theme *th, int screen_w, int screen_h)
{
    wm *m = calloc(1, sizeof *m);
    if (!m) return NULL;

    m->th       = *th;   /* Kopie, damit der Aufrufer sein Thema nicht am Leben halten muss */
    m->screen_w = screen_w;
    m->screen_h = screen_h;
    m->drag     = DRAG_NONE;

    return m;
}

void wm_destroy(wm *m)
{
    if (!m) return;

    for (int i = 0; i < m->count; i++) window_destroy(m->z[i]);
    free(m->z);
    free(m);
}

/* Index des Fensters in der z-Liste, oder -1. */
static int index_of(const wm *m, const window *w)
{
    for (int i = 0; i < m->count; i++)
        if (m->z[i] == w) return i;
    return -1;
}

window *wm_open(wm *m, rect frame, const char *title, unsigned flags)
{
    window *w = window_create(&m->th, frame, title, flags);
    if (!w) return NULL;

    if (m->count == m->cap) {
        int      newcap = m->cap ? m->cap * 2 : 8;
        window **p      = realloc(m->z, (size_t)newcap * sizeof *p);
        if (!p) {
            window_destroy(w);
            return NULL;
        }
        m->z   = p;
        m->cap = newcap;
    }

    m->z[m->count++] = w;
    wm_activate(m, w);

    return w;
}

void wm_close(wm *m, window *w)
{
    int idx = index_of(m, w);
    if (idx < 0) return;

    bool was_active = window_is_active(w);

    for (int i = idx; i < m->count - 1; i++) m->z[i] = m->z[i + 1];
    m->count--;

    /* Ein etwaiges Ziehen dieses Fensters bricht sauber ab. */
    if (m->drag_win == w) {
        m->drag     = DRAG_NONE;
        m->drag_win = NULL;
    }

    window_destroy(w);

    if (was_active && m->count > 0)
        wm_activate(m, m->z[m->count - 1]);
}

void wm_draw(wm *m, gc *g)
{
    g->pat  = PAT_GRAY50;
    g->mode = GFX_COPY;
    gfx_clear(g);

    for (int i = 0; i < m->count; i++)
        window_draw(m->z[i], g);

    if (m->drag != DRAG_NONE) {
        g->pat  = PAT_BLACK;
        g->mode = GFX_XOR;
        gfx_frame_rect(g, m->outline);
        g->mode = GFX_COPY;
    }
}

window *wm_active(const wm *m)
{
    for (int i = 0; i < m->count; i++)
        if (window_is_active(m->z[i])) return m->z[i];
    return NULL;
}

void wm_activate(wm *m, window *w)
{
    int idx = index_of(m, w);
    if (idx < 0) return;

    for (int i = idx; i < m->count - 1; i++) m->z[i] = m->z[i + 1];
    m->z[m->count - 1] = w;

    for (int i = 0; i < m->count; i++)
        window_set_active(m->z[i], m->z[i] == w);
}

int wm_count(const wm *m)
{
    return m->count;
}

window *wm_at_z(const wm *m, int index)
{
    if (index < 0 || index >= m->count) return NULL;
    return m->z[index];
}

/* Das oberste Fenster mit WIN_MODAL, oder NULL. */
static window *find_modal(const wm *m)
{
    for (int i = m->count - 1; i >= 0; i--)
        if (window_flags(m->z[i]) & WIN_MODAL) return m->z[i];
    return NULL;
}

/* r um slop nach allen Seiten gewachsen, aber nie über clamp hinaus. */
static rect grow_and_clamp(rect r, int slop, rect clamp)
{
    r.x -= slop;
    r.y -= slop;
    r.w += 2 * slop;
    r.h += 2 * slop;
    return rect_intersect(r, clamp);
}

window *wm_hit(const wm *m, int x, int y, hit_part *part)
{
    if (part) *part = HIT_NONE;

    window *modal = find_modal(m);
    int     slop  = m->th.hit_slop;

    for (int i = m->count - 1; i >= 0; i--) {
        window *w = m->z[i];
        if (modal && w != modal) continue;   /* modal offen: alles andere ist unerreichbar */

        rect frame = window_frame(w);
        if (!rect_contains(frame, x, y)) continue;

        if (window_is_active(w) && (window_flags(w) & WIN_CLOSABLE)) {
            rect box = grow_and_clamp(window_close_box_rect(w), slop, frame);
            if (rect_contains(box, x, y)) {
                if (part) *part = HIT_CLOSE_BOX;
                return w;
            }
        }

        if (window_is_active(w) && (window_flags(w) & WIN_RESIZABLE)) {
            rect box = grow_and_clamp(window_grow_box_rect(w), slop, frame);
            if (rect_contains(box, x, y)) {
                if (part) *part = HIT_GROW_BOX;
                return w;
            }
        }

        if (rect_contains(window_titlebar_rect(w), x, y)) {
            if (part) *part = HIT_TITLEBAR;
            return w;
        }

        if (rect_contains(window_content_rect(w), x, y)) {
            if (part) *part = HIT_CONTENT;
            return w;
        }

        /* Im Rahmen getroffen, aber weder Feld noch Titelleiste noch Inhalt -
         * etwa die Randlinie. Das Fenster gilt trotzdem als getroffen. */
        return w;
    }

    return NULL;
}

bool wm_is_dragging(const wm *m)
{
    return m->drag != DRAG_NONE;
}

static void begin_drag(wm *m, drag_kind kind, window *w, int x, int y)
{
    m->drag        = kind;
    m->drag_win    = w;
    m->anchor_x    = x;
    m->anchor_y    = y;
    m->start_frame = window_frame(w);
    m->outline     = m->start_frame;
}

static int clampi(int v, int lo, int hi)
{
    if (hi < lo) return lo;   /* Fenster größer als der Schirm: am Rand verankern */
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Begrenzt so, dass die Titelleiste auf dem Schirm bleibt: waagerecht ganz,
 * senkrecht mindestens der Streifen der Titelleiste selbst. Der übrige
 * Fensterkörper darf durchaus über den unteren Bildschirmrand hinausragen. */
static void clamp_move(const wm *m, rect *r)
{
    r->x = clampi(r->x, 0, m->screen_w - r->w);
    r->y = clampi(r->y, 0, m->screen_h - m->th.titlebar_h);
}

bool wm_event(wm *m, const event *e)
{
    switch (e->kind) {
    case EV_MOUSE_DOWN: {
        window  *modal = find_modal(m);
        hit_part part;
        window  *w = wm_hit(m, e->x, e->y, &part);

        if (modal) {
            if (w != modal) return true;   /* daneben: verschluckt, sonst nichts */
        } else if (!w) {
            return false;
        }

        bool was_active = window_is_active(w);
        wm_activate(m, w);

        switch (part) {
        case HIT_CLOSE_BOX:
            wm_close(m, w);
            return true;

        case HIT_TITLEBAR:
            if (window_flags(w) & WIN_MOVABLE) begin_drag(m, DRAG_MOVE, w, e->x, e->y);
            return true;

        case HIT_GROW_BOX:
            if (window_flags(w) & WIN_RESIZABLE) begin_drag(m, DRAG_RESIZE, w, e->x, e->y);
            return true;

        case HIT_CONTENT:
            return !was_active;

        default:
            return true;
        }
    }

    case EV_MOUSE_MOVE: {
        if (m->drag == DRAG_NONE) return false;

        int  dx = e->x - m->anchor_x;
        int  dy = e->y - m->anchor_y;
        rect r  = m->start_frame;

        if (m->drag == DRAG_MOVE) {
            r.x += dx;
            r.y += dy;
            clamp_move(m, &r);
        } else {
            r.w += dx;
            r.h += dy;
            if (r.w < m->th.min_w) r.w = m->th.min_w;
            if (r.h < m->th.min_h) r.h = m->th.min_h;
        }

        m->outline = r;
        return true;
    }

    case EV_MOUSE_UP:
        if (m->drag == DRAG_NONE) return false;

        window_set_frame(m->drag_win, m->outline);
        m->drag     = DRAG_NONE;
        m->drag_win = NULL;
        return true;

    default:
        return false;
    }
}
