/* Die Vorführung aus M4 bis M6.
 *
 * Sie öffnet drei Fenster und füllt deren Inhalt; Rahmen, z-Ordnung,
 * Aktivierung und Ziehen macht die Fensterverwaltung. Was hier bleibt, ist
 * das, was eine Anwendung wirklich tut: Inhalt zeichnen und auf Ereignisse
 * reagieren, die die Verwaltung nicht selbst verbraucht hat.
 *
 * Der getippte Text wird angezeigt, und das ist Absicht. Wer auf einer
 * deutschen Tastatur ä eingibt, über AltGr ein € oder über die Tottaste ´
 * gefolgt von a ein á, sieht sofort, ob Entscheidung D-2 trägt.
 */

#include "demo.h"

#include <stdio.h>
#include <string.h>

#include "core/utf8.h"
#include "gfx/pattern.h"
#include "gfx/text.h"

extern const font system12;

/* Hängt einen Codepunkt als UTF-8 an, solange Platz ist. */
static void append_text(char *buf, size_t cap, const char *utf8)
{
    size_t len = strlen(buf), add = strlen(utf8);
    if (len + add + 1 <= cap) memcpy(buf + len, utf8, add + 1);
}

/* Entfernt den letzten Codepunkt, nicht das letzte Byte. */
static void backspace_codepoint(char *buf)
{
    const char *end = buf + strlen(buf);
    const char *p   = end;

    if (p == buf) return;
    utf8_prev(buf, &p);
    *(char *)p = '\0';
}

bool demo_init(demo_state *st, const keymap *km, const catalog *cat,
               const theme *th, int screen_w, int screen_h)
{
    static theme fallback;
    if (!th) {
        theme_defaults(&fallback);
        th = &fallback;
    }

    memset(st, 0, sizeof *st);
    st->km      = km;
    st->cat     = cat;
    st->running = true;

    st->m = wm_create(th, screen_w, screen_h);
    if (!st->m) return false;

    st->w_keys = wm_open(st->m, rect_make(screen_w / 2 - 210, screen_h / 2 - 130,
                                          240, 150),
                         T(cat, "window.keys"), WIN_NORMAL);
    st->w_desk = wm_open(st->m, rect_make(screen_w / 2 - 60, screen_h / 2 - 40,
                                          300, 170),
                         T(cat, "window.desk"), WIN_NORMAL);

    if (!st->w_keys || !st->w_desk) {
        demo_free(st);
        return false;
    }
    return true;
}

void demo_free(demo_state *st)
{
    if (st->m) wm_destroy(st->m);
    st->m      = NULL;
    st->w_keys = NULL;
    st->w_desk = NULL;
}

void demo_event(demo_state *st, const event *e)
{
    if (e->kind == EV_QUIT) {
        st->running = false;
        return;
    }

    /* Erst die Fensterverwaltung. Was sie verbraucht hat - Aktivieren,
     * Verschieben, Vergrößern, Schließen - sieht die Anwendung nie. */
    if (st->m && wm_event(st->m, e)) {
        if (e->kind == EV_MOUSE_DOWN) {
            st->click_x     = e->x;
            st->click_y     = e->y;
            st->click_count = e->clicks;
        }
        return;
    }

    switch (e->kind) {
    case EV_MOUSE_DOWN:
        st->click_x     = e->x;
        st->click_y     = e->y;
        st->click_count = e->clicks;
        break;

    case EV_KEY_DOWN: {
        /* Erst die Belegung fragen. Nur was dort nicht steht, wird hier von
         * Hand behandelt - und das ist bewusst wenig. */
        const char *action = st->km
            ? keymap_lookup(st->km, e->key, e->mods, "app") : NULL;

        if (action) {
            snprintf(st->last_action, sizeof st->last_action, "%s", action);
            if (strcmp(action, "app.quit") == 0) st->running = false;
            break;
        }

        if (e->key == KEY_ESCAPE)         st->running = false;
        else if (e->key == KEY_BACKSPACE) backspace_codepoint(st->typed);
        break;
    }

    case EV_TEXT:
        append_text(st->typed, sizeof st->typed - 1, e->text);
        break;

    default:
        break;
    }
}

/* Zeigt die Kürzel aus der Belegung. Sie kommen aus derselben Datei wie die
 * Auswertung; was hier steht, kann deshalb gar nicht von dem abweichen, was
 * die Taste tut. */
static void draw_keys_window(const demo_state *st, window *w)
{
    static const char *shown[] = { "record.new", "record.save", "search.open",
                                   "edit.undo", "edit.redo", "app.quit" };
    gc g;
    window_gc(w, &g);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);
    g.pat = PAT_BLACK;

    int y = 4 + system12.ascent;
    for (size_t i = 0; i < sizeof shown / sizeof shown[0]; i++) {
        char line[96], keys[32];
        if (st->km && keymap_describe(st->km, shown[i], keys, sizeof keys))
            snprintf(line, sizeof line, "%-13s %s", shown[i], keys);
        else
            snprintf(line, sizeof line, "%-13s -", shown[i]);
        gfx_text(&g, &system12, 8, y, line);
        y += 13;
    }
}

static void draw_desk_window(const demo_state *st, window *w)
{
    gc g;
    window_gc(w, &g);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);
    g.pat = PAT_BLACK;

    rect r = { 0, 0, 0, 0 };
    r.w = window_content_rect(w).w;

    int y = 4 + system12.ascent;
    gfx_text(&g, &system12, 8, y, T(st->cat, "demo.greeting"));
    y += 16;

    char info[128];
    const char *action = st->last_action[0]
        ? st->last_action : T(st->cat, "demo.action.none");
    const char *one[] = { action };
    if (Tf(st->cat, "demo.action", info, sizeof info, one, 1))
        gfx_text(&g, &system12, 8, y, info);
    y += 13;

    char bx[16], by[16], bn[16];
    snprintf(bx, sizeof bx, "%d", st->click_x);
    snprintf(by, sizeof by, "%d", st->click_y);
    snprintf(bn, sizeof bn, "%d", st->click_count);
    const char *three[] = { bx, by, bn };
    if (Tf(st->cat, "demo.click", info, sizeof info, three, 3))
        gfx_text(&g, &system12, 8, y, info);
    y += 13;

    gfx_text(&g, &system12, 8, y, T(st->cat, "demo.hint"));
    y += 20;

    gfx_hline(&g, 8, y - system12.ascent - 4, r.w - 16);
    gfx_text(&g, &system12, 8, y, st->typed);

    /* Schreibmarke, mit XOR gezeichnet - derselbe Aufruf löscht sie wieder. */
    g.mode = GFX_XOR;
    gfx_fill_rect(&g, rect_make(8 + text_width(&system12, st->typed),
                                y - system12.ascent, 1, system12.size));
    g.mode = GFX_COPY;
}

void demo_draw(demo_state *st, gc *g)
{
    if (!st->m) return;

    draw_keys_window(st, st->w_keys);
    draw_desk_window(st, st->w_desk);
    wm_draw(st->m, g);
}
