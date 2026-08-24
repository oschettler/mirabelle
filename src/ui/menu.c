/* Siehe menu.h für den Vertrag. Die Optik folgt window.c: Maße aus dem
 * theme, kein Text im Quelltext, Grundlinie statt Oberkante beim Zeichnen.
 */
#include "ui/menu.h"

#include <stdlib.h>

#include "gfx/pattern.h"
#include "gfx/text.h"

extern const font system12;

struct menubar {
    const menu    *menus;
    int            count;
    const catalog *cat;
    const keymap  *km;
    theme          th;   /* Kopie: die Leiste besitzt ihr Thema, damit kein
                          * Aufrufer es überleben muss - siehe wm_create */

    menu_enabled_fn enabled_fn;
    void           *enabled_user;

    bool open;         /* ist überhaupt ein Menü aufgeklappt */
    int  press_title;  /* Titel, auf dem die Maustaste gedrückt wurde, sonst -1 */
    int  open_index;   /* welches, Index in menus */
    int  highlight;    /* Index des hervorgehobenen Eintrags, -1 = keiner */
};

/* --- Geometrie --------------------------------------------------------------
 *
 * Die Breite eines Titels und die Breite eines Aufklappmenüs hängen nur vom
 * Inhalt ab, nicht von screen_w - deshalb brauchen diese Hilfsfunktionen die
 * Bildschirmbreite nicht.
 */

static bool item_is_selectable(const menubar *mb, const menu_item *it)
{
    if (!it->key) return false;   /* Trennlinie */
    if (mb->enabled_fn && !mb->enabled_fn(it->action, mb->enabled_user))
        return false;
    return true;
}

static int menu_title_width(const menubar *mb, int index)
{
    const char *text = T(mb->cat, mb->menus[index].key);
    return text_width(&system12, text) + 2 * mb->th.menu_pad;
}

static rect menu_title_rect(const menubar *mb, int index)
{
    /* Vor dem ersten Titel steht die Luft, wo im Original das Apfelmenü sitzt. */
    int x = mb->th.menubar_left;
    for (int i = 0; i < index; i++)
        x += menu_title_width(mb, i);
    return rect_make(x, 0, menu_title_width(mb, index), mb->th.menubar_h);
}

static int dropdown_width(const menubar *mb, int menu_index)
{
    const menu *m = &mb->menus[menu_index];
    int         max_text = 0, max_short = 0;
    char        shortcut[64];

    for (int i = 0; i < m->count; i++) {
        const menu_item *it = &m->items[i];
        if (!it->key) continue;

        int tw = text_width(&system12, T(mb->cat, it->key));
        if (tw > max_text) max_text = tw;

        if (it->action && keymap_describe(mb->km, it->action, shortcut, sizeof shortcut)) {
            int sw = text_width(&system12, shortcut);
            if (sw > max_short) max_short = sw;
        }
    }

    return mb->th.menu_text_pad + max_text + mb->th.menu_gap + max_short +
           mb->th.menu_pad;
}

static rect dropdown_rect(const menubar *mb)
{
    rect title = menu_title_rect(mb, mb->open_index);
    int  w     = dropdown_width(mb, mb->open_index);
    int  h     = mb->menus[mb->open_index].count * mb->th.menu_item_h;
    return rect_make(title.x, mb->th.menubar_h, w, h);
}

static int hit_title(const menubar *mb, int x, int y)
{
    for (int i = 0; i < mb->count; i++)
        if (rect_contains(menu_title_rect(mb, i), x, y)) return i;
    return -1;
}

static int hit_item(const menubar *mb, int x, int y)
{
    rect dd = dropdown_rect(mb);
    if (!rect_contains(dd, x, y)) return -1;

    int index = (y - dd.y) / mb->th.menu_item_h;
    if (index < 0 || index >= mb->menus[mb->open_index].count) return -1;
    return index;
}

static bool in_bar(const menubar *mb, int screen_w, int x, int y)
{
    return rect_contains(rect_make(0, 0, screen_w, mb->th.menubar_h), x, y);
}

/* --- Hervorhebung mit der Tastatur ------------------------------------------- */

static int first_selectable(const menubar *mb, int menu_index)
{
    const menu *m = &mb->menus[menu_index];
    for (int i = 0; i < m->count; i++)
        if (item_is_selectable(mb, &m->items[i])) return i;
    return -1;
}

/* Läuft höchstens count Schritte weit - so bleibt ein Menü ohne wählbaren
 * Eintrag ohne Endlosschleife einfach unverändert. */
static void move_highlight(menubar *mb, int dir)
{
    const menu *m   = &mb->menus[mb->open_index];
    int         idx = mb->highlight;

    for (int steps = 0; steps < m->count; steps++) {
        idx += dir;
        if (idx < 0) idx = m->count - 1;
        else if (idx >= m->count) idx = 0;

        if (item_is_selectable(mb, &m->items[idx])) {
            mb->highlight = idx;
            return;
        }
    }
}

static const menu_item *current_item(const menubar *mb)
{
    if (!mb->open || mb->highlight < 0) return NULL;
    const menu *m = &mb->menus[mb->open_index];
    if (mb->highlight >= m->count) return NULL;
    return &m->items[mb->highlight];
}

static void handle_key(menubar *mb, int key, const char **action)
{
    switch (key) {
    case KEY_LEFT:
        mb->open_index = (mb->open_index - 1 + mb->count) % mb->count;
        mb->highlight  = first_selectable(mb, mb->open_index);
        break;
    case KEY_RIGHT:
        mb->open_index = (mb->open_index + 1) % mb->count;
        mb->highlight  = first_selectable(mb, mb->open_index);
        break;
    case KEY_UP:
        move_highlight(mb, -1);
        break;
    case KEY_DOWN:
        move_highlight(mb, 1);
        break;
    case KEY_RETURN: {
        const menu_item *it = current_item(mb);
        if (it) {
            *action       = it->action;
            mb->open      = false;
            mb->highlight = -1;
        }
        break;
    }
    case KEY_ESCAPE:
        mb->open      = false;
        mb->highlight = -1;
        break;
    default:
        break;
    }
}

/* --- Verwaltung --------------------------------------------------------------- */

menubar *menubar_create(const menu *menus, int count, const catalog *cat,
                        const keymap *km, const theme *th)
{
    menubar *mb = calloc(1, sizeof *mb);
    if (!mb) return NULL;

    mb->menus      = menus;
    mb->count      = count;
    mb->cat        = cat;
    mb->km         = km;
    mb->th          = *th;
    mb->open        = false;
    mb->open_index  = 0;
    mb->press_title = -1;
    mb->highlight  = -1;

    return mb;
}

void menubar_free(menubar *mb)
{
    free(mb);
}

void menubar_set_enabled_fn(menubar *mb, menu_enabled_fn fn, void *user)
{
    mb->enabled_fn   = fn;
    mb->enabled_user = user;
}

int menubar_height(const menubar *mb)
{
    return mb->th.menubar_h;
}

bool menubar_is_open(const menubar *mb)
{
    return mb->open;
}

void menubar_enter(menubar *mb)
{
    mb->open       = true;
    mb->open_index = 0;
    mb->highlight  = first_selectable(mb, 0);
}

void menubar_dismiss(menubar *mb)
{
    mb->open      = false;
    mb->highlight = -1;
}

/* --- Zeichnen ------------------------------------------------------------------ */

static void draw_dropdown(const menubar *mb, gc *g)
{
    const theme *th = &mb->th;
    const menu  *m  = &mb->menus[mb->open_index];
    rect         dd = dropdown_rect(mb);

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    /* Schlagschatten unten und rechts, zwei Pixel, wie in System 1. Er wird
     * VOR dem Menü gezeichnet, damit das weiße Feld ihn nicht überdeckt. */
    if (th->menu_shadow > 0) {
        int sh = th->menu_shadow;
        g->pat = PAT_BLACK;
        gfx_fill_rect(g, rect_make(dd.x + sh, dd.y + dd.h, dd.w, sh));
        gfx_fill_rect(g, rect_make(dd.x + dd.w, dd.y + sh, sh, dd.h));
    }

    g->pat = PAT_WHITE;
    gfx_fill_rect(g, dd);
    g->pat = PAT_BLACK;
    gfx_frame_rect(g, dd);

    int  baseline_offset = (th->menu_item_h - system12.size) / 2 + system12.ascent;
    char shortcut[64];

    for (int i = 0; i < m->count; i++) {
        const menu_item *it  = &m->items[i];
        rect             row = rect_make(dd.x, dd.y + i * th->menu_item_h, dd.w, th->menu_item_h);

        if (!it->key) {
            g->pat = PAT_BLACK;
            gfx_hline(g, row.x, row.y + row.h / 2, row.w);
            continue;
        }

        int ty = row.y + baseline_offset;
        g->pat = PAT_BLACK;
        gfx_text(g, &system12, row.x + th->menu_text_pad, ty, T(mb->cat, it->key));

        if (it->action && keymap_describe(mb->km, it->action, shortcut, sizeof shortcut)) {
            int sw = text_width(&system12, shortcut);
            gfx_text(g, &system12, row.x + row.w - th->menu_pad - sw, ty, shortcut);
        }

        if (!item_is_selectable(mb, it)) {
            g->pat  = PAT_GRAY50;
            g->mode = GFX_CLEAR;
            gfx_fill_rect(g, row);
            g->mode = GFX_COPY;
        }

        if (i == mb->highlight)
            gfx_invert_rect(g, row);
    }
}

void menubar_draw(menubar *mb, gc *g, int screen_w)
{
    const theme *th = &mb->th;

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_rect(g, rect_make(0, 0, screen_w, th->menubar_h));
    g->pat = PAT_BLACK;
    gfx_hline(g, 0, th->menubar_h - 1, screen_w);

    int baseline = (th->menubar_h - system12.size) / 2 + system12.ascent;

    for (int i = 0; i < mb->count; i++) {
        rect        tr   = menu_title_rect(mb, i);
        const char *text = T(mb->cat, mb->menus[i].key);

        g->pat = PAT_BLACK;
        gfx_text(g, &system12, tr.x + th->menu_pad, baseline, text);

        if (mb->open && i == mb->open_index)
            gfx_invert_rect(g, tr);
    }

    if (mb->open)
        draw_dropdown(mb, g);
}

/* --- Ereignisse ------------------------------------------------------------------ */

bool menubar_event(menubar *mb, const event *e, int screen_w, const char **action)
{
    *action = NULL;

    switch (e->kind) {
    case EV_MOUSE_DOWN: {
        int ti = hit_title(mb, e->x, e->y);
        if (ti >= 0) {
            /* Auf denselben Titel noch einmal: zuklappen. Sonst wäre ein
             * offengehaltenes Menü nur über einen Klick daneben loszuwerden. */
            if (mb->open && mb->open_index == ti) {
                mb->open        = false;
                mb->highlight   = -1;
                mb->press_title = -1;
                return true;
            }
            mb->open        = true;
            mb->open_index  = ti;
            mb->highlight   = -1;
            mb->press_title = ti;
            return true;
        }
        mb->press_title = -1;

        if (mb->open) {
            /* Im aufgeklappten Menü gedrückt: das ist der Beginn einer
             * Auswahl, nicht das Zuklappen. Ausgelöst wird beim Loslassen. */
            int ii = hit_item(mb, e->x, e->y);
            if (ii >= 0) {
                const menu_item *it = &mb->menus[mb->open_index].items[ii];
                mb->highlight = item_is_selectable(mb, it) ? ii : -1;
                return true;
            }
            mb->open      = false;
            mb->highlight = -1;
            return true;
        }
        return in_bar(mb, screen_w, e->x, e->y);
    }

    case EV_MOUSE_MOVE: {
        if (!mb->open) return in_bar(mb, screen_w, e->x, e->y);

        int ti = hit_title(mb, e->x, e->y);
        if (ti >= 0 && ti != mb->open_index) {
            mb->open_index = ti;
            mb->highlight  = -1;   /* wie ein frischer Klick auf den Titel */
            return true;
        }

        int ii = hit_item(mb, e->x, e->y);
        if (ii >= 0 && item_is_selectable(mb, &mb->menus[mb->open_index].items[ii]))
            mb->highlight = ii;
        else
            mb->highlight = -1;
        return true;
    }

    case EV_MOUSE_UP: {
        int press = mb->press_title;
        mb->press_title = -1;

        if (!mb->open) return in_bar(mb, screen_w, e->x, e->y);

        /* Kurzer Klick auf den Titel: das Menü bleibt offen, und der Nutzer
         * kann in Ruhe hineinfahren. Nur wer aus dem Titel heraus in das Menü
         * gezogen hat, löst beim Loslassen aus - das ist die alte
         * System-1-Geste, und beide sollen nebeneinander funktionieren.
         *
         * Unterschieden wird an der Stelle, nicht an der Zeit: wurde auf einem
         * Titel gedrückt und auf demselben Titel wieder losgelassen, ist der
         * Zeiger nie im Menü gewesen. */
        if (press >= 0 && hit_title(mb, e->x, e->y) == press)
            return true;

        const menu_item *it = current_item(mb);
        if (it) *action = it->action;
        mb->open      = false;
        mb->highlight = -1;
        return true;
    }

    case EV_KEY_DOWN:
        if (!mb->open) return false;
        handle_key(mb, e->key, action);
        return true;

    default:
        return false;
    }
}
