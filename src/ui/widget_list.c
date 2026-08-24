/* Liste, siehe widget.h für den Vertrag.
 *
 * Baut genauso wie widget.c: struct widget als erstes Feld, eine einzige
 * widget_class-Konstante mit Funktionszeigern, kein eigenes destroy - die
 * Liste besitzt außer ihrer eigenen Struktur nichts, keys gehört dem
 * Aufrufer (siehe widget.h) und wird nur verwiesen, nie kopiert.
 *
 * Das Herzstück ist list_ensure_visible(): jede Stelle, an der sich die
 * Auswahl ändert - Neubefüllung, Mausklick, Pfeiltasten, Bild-ab/-auf,
 * Pos1/Ende - ruft sie auf, statt die Sichtbarkeitsrechnung fünfmal zu
 * wiederholen. Nur das Mausrad rührt die Auswahl nicht an und lässt sie
 * deshalb aus.
 */
#include "ui/widget.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "core/i18n.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/pattern.h"
#include "gfx/text.h"
#include "plat/plat.h"
#include "ui/scroll.h"
#include "ui/theme.h"

extern const font system12;

typedef struct {
    widget             base;
    const char *const *keys;      /* gehört dem Aufrufer, wird nicht kopiert */

    /* ... es sei denn, list_set_items_copy() wurde benutzt. Dann liegen die
     * Einträge hier, und die Liste gibt sie beim Zerstören frei. keys zeigt in
     * diesem Fall auf owned. */
    char             **owned;
    int                owned_count;
    int                count;
    int                selected;  /* -1, wenn nichts ausgewählt ist */
    scrollmodel        sc;        /* value ist der erste sichtbare Eintrag */
    bool               opened;    /* Merkerbit, list_was_opened liest und löscht */
} list_widget;

/* --- Geometrie -------------------------------------------------------------
 *
 * Wie viele Zeilen in frame.h passen. Vor dem ersten Layout ist frame.h
 * null - dann passt nichts hinein, und alles hier muss das aushalten, statt
 * abzustürzen.
 */

static int visible_rows(const widget *w)
{
    if (w->frame.h <= 0) return 0;
    return w->frame.h / w->th->menu_item_h;
}

/* Bringt das Bildlaufmodell auf den Stand von Inhalt und Rahmen.
 *
 * Die Höhe setzt das Layout, nicht die Liste; sie erfährt sie erst, wenn sie
 * gezeichnet wird oder ein Ereignis bekommt. Deshalb wird hier nachgezogen
 * statt an einer einzigen Stelle gesetzt - scroll_set() ist genau dafür
 * gebaut: es klemmt nach und lässt die Position sonst, wo sie ist.
 *
 * Das const wegzucasten ist derselbe Handgriff wie in text_area_ensure_wrap():
 * für den Aufrufer ändert sich nichts Sichtbares, es wird nur nachgerechnet,
 * was ohnehin aus Rahmen und Inhalt folgt. */
static void list_sync(const list_widget *lw_const)
{
    list_widget *lw = (list_widget *)lw_const;
    scroll_set(&lw->sc, lw->count, visible_rows(&lw->base));
}

/* --- Auswahl und Sichtbarkeit ------------------------------------------------
 *
 * Sorgt dafür, dass die Auswahl im Sichtfenster liegt. Die Rechnung dazu steht
 * in scroll.h und nicht hier - dieselbe gilt für das mehrzeilige Textfeld.
 */
static void list_ensure_visible(list_widget *lw)
{
    if (lw->selected < 0) return;
    list_sync(lw);
    scroll_reveal(&lw->sc, lw->selected);
}

/* Springt zu index, an die Enden geklemmt statt umgebrochen - so bewegen sich
 * Pfeiltasten, Bild-ab/-auf, Pos1 und Ende. Bei leerer Liste bleibt die
 * Auswahl bei -1. */
static void list_goto(list_widget *lw, int index)
{
    if (lw->count == 0) {
        lw->selected = -1;
        return;
    }
    if (index < 0) index = 0;
    if (index >= lw->count) index = lw->count - 1;

    lw->selected = index;
    list_ensure_visible(lw);
}

/* Wählt index nur aus, wenn er im gültigen Bereich liegt - sonst bleibt die
 * bisherige Auswahl stehen. Das ist der Vertrag von list_select() und gilt
 * genauso für einen Mausklick auf einen getroffenen Eintrag. */
static void list_select_valid(list_widget *lw, int index)
{
    if (index < 0 || index >= lw->count) return;
    lw->selected = index;
    list_ensure_visible(lw);
}

static int list_page(const widget *w)
{
    int rows = visible_rows(w);
    return rows > 0 ? rows : 1;
}

/* Liefert in *out den getroffenen Eintrag, aber nur, wenn er auch tatsächlich
 * gezeichnet wird - ein Klick in den Leerraum unterhalb der letzten Zeile
 * zählt nicht, auch wenn er noch innerhalb von frame liegt. */
static bool list_hit(const list_widget *lw, int y, int *out)
{
    int rows = visible_rows(&lw->base);
    int row  = (y - lw->base.frame.y) / lw->base.th->menu_item_h;
    if (row < 0 || row >= rows) return false;

    int idx = lw->sc.value + row;
    if (idx >= lw->count) return false;

    *out = idx;
    return true;
}

/* --- Wunschgröße -------------------------------------------------------------- */

static void list_measure(widget *w, int *pw, int *ph)
{
    const list_widget *lw = (const list_widget *)w;

    int max_tw = 0;
    for (int i = 0; i < lw->count; i++) {
        int tw = text_width(&system12, T(w->cat, lw->keys[i]));
        if (tw > max_tw) max_tw = tw;
    }

    int width = max_tw + 2 * w->th->menu_pad;
    if (width < 80) width = 80;

    int rows = lw->count;
    if (rows > 8) rows = 8;
    if (rows < 1) rows = 1;

    if (pw) *pw = width;
    if (ph) *ph = rows * w->th->menu_item_h;
}

/* --- Zeichnen ------------------------------------------------------------------ */

static void list_draw(const widget *w, gc *g)
{
    const list_widget *lw = (const list_widget *)w;
    rect                r = w->frame;

    /* Vor dem Zeichnen nachziehen: erst jetzt steht die Höhe fest, und ein
     * Rollbalken daneben liest gleich darauf dasselbe Modell. */
    list_sync(lw);

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_rect(g, r);
    g->pat = PAT_BLACK;
    gfx_frame_rect(g, r);

    int rows            = visible_rows(w);
    int baseline_offset = (w->th->menu_item_h - system12.size) / 2 + system12.ascent;

    for (int i = 0; i < rows; i++) {
        int idx = lw->sc.value + i;
        if (idx >= lw->count) break;

        rect row = rect_make(r.x, r.y + i * w->th->menu_item_h, r.w, w->th->menu_item_h);
        int  ty  = row.y + baseline_offset;

        g->pat = PAT_BLACK;
        gfx_text(g, &system12, row.x + w->th->menu_pad, ty, T(w->cat, lw->keys[idx]));

        if (idx == lw->selected)
            gfx_invert_rect(g, row);
    }

    if (w->focused) {
        rect outer = rect_make(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
        gfx_frame_rect(g, outer);
    }

    if (!w->enabled) {
        g->pat  = PAT_GRAY50;
        g->mode = GFX_CLEAR;
        gfx_fill_rect(g, r);
        g->mode = GFX_COPY;
    }
}

/* --- Ereignisse ------------------------------------------------------------------ */

static bool list_event(widget *w, const event *e)
{
    list_widget *lw = (list_widget *)w;

    list_sync(lw);

    switch (e->kind) {
    case EV_MOUSE_DOWN: {
        if (!rect_contains(w->frame, e->x, e->y)) return false;

        int idx;
        if (list_hit(lw, e->y, &idx)) {
            list_select_valid(lw, idx);
            if (e->clicks >= 2) lw->opened = true;
        }
        return true;
    }

    case EV_WHEEL:
        if (!rect_contains(w->frame, e->x, e->y)) return false;
        /* Das Rad rührt die Auswahl nicht an - es verschiebt nur die Sicht.
         * Deshalb hier scroll_by und nicht list_goto. */
        scroll_by(&lw->sc, -e->wheel);
        return true;

    case EV_KEY_DOWN:
        if (!w->focused) return false;

        switch (e->key) {
        case KEY_UP:        list_goto(lw, lw->selected - 1); return true;
        case KEY_DOWN:      list_goto(lw, lw->selected + 1); return true;
        case KEY_PAGE_UP:   list_goto(lw, lw->selected - list_page(w)); return true;
        case KEY_PAGE_DOWN: list_goto(lw, lw->selected + list_page(w)); return true;
        case KEY_HOME:      list_goto(lw, 0); return true;
        case KEY_END:       list_goto(lw, lw->count - 1); return true;
        case KEY_RETURN:
            if (lw->selected >= 0) lw->opened = true;
            return true;
        default:
            return false;
        }

    default:
        return false;
    }
}

/* Gibt nur frei, was die Klasse selbst belegt hat - das Widget selbst gibt
 * widget_destroy frei (siehe widget.h). */
static void list_free_owned(list_widget *lw)
{
    if (!lw->owned) return;
    for (int i = 0; i < lw->owned_count; i++) free(lw->owned[i]);
    free(lw->owned);

    lw->owned       = NULL;
    lw->owned_count = 0;
    lw->keys        = NULL;
    lw->count       = 0;
}

static void list_destroy(widget *w)
{
    list_free_owned((list_widget *)w);
}

static const widget_class list_class = {
    .name    = "list",
    .measure = list_measure,
    .draw    = list_draw,
    .event   = list_event,
    .destroy = list_destroy,
};

/* --- Verwaltung -------------------------------------------------------------- */

widget *list_create(const theme *th, const catalog *cat)
{
    list_widget *lw = calloc(1, sizeof *lw);
    if (!lw) return NULL;

    lw->base.cls         = &list_class;
    lw->base.th          = th;
    lw->base.cat         = cat;
    lw->base.enabled     = true;
    lw->base.wants_focus = true;
    lw->selected         = -1;

    return &lw->base;
}

void list_set_items(widget *w, const char *const *keys, int count)
{
    list_widget *lw = (list_widget *)w;

    list_free_owned(lw);
    lw->keys     = keys;
    lw->count    = count;
    lw->selected = count > 0 ? 0 : -1;

    /* Zurück an den Anfang stellt list_ensure_visible: die Auswahl steht auf
     * dem ersten Eintrag, und sichtbar zu machen heißt hier, ganz nach oben zu
     * gehen. Es hier noch einmal zu setzen wäre eine zweite Stelle für
     * dieselbe Entscheidung. */
    list_ensure_visible(lw);
}

bool list_set_items_copy(widget *w, const char *const *keys, int count)
{
    list_widget *lw = (list_widget *)w;
    if (count < 0) count = 0;

    char **copy = count > 0 ? calloc((size_t)count, sizeof *copy) : NULL;
    if (count > 0 && !copy) return false;

    for (int i = 0; i < count; i++) {
        const char *src = keys[i] ? keys[i] : "";
        size_t      n   = strlen(src) + 1;

        copy[i] = malloc(n);
        if (!copy[i]) {
            for (int j = 0; j < i; j++) free(copy[j]);
            free(copy);
            return false;
        }
        memcpy(copy[i], src, n);
    }

    /* Erst jetzt das Alte wegwerfen: schlägt das Belegen fehl, steht die Liste
     * noch da, wie sie war. */
    list_free_owned(lw);

    lw->owned       = copy;
    lw->owned_count = count;
    lw->keys        = (const char *const *)copy;
    lw->count       = count;
    lw->selected    = count > 0 ? 0 : -1;
    list_ensure_visible(lw);
    return true;
}

int list_count(const widget *w)
{
    return ((const list_widget *)w)->count;
}

int list_selected(const widget *w)
{
    return ((const list_widget *)w)->selected;
}

void list_select(widget *w, int index)
{
    list_select_valid((list_widget *)w, index);
}

bool list_was_opened(widget *w)
{
    list_widget *lw = (list_widget *)w;
    bool         o  = lw->opened;
    lw->opened = false;
    return o;
}

int list_top(const widget *w)
{
    const list_widget *lw = (const list_widget *)w;
    list_sync(lw);
    return lw->sc.value;
}

scrollmodel *list_scroll(widget *w)
{
    list_widget *lw = (list_widget *)w;
    list_sync(lw);
    return &lw->sc;
}
