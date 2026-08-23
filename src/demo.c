/* Die Vorführung aus M4: Zustand, Ereignisse, Zeichnen.
 *
 * Ab M6 übernimmt die Fensterverwaltung, was hier noch von Hand gezeichnet
 * wird. Bis dahin zeigt dieses Programm, dass die Schichten zusammenspielen:
 * Plattform, Grafik, Zeichensatz und die Ereignisse.
 *
 * Der getippte Text wird angezeigt, und das ist Absicht. Wer hier auf einer
 * deutschen Tastatur ä eingibt, über AltGr ein € oder über die Tottaste ´
 * gefolgt von a ein á, sieht sofort, ob Entscheidung D-2 trägt.
 */

#include <string.h>

#include "core/utf8.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/pattern.h"
#include "gfx/text.h"
#include "plat/plat.h"

#include "demo.h"

extern const font system12;

#define TITLEBAR_H 20
#define CLOSE_BOX  11

/* Rahmen im Stil von System 1: gestreifte Titelleiste, Schließfeld links,
 * einfache Linie außen. */
static void draw_window_frame(gc *g, rect r, const char *title, bool active)
{
    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_rect(g, r);

    g->pat = PAT_BLACK;
    gfx_frame_rect(g, r);
    gfx_hline(g, r.x, r.y + TITLEBAR_H, r.w);

    if (active) {
        /* Streifen: jede zweite Zeile der Titelleiste. */
        for (int y = r.y + 3; y < r.y + TITLEBAR_H - 3; y += 2)
            gfx_hline(g, r.x + 1, y, r.w - 2);

        rect box = rect_make(r.x + 6, r.y + (TITLEBAR_H - CLOSE_BOX) / 2,
                             CLOSE_BOX, CLOSE_BOX);
        g->pat = PAT_WHITE;
        gfx_fill_rect(g, box);
        g->pat = PAT_BLACK;
        gfx_frame_rect(g, box);
    }

    int tw = text_width(&system12, title);
    int tx = r.x + (r.w - tw) / 2;
    int ty = r.y + (TITLEBAR_H - system12.size) / 2 + system12.ascent;

    /* Der Titel bekommt Luft, damit die Streifen ihn nicht auffressen. */
    g->pat = PAT_WHITE;
    gfx_fill_rect(g, rect_make(tx - 5, r.y + 1, tw + 10, TITLEBAR_H - 2));
    g->pat = PAT_BLACK;
    gfx_text(g, &system12, tx, ty, title);
}

void demo_draw(const demo_state *st, gc *g, int w, int h)
{
    const char *typed = st->typed;

    g->pat  = PAT_GRAY50;
    g->mode = GFX_COPY;
    gfx_clear(g);

    rect win = rect_make(w / 2 - 190, h / 2 - 90, 380, 180);
    draw_window_frame(g, win, "Schreibtisch", true);

    g->pat = PAT_BLACK;
    int y = win.y + TITLEBAR_H + 10 + system12.ascent;

    gfx_text(g, &system12, win.x + 12, y, "Grüße aus Köln, Fräulein Müller!");
    y += 18;
    gfx_text(g, &system12, win.x + 12, y, "Tippen Sie etwas. Esc beendet.");
    y += 22;

    gfx_hline(g, win.x + 12, y - system12.ascent - 4, win.w - 24);
    gfx_text(g, &system12, win.x + 12, y, typed);

    /* Schreibmarke, mit XOR gezeichnet - derselbe Aufruf löscht sie wieder. */
    g->mode = GFX_XOR;
    gfx_fill_rect(g, rect_make(win.x + 12 + text_width(&system12, typed),
                               y - system12.ascent, 1, system12.size));
    g->mode = GFX_COPY;
}

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


void demo_init(demo_state *st)
{
    st->typed[0] = '\0';
    st->running  = true;
}

void demo_event(demo_state *st, const event *e)
{
    switch (e->kind) {
    case EV_QUIT:
        st->running = false;
        break;
    case EV_KEY_DOWN:
        if (e->key == KEY_ESCAPE)         st->running = false;
        else if (e->key == KEY_BACKSPACE) backspace_codepoint(st->typed);
        break;
    case EV_TEXT:
        append_text(st->typed, sizeof st->typed - 1, e->text);
        break;
    default:
        break;
    }
}
