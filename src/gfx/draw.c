#include "draw.h"

#include <stdlib.h>

rect rect_make(int x, int y, int w, int h)
{
    rect r;

    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;

    return r;
}

bool rect_empty(rect r)
{
    return r.w <= 0 || r.h <= 0;
}

bool rect_contains(rect r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

rect rect_intersect(rect a, rect b)
{
    int x0 = a.x > b.x ? a.x : b.x;
    int y0 = a.y > b.y ? a.y : b.y;
    int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;

    if (x1 <= x0 || y1 <= y0) return rect_make(0, 0, 0, 0);

    return rect_make(x0, y0, x1 - x0, y1 - y0);
}

void gc_init(gc *g, bitmap *dst)
{
    g->dst    = dst;
    g->clip   = rect_make(0, 0, dst->w, dst->h);
    g->origin = (point){ 0, 0 };
    g->pat    = PAT_BLACK;
    g->mode   = GFX_COPY;
}

void gc_clip(gc *g, rect r)
{
    g->clip = rect_intersect(r, rect_make(0, 0, g->dst->w, g->dst->h));
}

void gc_clip_intersect(gc *g, rect r)
{
    g->clip = rect_intersect(g->clip, r);
}

/* Wendet den Übertragungsmodus an: quelle wirkt auf das vorhandene Pixel. */
static int apply_mode(gfx_mode mode, int dstbit, int srcbit)
{
    switch (mode) {
    case GFX_COPY:    return srcbit;
    case GFX_OR:      return dstbit | srcbit;
    case GFX_XOR:     return dstbit ^ srcbit;
    case GFX_CLEAR:   return dstbit & !srcbit;
    case GFX_NOTCOPY: return !srcbit;
    }
    return srcbit;
}

/* Der gemeinsame Setzpunkt: alle Koordinaten hier sind schon Bildkoordinaten
 * (origin bereits addiert). Jede Primitive läuft am Ende hier durch, deshalb
 * gibt es nur eine einzige Stelle, die den Clip prüft. */
static void set_pixel(gc *g, int ix, int iy, int value)
{
    if (!rect_contains(g->clip, ix, iy)) return;
    bitmap_set(g->dst, ix, iy, value);
}

/* Mischt ein Quellbit (Muster- oder Blit-Bit) mit dem vorhandenen Pixel gemäß
 * gc.mode und schreibt über den gemeinsamen Setzpunkt. */
static void blend_at(gc *g, int ix, int iy, int srcbit)
{
    int dstbit = bitmap_get(g->dst, ix, iy);
    set_pixel(g, ix, iy, apply_mode(g->mode, dstbit, srcbit));
}

/* Für die musterbasierten Primitive: x, y sind relativ zu origin, das
 * Musterbit wird - wie gefordert - erst nach der Verschiebung ins Bild
 * abgefragt, damit das Muster am Zielbild und nicht am Objekt haftet. */
static void draw_pixel(gc *g, int x, int y)
{
    int ix = x + g->origin.x;
    int iy = y + g->origin.y;

    blend_at(g, ix, iy, pattern_bit(&g->pat, ix, iy));
}

void gfx_clear(gc *g)
{
    for (int y = g->clip.y; y < g->clip.y + g->clip.h; y++)
        for (int x = g->clip.x; x < g->clip.x + g->clip.w; x++)
            blend_at(g, x, y, pattern_bit(&g->pat, x, y));
}

void gfx_pset(gc *g, int x, int y)
{
    draw_pixel(g, x, y);
}

void gfx_hline(gc *g, int x, int y, int w)
{
    for (int i = 0; i < w; i++)
        draw_pixel(g, x + i, y);
}

void gfx_vline(gc *g, int x, int y, int h)
{
    for (int i = 0; i < h; i++)
        draw_pixel(g, x, y + i);
}

/* Ganzzahliger Bresenham, funktioniert unverändert in allen acht Oktanten;
 * die Schleife plottet zuerst und bricht erst danach auf den Endpunkt hin ab,
 * deshalb sind beide Endpunkte eingeschlossen. */
void gfx_line(gc *g, int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        draw_pixel(g, x0, y0);
        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_fill_rect(gc *g, rect r)
{
    if (rect_empty(r)) return;

    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            draw_pixel(g, x, y);
}

void gfx_frame_rect(gc *g, rect r)
{
    if (rect_empty(r)) return;

    int left   = r.x;
    int right  = r.x + r.w - 1;
    int top    = r.y;
    int bottom = r.y + r.h - 1;

    /* Waagerechte Teile zuerst: sie zeichnen auch die vier Ecken. */
    for (int x = left; x <= right; x++) {
        draw_pixel(g, x, top);
        if (bottom != top) draw_pixel(g, x, bottom);
    }

    /* Die senkrechten Teile lassen die schon gezeichneten Zeilen aus, sonst
     * würden die Ecken bei GFX_XOR sich selbst wieder auslöschen. */
    for (int y = top + 1; y <= bottom - 1; y++) {
        draw_pixel(g, left, y);
        if (right != left) draw_pixel(g, right, y);
    }
}

/* (fx/rw)^2 + (fy/rh)^2 <= 1, mit fx = 2x - (rw - 1), auf beiden Seiten mit
 * (rw * rh)^2 durchmultipliziert, um ohne Fließkomma auszukommen. Symmetrisch
 * in x und y, weil fx und fy symmetrisch um die Mitte des Rechtecks liegen. */
static bool in_ellipse(int lx, int ly, int rw, int rh)
{
    long long fx  = 2LL * lx - (rw - 1);
    long long fy  = 2LL * ly - (rh - 1);
    long long lhs = fx * fx * (long long)rh * rh + fy * fy * (long long)rw * rw;
    long long rhs = (long long)rw * rw * (long long)rh * rh;

    return lhs <= rhs;
}

void gfx_fill_oval(gc *g, rect r)
{
    if (rect_empty(r)) return;

    for (int ly = 0; ly < r.h; ly++)
        for (int lx = 0; lx < r.w; lx++)
            if (in_ellipse(lx, ly, r.w, r.h))
                draw_pixel(g, r.x + lx, r.y + ly);
}

/* Ein Pixel gehört zum Rahmen, wenn es selbst innerhalb der Ellipse liegt,
 * aber mindestens einer seiner vier Nachbarn nicht (oder außerhalb des
 * Rechtecks) - das ergibt einen durchgängigen, ein Pixel breiten Ring, ohne
 * eine eigene Kurvenverfolgung zu brauchen. */
void gfx_frame_oval(gc *g, rect r)
{
    static const int dx[4] = { -1, 1, 0, 0 };
    static const int dy[4] = { 0, 0, -1, 1 };

    if (rect_empty(r)) return;

    for (int ly = 0; ly < r.h; ly++) {
        for (int lx = 0; lx < r.w; lx++) {
            if (!in_ellipse(lx, ly, r.w, r.h)) continue;

            bool boundary = false;
            for (int k = 0; k < 4; k++) {
                int nx = lx + dx[k];
                int ny = ly + dy[k];
                if (nx < 0 || ny < 0 || nx >= r.w || ny >= r.h ||
                    !in_ellipse(nx, ny, r.w, r.h)) {
                    boundary = true;
                    break;
                }
            }

            if (boundary) draw_pixel(g, r.x + lx, r.y + ly);
        }
    }
}

void gfx_invert_rect(gc *g, rect r)
{
    if (rect_empty(r)) return;

    rect img  = rect_make(r.x + g->origin.x, r.y + g->origin.y, r.w, r.h);
    rect area = rect_intersect(img, g->clip);

    for (int y = area.y; y < area.y + area.h; y++)
        for (int x = area.x; x < area.x + area.w; x++)
            set_pixel(g, x, y, !bitmap_get(g->dst, x, y));
}

void gfx_blit(gc *g, const bitmap *src, int x, int y)
{
    for (int sy = 0; sy < src->h; sy++) {
        for (int sx = 0; sx < src->w; sx++) {
            int ix = x + g->origin.x + sx;
            int iy = y + g->origin.y + sy;

            blend_at(g, ix, iy, bitmap_get(src, sx, sy));
        }
    }
}
