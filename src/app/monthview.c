/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe monthview.h für den Vertrag.
 *
 * Das Raster ist sieben Spalten breit und höchstens sechs Wochen hoch. Sechs,
 * weil ein Monat mit 31 Tagen, der an der ungünstigsten Stelle beginnt, über
 * sechs Wochenzeilen reicht - ein Raster mit fünf Zeilen verliert in solchen
 * Monaten die letzten Tage, und zwar nur in manchen.
 */
#include "app/monthview.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "core/date.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/pattern.h"
#include "gfx/text.h"
#include "plat/plat.h"

extern const font system12;

#define COLS 7
#define ROWS 6

typedef struct {
    widget base;
    date   shown;             /* immer der erste des angezeigten Monats */
    int    selected_day;      /* 1 bis 31 */
    bool   marked[32];        /* nach Tagesnummer; Index 0 bleibt leer */
    bool   opened;
} monthview;

/* --- Aus dem Katalog -------------------------------------------------------------- */

/* Der Wochentag, mit dem eine Zeile beginnt, in der Zählung von
 * date_weekday(): 0 ist Montag. */
static int week_start(const catalog *cat)
{
    const char *s = T(cat, "week.start");
    int         v = (s && *s >= '1' && *s <= '7') ? *s - '0' : 1;
    return v - 1;
}

/* Die Überschrift der Spalte col. Schreibt in out, weil weekday.short eine
 * Liste in einer Zeichenkette ist und kein Feld von Zeigern. */
static void weekday_label(const catalog *cat, int weekday, char *out, size_t out_size)
{
    const char *s = T(cat, "weekday.short");
    out[0] = '\0';

    for (int i = 0; *s; i++) {
        while (*s == ' ') s++;
        const char *start = s;
        while (*s && *s != ' ') s++;

        if (i == weekday) {
            size_t n = (size_t)(s - start);
            if (n >= out_size) n = out_size - 1;
            memcpy(out, start, n);
            out[n] = '\0';
            return;
        }
    }
}

/* --- Geometrie ---------------------------------------------------------------------
 *
 * Die Kopfzeile mit den Wochentagen und darunter sechs Zeilen. Die Spalten
 * teilen die Breite; der Rest, der beim Teilen übrigbleibt, geht an die letzte
 * Spalte, damit das Raster rechts bündig abschließt.
 */

static int header_h(const widget *w) { return w->th->menu_item_h; }

/* Null statt einer negativen Zahl, wenn der Rahmen nicht einmal die Kopfzeile
 * fasst. Beobachtbar ist der Unterschied nicht - jede Aufrufstelle prüft auf
 * „höchstens null" -, aber eine negative Zellenhöhe wäre eine Zahl, die nichts
 * bedeutet, und die nächste Aufrufstelle prüft vielleicht nur auf null. */
static int cell_h(const widget *w)
{
    int h = w->frame.h - header_h(w);
    return h > 0 ? h / ROWS : 0;
}

static int col_x(const widget *w, int col)
{
    return w->frame.x + w->frame.w * col / COLS;
}

static rect cell_rect(const widget *w, int col, int row)
{
    int x0 = col_x(w, col);
    int x1 = col_x(w, col + 1);
    int y  = w->frame.y + header_h(w) + row * cell_h(w);

    return rect_make(x0, y, x1 - x0, cell_h(w));
}

/* In welcher Zelle steht der erste des Monats? */
static int first_offset(const monthview *mv)
{
    int wd = date_weekday(mv->shown);
    int ws = week_start(mv->base.cat);
    int off = wd - ws;
    return off < 0 ? off + 7 : off;
}

/* Der Tag in Zelle (col,row), oder 0, wenn dort keiner steht. */
static int day_at_cell(const monthview *mv, int col, int row)
{
    int idx = row * COLS + col - first_offset(mv);
    int len = date_days_in_month(mv->shown.year, mv->shown.month);

    if (idx < 0 || idx >= len) return 0;
    return idx + 1;
}

/* --- Zustand -------------------------------------------------------------------- */

static void clamp_selection(monthview *mv)
{
    int len = date_days_in_month(mv->shown.year, mv->shown.month);
    if (mv->selected_day < 1)   mv->selected_day = 1;
    if (mv->selected_day > len) mv->selected_day = len;
}

date monthview_month(const widget *w)
{
    return ((const monthview *)w)->shown;
}

date monthview_selected(const widget *w)
{
    const monthview *mv = (const monthview *)w;
    date d = mv->shown;
    d.day = mv->selected_day;
    return d;
}

bool monthview_select(widget *w, date d)
{
    monthview *mv = (monthview *)w;
    if (!date_valid(d)) return false;

    if (d.year != mv->shown.year || d.month != mv->shown.month) {
        mv->shown = d;
        mv->shown.day = 1;
        monthview_clear_marks(w);
    }
    mv->selected_day = d.day;
    clamp_selection(mv);
    return true;
}

void monthview_show_month(widget *w, int months)
{
    monthview *mv = (monthview *)w;

    date cur = monthview_selected(w);
    date nxt = date_add_months(cur, months);

    mv->shown        = nxt;
    mv->shown.day    = 1;

    /* date_add_months() hat den Tag schon auf den letzten des Zielmonats
     * gekürzt - hier noch einmal zu klemmen wäre dieselbe Rechnung zweimal. */
    mv->selected_day = nxt.day;

    /* Ein anderer Monat, andere Termine. Die alten Markierungen stehen zu
     * lassen hieße, Tage zu belegen, an denen nichts ist. */
    monthview_clear_marks(w);
}

void monthview_clear_marks(widget *w)
{
    memset(((monthview *)w)->marked, 0, sizeof ((monthview *)w)->marked);
}

void monthview_mark(widget *w, date d)
{
    monthview *mv = (monthview *)w;
    if (!date_valid(d)) return;
    if (d.year != mv->shown.year || d.month != mv->shown.month) return;

    mv->marked[d.day] = true;
}

bool monthview_is_marked(const widget *w, int day)
{
    const monthview *mv = (const monthview *)w;
    if (day < 1 || day > 31) return false;
    return mv->marked[day];
}

bool monthview_was_opened(widget *w)
{
    monthview *mv = (monthview *)w;
    bool       o  = mv->opened;
    mv->opened = false;
    return o;
}

/* --- Zeichnen ------------------------------------------------------------------- */

static void draw_centered(gc *g, rect r, const char *text)
{
    int tw = text_width(&system12, text);
    int tx = r.x + (r.w - tw) / 2;
    int ty = r.y + (r.h - system12.size) / 2 + system12.ascent;
    gfx_text(g, &system12, tx, ty, text);
}

static void monthview_draw(const widget *w, gc *g)
{
    const monthview *mv = (const monthview *)w;
    rect             r  = w->frame;

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_rect(g, r);
    g->pat = PAT_BLACK;
    gfx_frame_rect(g, r);

    if (cell_h(w) <= 0) return;

    /* Die Kopfzeile steht auf schwarzem Grund - so sieht man auf einen Blick,
     * wo das Raster anfängt, ohne eine zweite Linienstärke einzuführen.
     *
     * Geschrieben wird sie aber schwarz auf weiß und danach umgedreht. Der
     * Grund liegt in gfx_text: es kopiert die ganze Glyphenzelle, nicht nur
     * ihre gesetzten Punkte. Weiß zu schreiben ergäbe deshalb keine weiße
     * Schrift, sondern einen weißen Kasten mit schwarzer Schrift darin.
     * Umdrehen ist derselbe Handgriff, mit dem die Liste ihre Auswahl
     * hervorhebt. */
    rect head = rect_make(r.x, r.y, r.w, header_h(w));

    int ws = week_start(w->cat);
    for (int col = 0; col < COLS; col++) {
        char label[16];
        weekday_label(w->cat, (ws + col) % 7, label, sizeof label);

        rect cell = rect_make(col_x(w, col), r.y, col_x(w, col + 1) - col_x(w, col),
                              header_h(w));
        draw_centered(g, cell, label);
    }
    gfx_invert_rect(g, head);

    /* Die Trennlinien des Rasters. */
    for (int col = 1; col < COLS; col++)
        gfx_vline(g, col_x(w, col), r.y + header_h(w), r.h - header_h(w));

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            int day = day_at_cell(mv, col, row);
            if (!day) continue;

            rect cell = cell_rect(w, col, row);

            char num[8];
            snprintf(num, sizeof num, "%d", day);
            draw_centered(g, cell, num);

            /* Ein belegter Tag bekommt einen Strich unter die Zahl. Ein Punkt
             * daneben verschöbe die Zahl aus der Mitte, sobald er da ist. */
            if (mv->marked[day]) {
                int len = cell.w / 2;
                gfx_hline(g, cell.x + (cell.w - len) / 2, cell.y + cell.h - 3, len);
            }

            if (day == mv->selected_day) gfx_invert_rect(g, cell);
        }
    }

    if (w->focused) {
        rect outer = rect_make(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
        gfx_frame_rect(g, outer);
    }
}

static void monthview_measure(widget *w, int *pw, int *ph)
{
    /* Sieben Spalten, in denen eine zweistellige Zahl und eine
     * Wochentagsabkürzung Platz haben, und die Zeilen des Themas. */
    int col = text_width(&system12, "88") + 2 * w->th->menu_pad;
    if (pw) *pw = col * COLS;
    if (ph) *ph = w->th->menu_item_h * (ROWS + 1);
}

/* --- Ereignisse ------------------------------------------------------------------ */

static bool hit_cell(const widget *w, int x, int y, int *col, int *row)
{
    if (!rect_contains(w->frame, x, y)) return false;
    if (cell_h(w) <= 0) return false;

    int ry = y - w->frame.y - header_h(w);
    if (ry < 0) return false;

    *row = ry / cell_h(w);
    if (*row >= ROWS) return false;

    for (int c = 0; c < COLS; c++)
        if (x >= col_x(w, c) && x < col_x(w, c + 1)) { *col = c; return true; }
    return false;
}

static bool monthview_event(widget *w, const event *e)
{
    monthview *mv = (monthview *)w;

    switch (e->kind) {
    case EV_MOUSE_DOWN: {
        int col, row;
        if (!hit_cell(w, e->x, e->y, &col, &row)) {
            /* Ein Klick in den Kopf oder daneben gehört trotzdem dem
             * Kalender, wenn er im Rahmen liegt. */
            return rect_contains(w->frame, e->x, e->y);
        }

        int day = day_at_cell(mv, col, row);
        if (day) {
            mv->selected_day = day;
            if (e->clicks >= 2) mv->opened = true;
        }
        return true;
    }

    case EV_WHEEL:
        if (!rect_contains(w->frame, e->x, e->y)) return false;
        monthview_show_month(w, -e->wheel);
        return true;

    case EV_KEY_DOWN: {
        if (!w->focused) return false;

        int len = date_days_in_month(mv->shown.year, mv->shown.month);

        switch (e->key) {
        /* Links und rechts einen Tag, hoch und runter eine Woche - das ist
         * die Bewegung, die das Raster nahelegt. An den Rändern wird
         * geklemmt statt in den Nachbarmonat zu springen: das Blättern ist
         * eine eigene Geste. */
        case KEY_LEFT:  mv->selected_day--;     clamp_selection(mv); return true;
        case KEY_RIGHT: mv->selected_day++;     clamp_selection(mv); return true;
        case KEY_UP:    mv->selected_day -= 7;  clamp_selection(mv); return true;
        case KEY_DOWN:  mv->selected_day += 7;  clamp_selection(mv); return true;

        case KEY_HOME:  mv->selected_day = 1;   return true;
        case KEY_END:   mv->selected_day = len; return true;

        case KEY_PAGE_UP:   monthview_show_month(w, -1); return true;
        case KEY_PAGE_DOWN: monthview_show_month(w, +1); return true;

        case KEY_RETURN: mv->opened = true; return true;

        default: return false;
        }
    }

    default:
        return false;
    }
}

static const widget_class monthview_class = {
    .name    = "monthview",
    .measure = monthview_measure,
    .draw    = monthview_draw,
    .event   = monthview_event,
};

widget *monthview_create(const theme *th, const catalog *cat, date shown)
{
    if (!date_valid(shown)) return NULL;

    monthview *mv = calloc(1, sizeof *mv);
    if (!mv) return NULL;

    mv->base.cls         = &monthview_class;
    mv->base.th          = th;
    mv->base.cat         = cat;
    mv->base.enabled     = true;
    mv->base.wants_focus = true;

    mv->selected_day = shown.day;
    mv->shown        = shown;
    mv->shown.day    = 1;

    return &mv->base;
}
